import subprocess
base=open('zopfli.c').read().split('\n')
CL={
 'index_range':"__CPROVER_requires(index >= 0 && index <= 14)",
 'numsym':"__CPROVER_requires(numsymbols >= 1 && numsymbols <= 8)",
 'fresh_lists':"__CPROVER_requires(__CPROVER_is_fresh(lists, 15 * sizeof(*lists)))",
 'fresh_pool':"__CPROVER_requires(__CPROVER_is_fresh(pool, sizeof(*pool)) && __CPROVER_is_fresh(pool->next, sizeof(Node)))",
 'fresh_leaves':"__CPROVER_requires(__CPROVER_is_fresh(leaves, (size_t)numsymbols * sizeof(*leaves)))",
 'fresh_i1':"__CPROVER_requires(__CPROVER_is_fresh(lists[index][1], sizeof(Node)))",
 'count_nn':"__CPROVER_requires(lists[index][1]->count >= 0)",
 'fresh_im10':"__CPROVER_requires(index >= 1 ==> __CPROVER_is_fresh(lists[index - 1][0], sizeof(Node)))",
 'fresh_im11':"__CPROVER_requires(index >= 1 ==> __CPROVER_is_fresh(lists[index - 1][1], sizeof(Node)))",
 'forced':"__CPROVER_requires(index >= 1 ==> (lists[index][1]->count < numsymbols && lists[index - 1][0]->weight + lists[index - 1][1]->weight > leaves[lists[index][1]->count].weight))",
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
def run(src, replace):
    open('_e.c','w').write(src)
    if subprocess.run(['goto-cc','-o','_e.goto','_e.c','--function','BoundaryPM'],capture_output=True).returncode!=0: return 'CFAIL'
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_e.goto','_e.goto'],capture_output=True)
    flags=[]
    for r in replace: flags+=['--replace-call-with-contract',r]
    subprocess.run(['goto-instrument',*flags,'--enforce-contract','BoundaryPM','_e.goto','_ec.goto'],capture_output=True)
    c=subprocess.run(['cbmc','_ec.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    return 'SUCCESS' if 'VERIFICATION SUCCESSFUL' in c.stdout else ('FAILED' if 'VERIFICATION FAILED' in c.stdout else 'OTHER')

order=['index_range','numsym','fresh_lists','fresh_i1','fresh_im10','fresh_im11','fresh_pool','fresh_leaves','count_nn','forced']
for rep in [['InitNode'], ['InitNode','BoundaryPM']]:
    print('replace=',rep)
    print('  vacuity(ens0 want FAILED):', run(gen(order,ens0=True), rep))
    print('  original (want SUCCESS)  :', run(gen(order), rep))
    print('  L455 (want FAILED)       :', run(gen(order,mutate=(455,'        InitNode(leaves[lastcount].weight, lastcount - 1, 0, newchain);')), rep))
