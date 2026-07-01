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
    if cc.returncode: print(label,"CCFAIL",cc.stderr[-150:]); return
    subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
    subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch","--depth","200"],capture_output=True,text=True)
    v=[l for l in r.stdout.splitlines() if "VERIFICATION" in l]
    reachst="REACHBODY: FAILURE" in r.stdout
    fails=[l for l in r.stdout.splitlines() if l.strip().endswith("FAILURE") and "GetMatch" in l and "REACHBODY" not in l]
    print("%-30s %s reach=%s fails=%d"%(label,v,reachst,len(fails)))
    for f in fails[:8]: print("       ",f.strip())

NN=['__CPROVER_requires(scan != NULL)','__CPROVER_requires(end != NULL)','__CPROVER_requires(match != NULL)','__CPROVER_requires(safe_end != NULL)']
SOe='__CPROVER_requires(__CPROVER_same_object(scan, end))'
SOm='__CPROVER_requires(__CPROVER_same_object(scan, match))'
SOs='__CPROVER_requires(__CPROVER_same_object(scan, safe_end))'
OFS0='__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) == 0)'
LE='__CPROVER_requires(scan <= end)'
SE='__CPROVER_requires(safe_end == end - 8)'
ENDOK='__CPROVER_requires((size_t)(end - scan) <= __CPROVER_OBJECT_SIZE(scan))'
MOFF='__CPROVER_requires(__CPROVER_POINTER_OFFSET(match) >= 0)'
MEND='__CPROVER_requires((size_t)(__CPROVER_POINTER_OFFSET(match) + (end - scan)) <= __CPROVER_OBJECT_SIZE(scan))'

verify(NN+[SOe,SOm,SOs,OFS0,LE,ENDOK,MOFF,MEND], "full-offset")
verify(NN+[SOe,SOm,SOs,OFS0,LE,SE,ENDOK,MOFF,MEND], "full-offset+SE")
