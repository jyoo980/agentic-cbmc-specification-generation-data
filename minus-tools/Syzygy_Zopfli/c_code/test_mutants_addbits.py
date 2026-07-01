#!/usr/bin/env python3
"""Apply each AddBits mutant, run the CBMC pipeline, report killed/survived."""
import subprocess, os

SRC = "zopfli.c"
FUNC = "AddBits"

MUTANTS = [
    ("if-flip",   "\n        if (*bp == 0)", "\n        if (*bp != 0)"),
    ("le",        "\n    for (i = 0; i < length; i++)", "\n    for (i = 0; i <= length; i++)"),
    ("gt",        "\n    for (i = 0; i < length; i++)", "\n    for (i = 0; i > length; i++)"),
    ("ge",        "\n    for (i = 0; i < length; i++)", "\n    for (i = 0; i >= length; i++)"),
    ("eq",        "\n    for (i = 0; i < length; i++)", "\n    for (i = 0; i == length; i++)"),
    ("ne",        "\n    for (i = 0; i < length; i++)", "\n    for (i = 0; i != length; i++)"),
    ("dec",       "\n        *bp = (*bp + 1) & 7;", "\n        *bp = (*bp - 1) & 7;"),
    ("oob",       "\n        (*out)[*outsize - 1] |= bit << *bp;",
                  "\n        (*out)[*outsize + 1] |= bit << *bp;"),
]

def run_pipeline(src):
    g = FUNC + ".goto"
    c = "checking-%s-contracts.goto" % FUNC
    for cmd in (
        ["goto-cc", "-o", g, src, "--function", FUNC],
        ["goto-instrument", "--partial-loops", "--unwind", "5", g, g],
        ["goto-instrument", "--enforce-contract", FUNC, g, c],
        ["cbmc", c, "--function", FUNC, "--depth", "200"],
    ):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if cmd[0] == "cbmc":
            return r.returncode, r.stdout
        if r.returncode != 0:
            return -1, r.stdout + r.stderr
    return None, ""

orig = open(SRC).read()
# Scope mutation to the AddBits function body only (patterns are shared with
# AddHuffmanBits / AddBit otherwise).
start = orig.index("static void AddBits(")
end = orig.index("ZopfliLengthsToSymbols", start + 1)
body = orig[start:end]
killed = survived = 0
for name, o, m in MUTANTS:
    assert body.count(o) == 1, "pattern not unique for %s: %s" % (name, o)
    mutated = orig[:start] + body.replace(o, m) + orig[end:]
    tmp = "mutant_%s.c" % name
    open(tmp, "w").write(mutated)
    rc, out = run_pipeline(tmp)
    if rc == -1:
        verdict = "BUILD-FAIL (excluded)"
    elif rc == 0:
        verdict = "SURVIVED (cbmc passed)"
        survived += 1
    else:
        verdict = "KILLED (cbmc failed)"
        killed += 1
    print("%-10s %s" % (name, verdict))
    os.remove(tmp)
print("---- killed=%d survived=%d" % (killed, survived))
