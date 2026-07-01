#!/usr/bin/env python3
"""Measure CBMC mutation kill score for AddLZ77Data.

For each mutant from get-mutants, apply the single-line change to
zopfli.c, rebuild + instrument + run CBMC, and record whether the mutant is
killed (CBMC verdict flips from SUCCESS to FAILED) or survives.
The original file is always restored.
"""
import subprocess, re, sys, shutil, os

FILE = "zopfli.c"
FUNC = "AddLZ77Data"
CALLEES = [
    "AddHuffmanBits", "AddBits", "ZopfliGetLengthSymbol", "ZopfliGetDistSymbol",
    "ZopfliGetLengthExtraBitsValue", "ZopfliGetLengthExtraBits",
    "ZopfliGetDistExtraBitsValue", "ZopfliGetDistExtraBits",
]

def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNC, "--file", FILE],
        capture_output=True, text=True).stdout
    mutants = []
    blocks = out.split("--- original")
    for b in blocks:
        minus = plus = None
        for ln in b.splitlines():
            if ln.startswith("+++") or ln.startswith("---") or ln.startswith("@@"):
                continue
            if ln.startswith("-") and minus is None:
                minus = ln[1:]
            elif ln.startswith("+") and plus is None:
                plus = ln[1:]
        if minus is not None and plus is not None:
            mutants.append((minus, plus))
    return mutants

def run_cbmc():
    g = f"{FUNC}.goto"
    c = f"checking-{FUNC}-contracts.goto"
    if subprocess.run(["goto-cc", "-o", g, FILE, "--function", FUNC],
                      capture_output=True, text=True).returncode != 0:
        return "BUILDERR"
    subprocess.run(["goto-instrument", "--partial-loops", "--unwind", "5", g, g],
                   capture_output=True, text=True)
    cmd = ["goto-instrument"]
    for cal in CALLEES:
        cmd += ["--replace-call-with-contract", cal]
    cmd += ["--enforce-contract", FUNC, g, c]
    subprocess.run(cmd, capture_output=True, text=True)
    r = subprocess.run(["cbmc", c, "--function", FUNC, "--depth", "1500"],
                       capture_output=True, text=True)
    if "VERIFICATION SUCCESSFUL" in r.stdout:
        return "SUCCESS"
    if "VERIFICATION FAILED" in r.stdout:
        return "FAILED"
    return "UNKNOWN"

def main():
    mutants = get_mutants()
    print(f"{len(mutants)} mutants")
    orig = open(FILE).read()
    base = run_cbmc()
    print(f"baseline (original): {base}")
    assert base == "SUCCESS", "original must verify"
    killed = survived = 0
    results = []
    try:
        for i, (m, p) in enumerate(mutants):
            if m not in orig:
                results.append((i, "NOTFOUND", m.strip()))
                continue
            if orig.count(m) != 1:
                results.append((i, f"AMBIG({orig.count(m)})", m.strip()))
                continue
            open(FILE, "w").write(orig.replace(m, p, 1))
            v = run_cbmc()
            open(FILE, "w").write(orig)
            if v == "FAILED":
                killed += 1; tag = "KILLED"
            elif v == "SUCCESS":
                survived += 1; tag = "SURVIVED"
            else:
                tag = v
            results.append((i, tag, p.strip()))
            print(f"[{i:2}] {tag:9} {p.strip()}")
    finally:
        open(FILE, "w").write(orig)
    print(f"\nKILLED {killed} / {len(mutants)}  (survived {survived})")

if __name__ == "__main__":
    main()
