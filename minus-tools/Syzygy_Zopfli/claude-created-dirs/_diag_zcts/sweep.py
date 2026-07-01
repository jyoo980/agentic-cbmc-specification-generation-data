import subprocess, os, re
SRC='/app/Syzygy_Zopfli/c_code/zopfli.c'
orig=open(SRC).read().split('\n')
# locate the contract block of ZopfliCacheToSublen (lines 2812..2830 -> the requires/ensures)
# We'll replace the block between the signature ')' line 2811 and the '{' at line 2831.
# Find indices (0-based): function sig ends line 2811 (idx2810 'unsigned short *sublen)')
# contract lines idx 2811..2829 (lines 2812..2830), body '{' at idx 2830 (line2831)
def build(variant_requires, ensures, mutate=False):
    lines=list(orig)
    block = variant_requires + [ensures]
    # replace idx 2811..2829 inclusive (lines 2812-2830)
    new = lines[:2811] + block + lines[2830:]
    if mutate:
        # apply dist mutant on line 2842 (now shifted). find it
        for i,l in enumerate(new):
            if 'unsigned dist = cache[j * 3 + 1] + 256 * cache[j * 3 + 2];' in l:
                new[i]=l.replace('+ 256','- 256'); break
    return '\n'.join(new)

def run(src_text, tag, depth=200):
    d='/app/_diag_zcts'
    f=f'{d}/v_{tag}.c'; open(f,'w').write(src_text)
    g=f'{d}/v_{tag}.goto'; c=f'{d}/v_{tag}c.goto'
    r=subprocess.run(['goto-cc','-o',g,f,'-I','/app/Syzygy_Zopfli/c_code','--function','ZopfliCacheToSublen'],capture_output=True,text=True)
    if r.returncode: return 'CCFAIL '+r.stderr[:200]
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5',g,g],capture_output=True,text=True)
    r=subprocess.run(['goto-instrument','--replace-call-with-contract','ZopfliMaxCachedSublen','--enforce-contract','ZopfliCacheToSublen',g,c],capture_output=True,text=True)
    if r.returncode: return 'INSTFAIL '+r.stderr[:200]
    r=subprocess.run(['cbmc',c,'--function','ZopfliCacheToSublen','--depth',str(depth)],capture_output=True,text=True)
    o=r.stdout+r.stderr
    if 'VERIFICATION SUCCESSFUL' in o: return 'SUCCESSFUL'
    if 'VERIFICATION FAILED' in o: return 'FAILED'
    return 'UNKNOWN'

ENS=('__CPROVER_ensures(length < 3 ||\n'
'    sublen[0] == (unsigned short)(lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3 + 1]\n'
'        + 256 * lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3 + 2]))')

variants={
 'orig':[
   '__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))',
   '__CPROVER_requires(pos <= 1024)',
   '__CPROVER_requires(__CPROVER_is_fresh(\n    lmc->sublen,\n    (ZOPFLI_CACHE_LENGTH * pos * 3 + ZOPFLI_CACHE_LENGTH * 3) * sizeof(*lmc->sublen)))',
   '__CPROVER_requires(__CPROVER_is_fresh(sublen, 259 * sizeof(*sublen)))',
   '__CPROVER_assigns(__CPROVER_object_whole(sublen))',
 ],
 # smaller pos bound
 'pos8':[
   '__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))',
   '__CPROVER_requires(pos <= 8)',
   '__CPROVER_requires(__CPROVER_is_fresh(\n    lmc->sublen,\n    (ZOPFLI_CACHE_LENGTH * pos * 3 + ZOPFLI_CACHE_LENGTH * 3) * sizeof(*lmc->sublen)))',
   '__CPROVER_requires(__CPROVER_is_fresh(sublen, 259 * sizeof(*sublen)))',
   '__CPROVER_assigns(__CPROVER_object_whole(sublen))',
 ],
 # pos==0 removes symbolic offset
 'pos0':[
   '__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))',
   '__CPROVER_requires(pos == 0)',
   '__CPROVER_requires(__CPROVER_is_fresh(\n    lmc->sublen,\n    (ZOPFLI_CACHE_LENGTH * 3) * sizeof(*lmc->sublen)))',
   '__CPROVER_requires(__CPROVER_is_fresh(sublen, 259 * sizeof(*sublen)))',
   '__CPROVER_assigns(__CPROVER_object_whole(sublen))',
 ],
 # smaller sublen output
 'subsmall':[
   '__CPROVER_requires(__CPROVER_is_fresh(lmc, sizeof(*lmc)))',
   '__CPROVER_requires(pos <= 1024)',
   '__CPROVER_requires(__CPROVER_is_fresh(\n    lmc->sublen,\n    (ZOPFLI_CACHE_LENGTH * pos * 3 + ZOPFLI_CACHE_LENGTH * 3) * sizeof(*lmc->sublen)))',
   '__CPROVER_requires(__CPROVER_is_fresh(sublen, 259 * sizeof(*sublen)))',
   '__CPROVER_requires(length <= 1024)',
   '__CPROVER_assigns(__CPROVER_object_whole(sublen))',
 ],
}
for name,req in variants.items():
    so=run(build(req,ENS,False),name+'_o')
    sm=run(build(req,ENS,True),name+'_m')
    print(f'{name:10} orig={so:12} dist_mutant={sm}')
