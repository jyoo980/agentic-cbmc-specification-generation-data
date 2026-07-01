import subprocess
base=open('zopfli.c').read().split('\n')
# We will replace the whole requires block (lines 396-411) with a candidate, keep ensures.
# Identify block: lines 396..411 (1-based) are requires + comments 406,407.
# We'll rebuild file: keep 388-395 (signature+comment), then candidate requires, then ensures lines 414-435, then body.
def make(candidate_requires, add_ensures0=True, force=None):
    out=[]
    for i,l in enumerate(base):
        ln=i+1
        if 396<=ln<=411:
            if ln==396:
                out.extend(candidate_requires)
            continue
        out.append(l)
        if ln==435:
            if force: out.append('__CPROVER_requires(%s)'%force)
            if add_ensures0: out.append('__CPROVER_ensures(0)')
    open('_e.c','w').write('\n'.join(out))
    r1=subprocess.run(['goto-cc','-o','_e.goto','_e.c','--function','BoundaryPM'],capture_output=True,text=True)
    if r1.returncode!=0: return 'COMPILE_FAIL '+r1.stderr[-200:]
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_e.goto','_e.goto'],capture_output=True)
    r2=subprocess.run(['goto-instrument','--replace-call-with-contract','InitNode','--replace-call-with-contract','BoundaryPM','--enforce-contract','BoundaryPM','_e.goto','_ec.goto'],capture_output=True,text=True)
    c=subprocess.run(['cbmc','_ec.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    if 'VERIFICATION SUCCESSFUL' in c.stdout: return 'SUCCESS'
    if 'VERIFICATION FAILED' in c.stdout: return 'FAILED'
    return 'OTHER: '+c.stdout[-300:]

# Candidate 1: original requires but constant size for lists
cand1=[
"__CPROVER_requires(index >= 0 && index <= 14)",
"__CPROVER_requires(numsymbols >= 1 && numsymbols <= 8)",
"__CPROVER_requires(__CPROVER_is_fresh(lists, 15 * sizeof(*lists)))",
"__CPROVER_requires(__CPROVER_is_fresh(pool, sizeof(*pool)) && __CPROVER_is_fresh(pool->next, sizeof(Node)))",
"__CPROVER_requires(__CPROVER_is_fresh(leaves, (size_t)numsymbols * sizeof(*leaves)))",
"__CPROVER_requires(__CPROVER_is_fresh(lists[index][1], sizeof(Node)))",
"__CPROVER_requires(lists[index][1]->count >= 0)",
"__CPROVER_requires(index >= 1 ==> __CPROVER_is_fresh(lists[index - 1][0], sizeof(Node)))",
"__CPROVER_requires(index >= 1 ==> __CPROVER_is_fresh(lists[index - 1][1], sizeof(Node)))",
"__CPROVER_requires(index >= 1 ==> (lists[index][1]->count < numsymbols && lists[index - 1][0]->weight + lists[index - 1][1]->weight > leaves[lists[index][1]->count].weight))",
]
print('cand1 vacuity-check (ensures0):', make(cand1, True))
print('cand1 original verify       :', make(cand1, False))
