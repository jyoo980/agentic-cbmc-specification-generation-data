#!/usr/bin/env python3
"""Mutation-kill harness for EncodeTree in zopfli.c.

For each mutant produced by get-mutants, apply it to a copy of the
source, run the CBMC verification pipeline, and record whether the mutant is
killed (CBMC fails) or survives (CBMC succeeds).
"""
import os
import sys
# Remove this script's own directory from sys.path so local files (e.g.
# bisect.py) do not shadow stdlib modules.
_self_dir = os.path.dirname(os.path.abspath(__file__))
sys.path = [p for p in sys.path if os.path.abspath(p or ".") != _self_dir]
import re
import subprocess
import tempfile

SRC = "zopfli.c"
FUNCTION = "EncodeTree"
REPLACE = ["ZopfliCalculateBitLengths", "ZopfliLengthsToSymbols"]


def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNCTION, "--file", SRC],
        capture_output=True, text=True, check=True).stdout
    return out


def parse_mutants(text):
    muts = []
    blocks = text.split("--- original")
    for b in blocks[1:]:
        m = re.search(r"@@ -(\d+) \+\d+ @@", b)
        if not m:
            continue
        line = int(m.group(1))
        lines = b.splitlines()
        old = new = None
        for ln in lines:
            if ln.startswith("-") and not ln.startswith("---"):
                old = ln[1:]
            elif ln.startswith("+") and not ln.startswith("+++"):
                new = ln[1:]
        muts.append((line, old, new))
    return muts


def run_cbmc(src_path, workdir, tag):
    goto = os.path.join(workdir, f"{tag}.goto")
    chk = os.path.join(workdir, f"chk_{tag}.goto")
    r = subprocess.run(
        ["goto-cc", "-o", goto, src_path, "--function", FUNCTION],
        capture_output=True, text=True)
    if r.returncode != 0:
        return "compile_error", r.stderr
    subprocess.run(["goto-instrument", "--partial-loops", "--unwind", "5", goto, goto],
                   capture_output=True, text=True)
    inst = ["goto-instrument"]
    for c in REPLACE:
        inst += ["--replace-call-with-contract", c]
    inst += ["--enforce-contract", FUNCTION, goto, chk]
    subprocess.run(inst, capture_output=True, text=True)
    r = subprocess.run(["cbmc", chk, "--function", FUNCTION, "--depth", "200"],
                       capture_output=True, text=True)
    return ("survived" if "VERIFICATION SUCCESSFUL" in r.stdout else "killed"), r.stdout[-500:]


def main():
    text = get_mutants()
    muts = parse_mutants(text)
    with open(SRC) as f:
        orig_lines = f.readlines()

    from concurrent.futures import ThreadPoolExecutor

    wd = _self_dir  # compile in c_code so zopfli.h resolves

    def do_one(args):
        idx, (line, old, new) = args
        mut_lines = list(orig_lines)
        mut_lines[line - 1] = new + "\n"  # line is 1-based
        mpath = os.path.join(wd, f"_mut_zopfli_{idx}.c")
        with open(mpath, "w") as f:
            f.writelines(mut_lines)
        try:
            verdict, detail = run_cbmc(mpath, wd, f"m{idx}")
        finally:
            for f in (mpath, os.path.join(wd, f"m{idx}.goto"),
                      os.path.join(wd, f"chk_m{idx}.goto")):
                if os.path.exists(f):
                    os.remove(f)
        return (idx, line, new.strip(), verdict)

    results = []
    with ThreadPoolExecutor(max_workers=8) as ex:
        for idx, line, new, verdict in ex.map(do_one, list(enumerate(muts))):
            results.append((line, new, verdict))
            print(f"[{idx+1}/{len(muts)}] line {line}: {verdict}  | {new[:60]}", flush=True)

    killed = sum(1 for *_, v in results if v == "killed")
    survived = [(l, n) for l, n, v in results if v == "survived"]
    print(f"\nKILLED {killed}/{len(results)}")
    print("SURVIVORS:")
    for l, n in survived:
        print(f"  line {l}: {n}")


if __name__ == "__main__":
    main()
