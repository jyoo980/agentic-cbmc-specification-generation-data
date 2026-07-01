#!/usr/bin/env python3
"""Apply each read_stdin_to_bytes mutant, run the CBMC pipeline, report killed/survived."""
import subprocess, shutil, sys, os

SRC = "zopfli.c"
FUNC = "read_stdin_to_bytes"

# (lineno (1-based), original_substr, mutated_substr)
MUTANTS = [
    ("3527-neq", "if (new_buffer == NULL)", "if (new_buffer != NULL)"),
    ("3523-lt",  "if (total_read >= buffer_size)", "if (total_read < buffer_size)"),
    ("3523-le",  "if (total_read >= buffer_size)", "if (total_read <= buffer_size)"),
    ("3523-gt",  "if (total_read >= buffer_size)", "if (total_read > buffer_size)"),
    ("3523-eq",  "if (total_read >= buffer_size)", "if (total_read == buffer_size)"),
    ("3523-ne",  "if (total_read >= buffer_size)", "if (total_read != buffer_size)"),
    ("3520-eq",  "while ((ch = getchar()) != EOF)", "while ((ch = getchar()) == EOF)"),
    ("3512-neq", "if (buffer == NULL)", "if (buffer != NULL)"),
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
            return -1, r.stdout + r.stderr  # build failure
    return None, ""

orig = open(SRC).read()
for name, o, m in MUTANTS:
    assert orig.count(o) == 1, "pattern not unique for %s: %s" % (name, o)
    mutated = orig.replace(o, m)
    tmp = "mutant_%s.c" % name
    open(tmp, "w").write(mutated)
    rc, out = run_pipeline(tmp)
    if rc == -1:
        verdict = "BUILD-FAIL (excluded)"
    elif rc == 0:
        verdict = "SURVIVED (cbmc passed)"
    else:
        verdict = "KILLED (cbmc failed)"
    print("%-10s %s" % (name, verdict))
    os.remove(tmp)
