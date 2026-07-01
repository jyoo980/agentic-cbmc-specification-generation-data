import re, shutil, tempfile, os
from pathlib import Path
from tools.run_cbmc import run_cbmc

SRC = '/app/Syzygy_Zopfli/c_code/zopfli.c'
FUNC = 'ZopfliLZ77OptimalFixed'
orig = Path(SRC).read_text()

# (description, old_line, new_line)
mutants = [
    ("costs blocksize+1 -> -1",
     "    float *costs = (float *)malloc(sizeof(float) * (blocksize + 1));",
     "    float *costs = (float *)malloc(sizeof(float) * (blocksize - 1));"),
    ("length_array blocksize+1 -> -1",
     "        (unsigned short *)malloc(sizeof(unsigned short) * (blocksize + 1));",
     "        (unsigned short *)malloc(sizeof(unsigned short) * (blocksize - 1));"),
    ("blocksize inend-instart -> +",
     "    size_t blocksize = inend - instart;",
     "    size_t blocksize = inend + instart;"),
]

killed = 0
for desc, old, new in mutants:
    assert old in orig, f"NOT FOUND: {desc}"
    mutated = orig.replace(old, new)
    tmpdir = tempfile.mkdtemp()
    mfile = os.path.join(tmpdir, 'zopfli.c')
    # copy whole dir so includes resolve
    Path(mfile).write_text(mutated)
    # symlink headers
    for h in Path('/app/Syzygy_Zopfli/c_code').glob('*.h'):
        os.symlink(h, os.path.join(tmpdir, h.name))
    r = run_cbmc(FUNC, mfile, include_dirs=['/app/Syzygy_Zopfli/c_code'])
    verdict = 'KILLED' if not r.is_function_verified else 'SURVIVED'
    if not r.is_function_verified:
        killed += 1
    print(f"{verdict}: {desc}  (verified={r.is_function_verified}, rc={r.returncode}, failed_step={r.failed_step})")

print(f"\nKILL SCORE: {killed}/{len(mutants)}")
