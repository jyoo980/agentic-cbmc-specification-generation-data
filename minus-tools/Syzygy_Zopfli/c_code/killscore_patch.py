import sys, subprocess, re, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc, compile_with_goto_cc

SRC = "zopfli.c"
FUNC = "PatchDistanceCodesForBuggyDecoders"


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


orig_lines = open(SRC).read().split("\n")
mutants = get_mutants()
killed = survived = buildfail = 0
print("Found %d mutants\n" % len(mutants))
for i, (lineno, o, m) in enumerate(mutants):
    cur = orig_lines[lineno - 1]
    tag = "L%d#%d" % (lineno, i)
    if cur.strip() != o.strip():
        print("%-12s SKIP (line mismatch): have %r want %r" % (tag, cur.strip(), o.strip()))
        continue
    lines = list(orig_lines)
    lines[lineno - 1] = m
    tmp = "mutp_%d.c" % i
    open(tmp, "w").write("\n".join(lines))
    if compile_with_goto_cc(FUNC, tmp) != 0:
        print("%-12s BUILD-FAIL (excluded) | %s" % (tag, m.strip()))
        buildfail += 1
        os.remove(tmp)
        continue
    r = run_cbmc(FUNC, tmp)
    if r.is_function_verified:
        verdict = "SURVIVED"; survived += 1
    else:
        verdict = "KILLED"; killed += 1
    print("%-12s %-9s | %s -> %s" % (tag, verdict, o.strip(), m.strip()))
    os.remove(tmp)

total = killed + survived
print("\nKILLED=%d SURVIVED=%d BUILD-FAIL=%d  kill score = %d/%d" %
      (killed, survived, buildfail, killed, total))
