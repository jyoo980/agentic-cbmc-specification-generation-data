#!/usr/bin/env python3
"""Score the enableRawMode mutants against its CBMC contract.

Each mutant is applied to kilo.c by a unique-string replacement (so line numbers
don't matter), built with the stubs/rawmode.c models, the contract is enforced,
and CBMC is run at the canonical --depth 200.  A mutant is KILLED iff
verification fails.
"""
import subprocess, tempfile, shutil, os

KILO = "/app/kilo/kilo.c"
STUB = "/app/stubs/rawmode.c"
FN = "enableRawMode"

MUTANTS = [
    ("tcset_le",  "if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;",
                  "if (tcsetattr(fd,TCSAFLUSH,&raw) <= 0) goto fatal;"),
    ("tcset_gt",  "if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;",
                  "if (tcsetattr(fd,TCSAFLUSH,&raw) > 0) goto fatal;"),
    ("tcset_ge",  "if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;",
                  "if (tcsetattr(fd,TCSAFLUSH,&raw) >= 0) goto fatal;"),
    ("tcset_eq0", "if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;",
                  "if (tcsetattr(fd,TCSAFLUSH,&raw) == 0) goto fatal;"),
    ("tcset_ne0", "if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;",
                  "if (tcsetattr(fd,TCSAFLUSH,&raw) != 0) goto fatal;"),
    ("tcget_ne",  "if (tcgetattr(fd,&orig_termios) == -1) goto fatal;",
                  "if (tcgetattr(fd,&orig_termios) != -1) goto fatal;"),
]

def run(src_text, work):
    src = os.path.join(work, "kilo.c")
    with open(src, "w") as f:
        f.write(src_text)
    shutil.copy(STUB, os.path.join(work, "rawmode.c"))
    g = os.path.join(work, f"{FN}.goto")
    chk = os.path.join(work, f"checking-{FN}.goto")
    def sh(cmd):
        return subprocess.run(cmd, cwd=work, capture_output=True, text=True)
    if sh(["goto-cc","-o",g,"kilo.c","rawmode.c","--function",FN]).returncode != 0:
        return "BUILD_FAIL"
    sh(["goto-instrument","--partial-loops","--unwind","5",g,g])
    sh(["goto-instrument","--enforce-contract",FN,g,chk])
    r = sh(["cbmc",chk,"--function",FN,"--depth","200"])
    out = r.stdout + r.stderr
    if "VERIFICATION SUCCESSFUL" in out:
        return "SURVIVED"
    if "VERIFICATION FAILED" in out:
        return "KILLED"
    return "OTHER:\n" + out[-500:]

base = open(KILO).read()
# sanity: original must verify
with tempfile.TemporaryDirectory() as w:
    print("original:", run(base, w))

killed = 0
for name, old, new in MUTANTS:
    assert old in base, f"pattern not found for {name}"
    mtext = base.replace(old, new)
    with tempfile.TemporaryDirectory() as w:
        res = run(mtext, w)
    print(f"{name:10s}: {res}")
    if res == "KILLED":
        killed += 1
print(f"\nKILL SCORE: {killed}/{len(MUTANTS)}")
