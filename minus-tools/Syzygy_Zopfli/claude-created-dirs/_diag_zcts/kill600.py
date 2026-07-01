#!/usr/bin/env python3
"""Mutation kill harness for ZopfliCacheToSublen in zopfli.c.

Mirrors the avocado run_cbmc pipeline (goto-cc; --partial-loops --unwind 5;
--replace-call-with-contract ZopfliMaxCachedSublen --enforce-contract;
cbmc --depth 200).
"""
import re
import subprocess
import sys
import os

CFILE = "/app/Syzygy_Zopfli/c_code/zopfli.c"
FUNCTION = "ZopfliCacheToSublen"
CALLEES = ["ZopfliMaxCachedSublen"]
MUTANTS_FILE = sys.argv[1] if len(sys.argv) > 1 else "/app/mutants_zcts.txt"
WORKDIR = "/app/_kill_work_zcts600"


def parse_mutants(path):
    text = open(path).read()
    blocks = re.split(r'(?=^--- original$)', text, flags=re.M)
    muts = []
    for b in blocks:
        m = re.search(r'^@@ -(\d+) \+\d+ @@$', b, flags=re.M)
        if not m:
            continue
        lineno = int(m.group(1))
        lines = b.splitlines()
        idx = next(i for i, l in enumerate(lines) if l.startswith('@@'))
        old = new = None
        for l in lines[idx+1:]:
            if l.startswith('-'):
                old = l[1:]
            elif l.startswith('+'):
                new = l[1:]
        if old is not None and new is not None:
            muts.append((lineno, old, new))
    return muts


def run_cbmc(cfile, tag):
    goto = f"{WORKDIR}/{FUNCTION}_{tag}.goto"
    chk = f"{WORKDIR}/checking_{FUNCTION}_{tag}.goto"
    try:
        subprocess.run(["goto-cc", "-o", goto, cfile, "-I",
                        "/app/Syzygy_Zopfli/c_code", "--function", FUNCTION],
                       check=True, capture_output=True, text=True)
        subprocess.run(["goto-instrument", "--partial-loops", "--unwind", "5", goto, goto],
                       check=True, capture_output=True, text=True)
        cmd = ["goto-instrument"]
        for c in CALLEES:
            cmd += ["--replace-call-with-contract", c]
        cmd += ["--enforce-contract", FUNCTION, goto, chk]
        subprocess.run(cmd, check=True, capture_output=True, text=True)
        r = subprocess.run(["cbmc", chk, "--function", FUNCTION, "--depth", "600"],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
    except subprocess.CalledProcessError as e:
        return "BUILDERR", (e.stdout or "") + (e.stderr or "")
    if "VERIFICATION SUCCESSFUL" in out:
        return "SUCCESSFUL", out
    if "VERIFICATION FAILED" in out:
        return "FAILED", out
    return "UNKNOWN", out


def main():
    os.makedirs(WORKDIR, exist_ok=True)
    orig_lines = open(CFILE).read().split("\n")
    muts = parse_mutants(MUTANTS_FILE)
    print(f"Parsed {len(muts)} mutants")

    killed = survived = 0
    survivors = []
    for i, (lineno, old, new) in enumerate(muts):
        actual = orig_lines[lineno-1]
        if actual.rstrip() != old.rstrip():
            print(f"[{i}] line {lineno}: MISMATCH expected {old!r} got {actual!r}")
            continue
        mut_lines = list(orig_lines)
        mut_lines[lineno-1] = new
        mutfile = "/app/Syzygy_Zopfli/c_code/zopfli_mut_zcts.c"
        open(mutfile, "w").write("\n".join(mut_lines))
        verdict, _ = run_cbmc(mutfile, str(i))
        status = "KILLED" if verdict == "FAILED" else ("SURVIVED" if verdict == "SUCCESSFUL" else verdict)
        if verdict == "FAILED":
            killed += 1
        elif verdict == "SUCCESSFUL":
            survived += 1
            survivors.append((lineno, new))
        print(f"[{i}] line {lineno}: {status}  | {new.strip()}")

    print(f"\nKILLED {killed} / {killed+survived}  (SURVIVED {survived})")
    print("\nSURVIVORS:")
    for lineno, new in survivors:
        print(f"  line {lineno}: {new.strip()}")


if __name__ == "__main__":
    main()
