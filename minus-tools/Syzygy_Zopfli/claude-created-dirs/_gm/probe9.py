import subprocess
base=open("zopfli.c").read()
sig='''static const unsigned char *GetMatch(const unsigned char *scan,
                                     const unsigned char *match,
                                     const unsigned char *end,
                                     const unsigned char *safe_end)
'''
idx=base.index(sig)+len(sig)
bpos=base.index("\n{\n", idx)
prefix=base[:idx]; body=base[bpos:]
def verify(reqs,label):
    block="".join(r+"\n" for r in reqs)+"__CPROVER_assigns()\n"
    b=body.replace("\n{\n","\n{\n    __CPROVER_assert(0, \"REACHBODY\");\n",1)
    open("zr.c","w").write(prefix+block+b)
    cc=subprocess.run(["goto-cc","-o","zr.goto","zr.c","--function","GetMatch"],capture_output=True,text=True)
    if cc.returncode: print(label,"CCFAIL"); return
    subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
    subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch","--depth","200"],capture_output=True,text=True)
    v=[l for l in r.stdout.splitlines() if "VERIFICATION" in l]
    reachst="REACHBODY: FAILURE" in r.stdout
    fails=[l for l in r.stdout.splitlines() if l.strip().endswith("FAILURE") and "GetMatch" in l and "REACHBODY" not in l]
    cats={}
    for f in fails:
        c=f.split("line")[1].split()[1] if "line" in f else "?"
        cats[c]=cats.get(c,0)+1
    print("%-30s %s reach=%s fails=%d %s"%(label,v[0] if v else "?",reachst,len(fails),dict(cats)))
    for f in fails[:6]: print("       ",f.strip())

NN=['__CPROVER_requires(scan != NULL)','__CPROVER_requires(end != NULL)','__CPROVER_requires(match != NULL)','__CPROVER_requires(safe_end != NULL)']
SOe='__CPROVER_requires(__CPROVER_same_object(scan, end))'
SOm='__CPROVER_requires(__CPROVER_same_object(scan, match))'
SOs='__CPROVER_requires(__CPROVER_same_object(scan, safe_end))'
OFS0='__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) == 0)'
LE='__CPROVER_requires(scan <= end)'
ENDOK='__CPROVER_requires((size_t)(end - scan) <= __CPROVER_OBJECT_SIZE(scan))'

verify([SOe,SOm,SOs,OFS0,LE,ENDOK], "base(reach earlier)")
verify(NN+[SOe,SOm,SOs,OFS0,LE,ENDOK], "base+NN")
verify(NN+[SOe,SOs,OFS0,LE,ENDOK], "base+NN-SOm")
