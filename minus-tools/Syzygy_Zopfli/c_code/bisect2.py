import subprocess
base=open('zopfli.c').read().split('\n')
def build(extra_req):
    out=[]
    for i,l in enumerate(base):
        out.append(l)
        if i+1==435:
            for e in extra_req:
                out.append('__CPROVER_requires(%s)'%e)
            out.append('__CPROVER_ensures(0)')
    open('_b.c','w').write('\n'.join(out))
    subprocess.run(['goto-cc','-o','_b.goto','_b.c','--function','BoundaryPM'],capture_output=True)
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','_b.goto','_b.goto'],capture_output=True)
    subprocess.run(['goto-instrument','--replace-call-with-contract','InitNode','--replace-call-with-contract','BoundaryPM','--enforce-contract','BoundaryPM','_b.goto','_bc.goto'],capture_output=True,text=True)
    c=subprocess.run(['cbmc','_bc.goto','--function','BoundaryPM','--depth','200'],capture_output=True,text=True)
    return 'VERIFICATION SUCCESSFUL' in c.stdout  # True=vacuous

for cond in [['index==0'],['index==1'],['index==2'],['index>=1'],['index<=1'],[]]:
    print(cond,'-> vacuous=',build(cond))
