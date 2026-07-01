import sys, shutil, os
from tools.run_cbmc import run_cbmc

SRC='/app/Syzygy_Zopfli/c_code/zopfli.c'
TMP='/app/Syzygy_Zopfli/c_code/_mut_zcopylz.c'
FN='ZopfliCopyLZ77Store'

# (line, original, mutant)
muts=[]
for ln,orig in [(5148,'    for (i = 0; i < source->size; i++)'),
                (5173,'    for (i = 0; i < llsize; i++)'),
                (5182,'    for (i = 0; i < dsize; i++)')]:
    var=orig.split('i < ')[1].split(';')[0]
    for op in ['<=','>','>=','==','!=']:
        muts.append((ln,orig,orig.replace('i < ','i %s '%op)))
muts.append((5138,'    if (!dest->litlens || !dest->dists)','    if (!dest->litlens && !dest->dists)'))
muts.append((5142,'    if (!dest->ll_symbol || !dest->d_symbol)','    if (!dest->ll_symbol && !dest->d_symbol)'))
muts.append((5144,'    if (!dest->ll_counts || !dest->d_counts)','    if (!dest->ll_counts && !dest->d_counts)'))

lines=open(SRC).read().split('\n')
killed=0; total=0
for ln,orig,mut in muts:
    total+=1
    if lines[ln-1]!=orig:
        print('LINEMISS',ln,repr(lines[ln-1])); continue
    nl=lines[:]; nl[ln-1]=mut
    open(TMP,'w').write('\n'.join(nl))
    r=run_cbmc(FN,TMP)
    ok = (r.returncode==0 and 'PASS' in str(r))
    if ok:
        print('SURVIVED %4d %s'%(ln,mut.strip()))
    else:
        print('KILLED   %4d %s'%(ln,mut.strip())); killed+=1
os.path.exists(TMP) and os.remove(TMP)
print('=== Killed %d / %d ==='%(killed,total))
