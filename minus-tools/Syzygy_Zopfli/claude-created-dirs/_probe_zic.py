from tools.run_cbmc import run_cbmc
SRC='/app/Syzygy_Zopfli/c_code/zopfli.c'
TMP='/app/Syzygy_Zopfli/c_code/_probe_zic.c'
lines=open(SRC).read().split('\n')
print('L4369:',repr(lines[4368]))
nl=lines[:]; nl[4368]=nl[4368].replace('i < blocksize','i <= blocksize')
open(TMP,'w').write('\n'.join(nl))
r=run_cbmc('ZopfliInitCache',TMP)
print('mutant <=blocksize ->', str(r), 'rc=',r.returncode)
import os; os.remove(TMP)
