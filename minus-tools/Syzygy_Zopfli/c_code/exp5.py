import subprocess, os
base=open('zopfli.c').read().split('\n')
def gen(reqs, mutate=None, ens0=False):
    out=[]
    for i,l in enumerate(base):
        ln=i+1
        if 396<=ln<=411:
            if ln==396: out.extend(reqs)
            continue
        cur=mutate[1] if (mutate and ln==mutate[0]) else l
        out.append(cur)
        if ln==435 and ens0: out.append('__CPROVER_ensures(0)')
    return '\n'.join(out)
def run(src, replace=('InitNode','BoundaryPM')):
    open('_e.c','w').write(src)
    if subprocess.run(['goto-cc','-o','_e.goto','_e.c','--function','BoundaryPM'],capture_output=True).returncode!=0: return 'CFAIL'
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_e.goto','_e.goto'],capture_output=True)
    fl=[]
    for r in replace: fl+=['--replace-call-with-contract',r]
    subprocess.run(['goto-instrument',*fl,'--enforce-contract','BoundaryPM','_e.goto','_ec.goto'],capture_output=True)
    if not os.path.exists('_ec.goto'): return 'INSTR_ERR'
    c=subprocess.run(['cbmc','_ec.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    os.remove('_ec.goto')
    return 'SUCCESS' if 'VERIFICATION SUCCESSFUL' in c.stdout else ('FAILED' if 'VERIFICATION FAILED' in c.stdout else 'OTHER')

# Candidate index==0 restricted, 4 is_fresh + pointer_equals alias for lists[0][1]
R=lambda s:"__CPROVER_requires(%s)"%s
cand_i0_4=[
 R("index == 0"),
 R("numsymbols >= 1 && numsymbols <= 8"),
 R("__CPROVER_is_fresh(lists, sizeof(*lists))"),
 R("__CPROVER_is_fresh(pool, sizeof(*pool)) && __CPROVER_is_fresh(pool->next, sizeof(Node))"),
 R("__CPROVER_is_fresh(leaves, (size_t)numsymbols * sizeof(*leaves))"),
 R("__CPROVER_pointer_equals(lists[index][1], leaves)"),
 R("lists[index][1]->count >= 0"),
]
print('i0_4 vacuity(ens0):', run(gen(cand_i0_4,ens0=True)))
print('i0_4 original     :', run(gen(cand_i0_4)))
