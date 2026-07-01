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
def gen(order, mutate=None, ensures0=False):
    reqs=[CL[k] for k in order]
    out=[]
    for i,l in enumerate(base):
        ln=i+1
        if 396<=ln<=411:
            if ln==396: out.extend(reqs)
            continue
        cur=l
        if mutate and ln==mutate[0]: cur=mutate[1]
        out.append(cur)
        if ln==435 and ensures0: out.append('__CPROVER_ensures(0)')
    return '\n'.join(out)
def run(src):
    open('_e.c','w').write(src)
    if subprocess.run(['goto-cc','-o','_e.goto','_e.c','--function','BoundaryPM'],capture_output=True).returncode!=0: return 'CFAIL'
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_e.goto','_e.goto'],capture_output=True)
    subprocess.run(['goto-instrument','--replace-call-with-contract','InitNode','--replace-call-with-contract','BoundaryPM','--enforce-contract','BoundaryPM','_e.goto','_ec.goto'],capture_output=True)
    c=subprocess.run(['cbmc','_ec.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    return 'SUCCESS' if 'VERIFICATION SUCCESSFUL' in c.stdout else ('FAILED' if 'VERIFICATION FAILED' in c.stdout else 'OTHER')

order=['index_range','numsym','fresh_lists','fresh_i1','fresh_im10','fresh_im11','fresh_pool','fresh_leaves','count_nn','forced']
print('vacuity(ensures0->want FAILED):', run(gen(order,ensures0=True)))
print('original verify (->want SUCCESS):', run(gen(order)))
# L455 mutant: lastcount+1 -> lastcount-1
mut=(455,'        InitNode(leaves[lastcount].weight, lastcount - 1, 0, newchain);')
print('mutant L455 (->want FAILED=killed):', run(gen(order,mutate=mut)))

print('=== LIVE-order from cumulative test ===')
o2=['index_range','fresh_lists','fresh_i1','count_nn','forced','numsym','fresh_pool','fresh_leaves','fresh_im10','fresh_im11']
print('vacuity(ensures0->want FAILED):', run(gen(o2,ensures0=True)))
print('original verify (->want SUCCESS):', run(gen(o2)))
mut=(455,'        InitNode(leaves[lastcount].weight, lastcount - 1, 0, newchain);')
print('mutant L455 (->want FAILED):', run(gen(o2,mutate=mut)))

print('=== probe index==0 reachability on proper order ===')
order=['index_range','numsym','fresh_lists','fresh_i1','fresh_im10','fresh_im11','fresh_pool','fresh_leaves','count_nn','forced']
# replace ensures0 with index==0 ==> 0
def gen_probe(order):
    reqs=[CL[k] for k in order]
    out=[]
    for i,l in enumerate(base):
        ln=i+1
        if 396<=ln<=411:
            if ln==396: out.extend(reqs)
            continue
        out.append(l)
        if ln==435: out.append('__CPROVER_ensures(index==0 ==> 0)')
    return '\n'.join(out)
print('index==0 reachable? (want FAILED):', run(gen_probe(order)))
print('index>=1 reachable? proper order:', run(gen_probe2:=gen(order,ensures0=False).replace('{','{',1)) if False else 'skip')
