import subprocess
base=open('zopfli.c').read().split('\n')
# map requires clause -> set of 1-based lines to blank
clauses={
 'index_range':[396],
 'numsym_range':[397],
 'fresh_lists':[398],
 'fresh_pool':[399,400],
 'fresh_leaves':[401],
 'fresh_lists_i_1':[402],
 'count_nonneg':[403],
 'fresh_im1_0':[404],
 'fresh_im1_1':[405],
 'forced_leaf':[408,409,410,411],
}
def build(disable_lines, extra=None):
    out=[]
    for i,l in enumerate(base):
        ln=i+1
        if ln in disable_lines:
            out.append('/*x*/'); continue
        out.append(l)
        if ln==435:
            if extra: out.append('__CPROVER_requires(%s)'%extra)
            out.append('__CPROVER_ensures(0)')
    open('_b.c','w').write('\n'.join(out))
    r1=subprocess.run(['goto-cc','-o','_b.goto','_b.c','--function','BoundaryPM'],capture_output=True,text=True)
    if r1.returncode!=0:
        return 'COMPILE_FAIL'
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_b.goto','_b.goto'],capture_output=True)
    subprocess.run(['goto-instrument','--replace-call-with-contract','InitNode','--replace-call-with-contract','BoundaryPM','--enforce-contract','BoundaryPM','_b.goto','_bc.goto'],capture_output=True)
    c=subprocess.run(['cbmc','_bc.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    return 'VACUOUS' if 'VERIFICATION SUCCESSFUL' in c.stdout else 'LIVE'

print('baseline (force index==0):', build(set(),'index==0'))
for name,lns in clauses.items():
    print('drop %-14s'%name, build(set(lns),'index==0'))
