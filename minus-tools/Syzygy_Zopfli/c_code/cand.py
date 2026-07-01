import subprocess, re, os
base=open('zopfli.c').read().split('\n')
# New requires block replacing lines 396-411 (1-based)
NEW=[
"__CPROVER_requires(index >= 0 && index <= 14)",
"__CPROVER_requires(numsymbols >= 1 && numsymbols <= 8)",
"__CPROVER_requires(__CPROVER_is_fresh(lists, (size_t)(index + 1) * sizeof(*lists)))",
"__CPROVER_requires(__CPROVER_is_fresh(pool, sizeof(*pool)) &&",
"                   __CPROVER_is_fresh(pool->next, sizeof(Node)))",
"__CPROVER_requires(__CPROVER_is_fresh(leaves, (size_t)numsymbols * sizeof(*leaves)))",
"__CPROVER_requires(__CPROVER_r_ok(lists[index][1], sizeof(Node)))",
"__CPROVER_requires(lists[index][1]->count >= 0)",
"__CPROVER_requires(index >= 1 ==> __CPROVER_r_ok(lists[index - 1][0], sizeof(Node)))",
"__CPROVER_requires(index >= 1 ==> __CPROVER_r_ok(lists[index - 1][1], sizeof(Node)))",
"__CPROVER_requires(index >= 1 ==>",
"    (lists[index][1]->count < numsymbols &&",
"     lists[index - 1][0]->weight + lists[index - 1][1]->weight >",
"         leaves[lists[index][1]->count].weight))",
]
def gen(mutate=None, ens0=False):
    out=[]
    for i,l in enumerate(base):
        ln=i+1
        if 396<=ln<=411:
            if ln==396: out.extend(NEW)
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
    if not os.path.exists('_ec.goto'): return 'INSTR_ERR'
    c=subprocess.run(['cbmc','_ec.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    os.remove('_ec.goto')
    return 'SUCCESS' if 'VERIFICATION SUCCESSFUL' in c.stdout else ('FAILED' if 'VERIFICATION FAILED' in c.stdout else 'OTHER')
print('vacuity(ens0 want FAILED):', run(gen(ens0=True)))
print('original (want SUCCESS)  :', run(gen()))
