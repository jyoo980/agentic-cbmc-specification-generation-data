import subprocess
base=open("zopfli.c").read()
sig='''static const unsigned char *GetMatch(const unsigned char *scan,
                                     const unsigned char *match,
                                     const unsigned char *end,
                                     const unsigned char *safe_end)
'''
idx=base.index(sig)+len(sig); bpos=base.index("\n{\n", idx)
prefix=base[:idx]; body=base[bpos:]
def verify(reqs,ens,label,reach=True):
    block="".join(r+"\n" for r in reqs)+"__CPROVER_assigns()\n"+"".join(e+"\n" for e in ens)
    b=body.replace("\n{\n","\n{\n    __CPROVER_assert(0, \"REACHBODY\");\n",1) if reach else body
    open("zr.c","w").write(prefix+block+b)
    cc=subprocess.run(["goto-cc","-o","zr.goto","zr.c","--function","GetMatch"],capture_output=True,text=True)
    if cc.returncode: print(label,"CCFAIL",cc.stderr[-150:]); return
    subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
    subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch","--depth","200"],capture_output=True,text=True)
    v=[l for l in r.stdout.splitlines() if "VERIFICATION" in l]
    reachst=("REACHBODY: FAILURE" in r.stdout) if reach else "n/a"
    fails=[l for l in r.stdout.splitlines() if l.strip().endswith("FAILURE") and "GetMatch" in l and "REACHBODY" not in l]
    print("%-25s %-22s reach=%s fails=%d"%(label,v[0] if v else "?",reachst,len(fails)))
    for f in fails[:8]: print("      ",f.strip())

OFS0='__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) == 0)'
RSW='__CPROVER_requires(__CPROVER_r_ok(scan, __CPROVER_OBJECT_SIZE(scan)))'
ENDDEF='__CPROVER_requires(end == scan + __CPROVER_OBJECT_SIZE(scan))'
SE='__CPROVER_requires(safe_end == end - 8)'
RMW='__CPROVER_requires(__CPROVER_r_ok(match, __CPROVER_OBJECT_SIZE(scan)))'
verify([OFS0,RSW,ENDDEF,SE,RMW], [], "derived-end")
verify([OFS0,RSW,ENDDEF,SE,RMW], [], "derived-end (noreach,verify)", reach=False)
