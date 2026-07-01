import subprocess, itertools
base=open('zopfli.c').read().split('\n')
CL={
 'idx':"__CPROVER_requires(index >= 0 && index <= 14)",
 'num':"__CPROVER_requires(numsymbols >= 1 && numsymbols <= 8)",
 'fl':"__CPROVER_requires(__CPROVER_is_fresh(lists, 15 * sizeof(*lists)))",
 'fp':"__CPROVER_requires(__CPROVER_is_fresh(pool, sizeof(*pool)) && __CPROVER_is_fresh(pool->next, sizeof(Node)))",
 'fv':"__CPROVER_requires(__CPROVER_is_fresh(leaves, (size_t)numsymbols * sizeof(*leaves)))",
 'fi':"__CPROVER_requires(__CPROVER_is_fresh(lists[index][1], sizeof(Node)))",
 'cn':"__CPROVER_requires(lists[index][1]->count >= 0)",
 'f0':"__CPROVER_requires(index >= 1 ==> __CPROVER_is_fresh(lists[index - 1][0], sizeof(Node)))",
 'f1':"__CPROVER_requires(index >= 1 ==> __CPROVER_is_fresh(lists[index - 1][1], sizeof(Node)))",
 'fo':"__CPROVER_requires(index >= 1 ==> (lists[index][1]->count < numsymbols && lists[index - 1][0]->weight + lists[index - 1][1]->weight > leaves[lists[index][1]->count].weight))",
}
def gen(order, mutate=None, ens0=False):
    reqs=[CL[k] for k in order]; out=[]
    for i,l in enumerate(base):
        ln=i+1
        if 396<=ln<=411:
            if ln==396: out.extend(reqs)
            continue
        cur=mutate[1] if (mutate and ln==mutate[0]) else l
        out.append(cur)
        if ln==435 and ens0: out.append('__CPROVER_ensures(0)')
    return '\n'.join(out)
def run(src):
    open('_e.c','w').write(src)
    if subprocess.run(['goto-cc','-o','_e.goto','_e.c','--function','BoundaryPM'],capture_output=True).returncode!=0: return 'CFAIL'
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_e.goto','_e.goto'],capture_output=True)
    r=subprocess.run(['goto-instrument','--replace-call-with-contract','InitNode','--replace-call-with-contract','BoundaryPM','--enforce-contract','BoundaryPM','_e.goto','_ec.goto'],capture_output=True,text=True)
    if 'xception' in r.stderr or not __import__('os').path.exists('_ec.goto'): return 'INSTR_ERR'
    c=subprocess.run(['cbmc','_ec.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    import os; os.remove('_ec.goto')
    return 'SUCCESS' if 'VERIFICATION SUCCESSFUL' in c.stdout else ('FAILED' if 'VERIFICATION FAILED' in c.stdout else 'OTHER')
mutL455=(455,'        InitNode(leaves[lastcount].weight, lastcount - 1, 0, newchain);')
def evalorder(order):
    v=run(gen(order,ens0=True))   # want FAILED (non-vacuous)
    s=run(gen(order))             # want SUCCESS (sound)
    k=run(gen(order,mutate=mutL455)) # want FAILED (kill)
    return v,s,k

candidates={
 'A_proper':['idx','num','fl','fi','fp','fv','cn','f0','f1','fo'],
 'B_num_late':['idx','fl','fi','cn','num','fp','fv','f0','f1','fo'],
 'C_fresh_first':['idx','num','fl','fi','f0','f1','fp','fv','cn','fo'],
 'D_vals_last':['idx','num','fl','fp','fv','fi','f0','f1','cn','fo'],
 'E_fi_after_im':['idx','num','fl','f0','f1','fi','fp','fv','cn','fo'],
}
for name,o in candidates.items():
    print('%-14s vac/sound/kill ='%name, evalorder(o))
