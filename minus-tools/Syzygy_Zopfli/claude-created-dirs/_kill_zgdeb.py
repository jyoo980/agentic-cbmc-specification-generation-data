import sys, os
sys.path.insert(0, '/app')
from tools.run_cbmc import run_cbmc

SRC = '/app/Syzygy_Zopfli/c_code/zopfli.c'
TMP = '/app/Syzygy_Zopfli/c_code/_mut_zgdeb.c'
FN = 'ZopfliGetDistExtraBits'

muts = []
L94 = '    if (dist < 5)'
for op in ['<=', '>', '>=', '==', '!=']:
    muts.append((2194, L94, L94.replace('< 5', '%s 5' % op)))
R = '    return (31 ^ __builtin_clz(dist - 1)) - 1; /* log2(dist - 1) - 1 */'
muts.append((2196, R, R.replace(')) - 1;', ')) + 1;')))
muts.append((2196, R, R.replace('dist - 1)) - 1;', 'dist + 1)) - 1;')))

lines = open(SRC).read().split('\n')
killed = 0
total = 0
for ln, orig, mut in muts:
    total += 1
    if lines[ln-1] != orig:
        print('LINEMISS', ln, repr(lines[ln-1]))
        continue
    nl = lines[:]
    nl[ln-1] = mut
    open(TMP, 'w').write('\n'.join(nl))
    r = run_cbmc(FN, TMP)
    ok = r.is_function_verified
    print(('SURVIVED' if ok else 'KILLED  '), ln, mut.strip())
    killed += 0 if ok else 1
os.path.exists(TMP) and os.remove(TMP)
print('=== Killed %d / %d ===' % (killed, total))
