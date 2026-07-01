import subprocess, re, os, sys

SRC = "zopfli.c"          # contracted scratch file
FUNC = "GetMatch"

def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNC, "--file", SRC],
        capture_output=True, text=True,
    ).stdout
    mutants = []
    for block in out.split("--- original")[1:]:
        orig_line = mut_line = None
        for ln in block.splitlines():
            if ln.startswith("-") and not ln.startswith("---"):
                orig_line = ln[1:]
            elif ln.startswith("+") and not ln.startswith("+++"):
                mut_line = ln[1:]
        if orig_line is not None and mut_line is not None:
            mutants.append((orig_line, mut_line))
    return mutants

base = open(SRC).read()
mutants = get_mutants()
killed = survived = buildfail = 0
print("Found %d mutants\n" % len(mutants))
for i, (o, m) in enumerate(mutants):
    if base.count(o) != 1:
        print("%-6d SKIP (orig not unique: %d)" % (i, base.count(o)))
        continue
    mutated = base.replace(o, m)
    tmp = "mut_%d.c" % i
    open(tmp, "w").write(mutated)
    # compile
    cc = subprocess.run(["goto-cc", "-o", "m.goto", tmp, "--function", FUNC],
                        capture_output=True, text=True)
    if cc.returncode != 0:
        print("%-6d BUILD-FAIL (excluded) | %s" % (i, m.strip()))
        buildfail += 1; os.remove(tmp); continue
    subprocess.run(["goto-instrument", "--partial-loops", "--unwind", "5", "m.goto", "m.goto"],
                   capture_output=True, text=True)
    subprocess.run(["goto-instrument", "--enforce-contract", FUNC, "m.goto", "mc.goto"],
                   capture_output=True, text=True)
    r = subprocess.run(["cbmc", "mc.goto", "--function", FUNC, "--depth", "200"],
                       capture_output=True, text=True)
    ok = "VERIFICATION SUCCESSFUL" in r.stdout
    if ok:
        verdict = "SURVIVED"; survived += 1
    else:
        verdict = "KILLED"; killed += 1
    print("%-6d %-9s | %s -> %s" % (i, verdict, o.strip()[:50], m.strip()[:50]))
    os.remove(tmp)

total = killed + survived
print("\nKILLED=%d SURVIVED=%d BUILD-FAIL=%d  kill score = %d/%d" %
      (killed, survived, buildfail, killed, total))
