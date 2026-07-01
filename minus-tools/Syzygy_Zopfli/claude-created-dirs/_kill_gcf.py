import sys, os
sys.path.insert(0, '/app')
from tools.run_cbmc import run_cbmc

SRC = '/app/Syzygy_Zopfli/c_code/zopfli.c'
TMP = '/app/Syzygy_Zopfli/c_code/_mut_gcf.c'
FN = 'GetCostFixed'

# (line, original, mutant)
muts = []
L2272 = '        if (lsym <= 279)'
for op in ['<', '>', '>=', '==', '!=']:
    muts.append((2272, L2272, L2272.replace('<= 279', '%s 279' % op)))
L2261 = '        if (litlen <= 143)'
for op in ['<', '>', '>=', '==', '!=']:
    muts.append((2261, L2261, L2261.replace('<= 143', '%s 143' % op)))
muts.append((2259, '    if (dist == 0)', '    if (dist != 0)'))
muts.append((2277, '        return cost + dbits + lbits;', '        return cost + dbits - lbits;'))
muts.append((2277, '        return cost + dbits + lbits;', '        return cost - dbits + lbits;'))

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
