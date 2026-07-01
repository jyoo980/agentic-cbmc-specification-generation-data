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
def verify(reqs,label,reach=True):
    block="".join(r+"\n" for r in reqs)+"__CPROVER_assigns()\n"
    b=body.replace("\n{\n","\n{\n    __CPROVER_assert(0, \"REACHBODY\");\n",1) if reach else body
    open("zr.c","w").write(prefix+block+b)
    cc=subprocess.run(["goto-cc","-o","zr.goto","zr.c","--function","GetMatch"],capture_output=True,text=True)
    if cc.returncode: print(label,"CCFAIL",cc.stderr[-150:]); return
    subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
    subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch","--depth","200"],capture_output=True,text=True)
    v=[l for l in r.stdout.splitlines() if "VERIFICATION" in l]
    reachst="REACHBODY: FAILURE" in r.stdout
    print("%-40s %s reach=%s"%(label,v,reachst))

SOe='__CPROVER_requires(__CPROVER_same_object(scan, end))'
SOm='__CPROVER_requires(__CPROVER_same_object(scan, match))'
SOs='__CPROVER_requires(__CPROVER_same_object(scan, safe_end))'
verify([SOe,SOm], "SO(end)+SO(match)")
verify([SOe,SOm,SOs], "SO(end)+SO(match)+SO(safe_end)")
