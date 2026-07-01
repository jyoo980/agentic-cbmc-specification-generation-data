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
    fails=[l for l in r.stdout.splitlines() if l.strip().endswith("FAILURE") and "GetMatch" in l]
    print("%-45s %s reach=%s fails=%d"%(label,v,reachst,len(fails)))
    for f in fails[:4]: print("       ",f.strip())

NN=['__CPROVER_requires(scan != NULL)','__CPROVER_requires(end != NULL)']
SOe='__CPROVER_requires(__CPROVER_same_object(scan, end))'
SOm='__CPROVER_requires(__CPROVER_same_object(scan, match))'
SOs='__CPROVER_requires(__CPROVER_same_object(scan, safe_end))'
RS='__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))'
RM='__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))'

verify([RS,SOe], "RS + SO(end)")
# offset/objsize based validity instead of r_ok, all same object
OFS0='__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) == 0)'
ENDOK='__CPROVER_requires((size_t)(end - scan) <= __CPROVER_OBJECT_SIZE(scan))'
LE='__CPROVER_requires(scan <= end)'
verify([SOe,SOm,SOs,OFS0,LE,ENDOK], "allSO+off0+endok (no r_ok)")
verify([SOe,SOm,SOs,OFS0,LE,ENDOK,RM], "allSO+off0+endok+RM(match)")
