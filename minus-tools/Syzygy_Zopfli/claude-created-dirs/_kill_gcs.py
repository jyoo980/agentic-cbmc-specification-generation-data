import sys, os
sys.path.insert(0, '/app')
from tools.run_cbmc import run_cbmc

SRC = '/app/Syzygy_Zopfli/c_code/zopfli.c'
TMP = '/app/Syzygy_Zopfli/c_code/_mut_gcs.c'
FN = 'GetCostStat'

L2206 = "    if (dist == 0)"
L2216 = "        return lbits + dbits + stats->ll_symbols[lsym] + stats->d_symbols[dsym];"
muts = [
    (2264, L2206, '    if (dist != 0)'),
    (2274, L2216, L2216.replace('+ stats->d_symbols[dsym]', '- stats->d_symbols[dsym]')),
    (2274, L2216, L2216.replace('dbits + stats->ll_symbols[lsym]', 'dbits - stats->ll_symbols[lsym]')),
    (2274, L2216, L2216.replace('lbits + dbits', 'lbits - dbits')),
]

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
    if ok:
        print('SURVIVED %4d  %s' % (ln, mut.strip()))
    else:
        print('KILLED   %4d  %s' % (ln, mut.strip()))
        killed += 1
os.path.exists(TMP) and os.remove(TMP)
print('=== Killed %d / %d ===' % (killed, total))
