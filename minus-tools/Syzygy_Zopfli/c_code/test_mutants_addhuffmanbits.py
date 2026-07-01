#!/usr/bin/env python3
"""Apply each AddHuffmanBits mutant (within the function body only) and run
verify.sh; report KILLED (VERIFICATION FAILED) vs SURVIVED (SUCCESSFUL)."""
import subprocess, sys, shutil

SRC = "zopfli.c"
FUNC_START = "static void AddHuffmanBits("
FUNC_END_AFTER = "static void AddBits("

MUTANTS = {
    "if-flip":            ("if (*bp == 0)", "if (*bp != 0)"),
    "le":                 ("for (i = 0; i < length; i++)", "for (i = 0; i <= length; i++)"),
    "gt":                 ("for (i = 0; i < length; i++)", "for (i = 0; i > length; i++)"),
    "ge":                 ("for (i = 0; i < length; i++)", "for (i = 0; i >= length; i++)"),
    "eq":                 ("for (i = 0; i < length; i++)", "for (i = 0; i == length; i++)"),
    "ne":                 ("for (i = 0; i < length; i++)", "for (i = 0; i != length; i++)"),
    "dec":                ("*bp = (*bp + 1) & 7;", "*bp = (*bp - 1) & 7;"),
    "oob":                ("(*out)[*outsize - 1] |= bit << *bp;", "(*out)[*outsize + 1] |= bit << *bp;"),
    "shift-plus1":        ("unsigned bit = (symbol >> (length - i - 1)) & 1;",
                           "unsigned bit = (symbol >> (length - i + 1)) & 1;"),
    "shift-plus-i":       ("unsigned bit = (symbol >> (length - i - 1)) & 1;",
                           "unsigned bit = (symbol >> (length + i - 1)) & 1;"),
}

orig = open(SRC).read()
s = orig.index(FUNC_START)
e = orig.index(FUNC_END_AFTER)
# Restrict mutation to the executable body (after the contract's "clang-format on"),
# so comment/assert text containing the same tokens is never touched.
body_start = orig.index("// clang-format on", s)
head, body, tail = orig[:body_start], orig[body_start:e], orig[e:]

results = {}
for name, (old, new) in MUTANTS.items():
    assert body.count(old) == 1, f"{name}: expected 1 occurrence of {old!r}, got {body.count(old)}"
    mutated = head + body.replace(old, new) + tail
    open(SRC, "w").write(mutated)
    try:
        p = subprocess.run(["./verify.sh", "AddHuffmanBits"], capture_output=True, text=True, timeout=900)
        out = p.stdout + p.stderr
    finally:
        open(SRC, "w").write(orig)
    if "VERIFICATION FAILED" in out:
        results[name] = "KILLED"
    elif "VERIFICATION SUCCESSFUL" in out:
        results[name] = "SURVIVED"
    else:
        results[name] = "ERROR"
    print(f"{name:14s} -> {results[name]}")

killed = sum(1 for v in results.values() if v == "KILLED")
print(f"\nKill score: {killed}/{len(MUTANTS)}")
