import subprocess,sys
base=open('zopfli.c').read().split('\n')
# requires lines (1-based) to optionally disable
req_lines=[396,397,398,399,400,401,402,403,404,405,408,409,410,411]
def build(disable, force_index0=True):
    lines=list(base)
    # add ensures(0) after line 435 (idx434)
    # comment out disabled requires by replacing with nothing (but careful with multiline)
    out=[]
    for i,l in enumerate(lines):
        ln=i+1
        if ln in disable:
            out.append('/*DIS*/')
            continue
        out.append(l)
        if ln==434:
            if force_index0:
                out.append('__CPROVER_requires(index==0)')
            out.append('__CPROVER_ensures(0)')
    src='\n'.join(out)
    open('_b.c','w').write(src)
    subprocess.run(['goto-cc','-o','_b.goto','_b.c','--function','BoundaryPM'],capture_output=True)
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_b.goto','_b.goto'],capture_output=True)
    r=subprocess.run(['goto-instrument','--replace-call-with-contract','InitNode','--replace-call-with-contract','BoundaryPM','--enforce-contract','BoundaryPM','_b.goto','_bc.goto'],capture_output=True,text=True)
    c=subprocess.run(['cbmc','_bc.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    # vacuous if SUCCESSFUL (ensures(0) passed)
    return 'VERIFICATION SUCCESSFUL' in c.stdout, r.stderr

# First: with nothing disabled, force index0
vac,err=build(set())
print("force index0, nothing disabled -> vacuous(SUCCESS)=",vac)
