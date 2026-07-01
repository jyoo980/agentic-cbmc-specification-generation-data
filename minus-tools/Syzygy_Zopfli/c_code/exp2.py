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
def vac(keys):
    reqs=[CL[k] for k in keys]
    out=[]
    for i,l in enumerate(base):
        ln=i+1
        if 396<=ln<=411:
            if ln==396: out.extend(reqs)
            continue
        out.append(l)
        if ln==435: out.append('__CPROVER_ensures(0)')
    open('_e.c','w').write('\n'.join(out))
    if subprocess.run(['goto-cc','-o','_e.goto','_e.c','--function','BoundaryPM'],capture_output=True).returncode!=0: return 'CFAIL'
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_e.goto','_e.goto'],capture_output=True)
    subprocess.run(['goto-instrument','--replace-call-with-contract','InitNode','--replace-call-with-contract','BoundaryPM','--enforce-contract','BoundaryPM','_e.goto','_ec.goto'],capture_output=True)
    c=subprocess.run(['cbmc','_ec.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    return 'VACUOUS' if 'VERIFICATION SUCCESSFUL' in c.stdout else 'LIVE'

core=['index_range','fresh_lists','fresh_i1','count_nn']
print('core', vac(core))
for k in CL:
    if k in core: continue
    print('core+',k, vac(core+[k]))

print('--- start from core+forced (LIVE) and add others ---')
cf=['index_range','fresh_lists','fresh_i1','count_nn','forced']
print('core+forced', vac(cf))
for k in CL:
    if k in cf: continue
    print('cf+',k, vac(cf+[k]))

print('--- cumulative additions to core+forced ---')
acc=['index_range','fresh_lists','fresh_i1','count_nn','forced']
for k in ['numsym','fresh_pool','fresh_leaves','fresh_im10','fresh_im11']:
    acc=acc+[k]
    print('+%-12s'%k, vac(acc))

print('--- order sensitivity ---')
orig_order=['index_range','numsym','fresh_lists','fresh_pool','fresh_leaves','fresh_i1','count_nn','fresh_im10','fresh_im11','forced']
print('original order        ', vac(orig_order))
reord=['index_range','numsym','fresh_lists','fresh_i1','count_nn','fresh_pool','fresh_leaves','fresh_im10','fresh_im11','forced']
print('fresh_i1 moved earlier', vac(reord))
