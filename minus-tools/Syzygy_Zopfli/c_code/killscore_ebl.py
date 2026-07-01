import subprocess, re, os, sys

SRC = "zopfli.c"
FUNC = "ExtractBitLengths"


def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNC, "--file", SRC],
        capture_output=True, text=True,
    ).stdout
    mutants = []
    for block in out.split("--- original")[1:]:
        m = re.search(r"@@ -(\d+) \+(\d+) @@", block)
        if not m:
            continue
        lineno = int(m.group(1))
        orig_line = mut_line = None
        for ln in block.splitlines():
            if ln.startswith("-") and not ln.startswith("---"):
                orig_line = ln[1:]
            elif ln.startswith("+") and not ln.startswith("+++"):
                mut_line = ln[1:]
        if orig_line is not None and mut_line is not None:
            mutants.append((lineno, orig_line, mut_line))
    return mutants


def run_pipeline(src):
    """Mirror verify.sh; return True if VERIFICATION SUCCESSFUL, None if build fails."""
    g = "kmut.goto"
    c = "checking-kmut.goto"
    steps = [
        ["goto-cc", "-o", g, src, "--function", FUNC],
        ["goto-instrument", "--partial-loops", "--unwind", "5", g, g],
        ["goto-instrument", "--enforce-contract", FUNC, g, c],
    ]
    for st in steps:
        r = subprocess.run(st, capture_output=True, text=True)
        if r.returncode != 0:
            return None
    r = subprocess.run(["cbmc", c, "--function", FUNC, "--depth", "200"],
                       capture_output=True, text=True)
    return "VERIFICATION SUCCESSFUL" in r.stdout


orig_text = open(SRC).read()
orig_lines = orig_text.split("\n")
mutants = get_mutants()
killed = survived = buildfail = 0
print("Found %d mutants\n" % len(mutants))
try:
    for i, (lineno, o, m) in enumerate(mutants):
        cur = orig_lines[lineno - 1]
        tag = "L%d#%d" % (lineno, i)
        if cur.strip() != o.strip():
            print("%-10s SKIP (line mismatch): have %r" % (tag, cur.strip()))
            continue
        lines = list(orig_lines)
        lines[lineno - 1] = m
        open(SRC, "w").write("\n".join(lines))
        res = run_pipeline(SRC)
        if res is None:
            print("%-10s BUILD-FAIL (excluded) | %s" % (tag, m.strip()))
            buildfail += 1
        elif res:
            print("%-10s SURVIVED | %s -> %s" % (tag, o.strip(), m.strip()))
            survived += 1
        else:
            print("%-10s KILLED   | %s -> %s" % (tag, o.strip(), m.strip()))
            killed += 1
finally:
    open(SRC, "w").write(orig_text)

total = killed + survived
print("\nKILLED=%d SURVIVED=%d BUILD-FAIL=%d  kill score = %d/%d"
      % (killed, survived, buildfail, killed, total if total else 0))
