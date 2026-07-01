import subprocess
base=open("zopfli.c").read()
sig='''static const unsigned char *GetMatch(const unsigned char *scan,
                                     const unsigned char *match,
                                     const unsigned char *end,
                                     const unsigned char *safe_end)
'''
idx=base.index(sig)+len(sig)
# body brace: first "\n{\n" after idx
bpos=base.index("\n{\n", idx)
prefix=base[:idx]
body=base[bpos:]  # "\n{\n..."
# inject reach assert after the body brace
body_inj=body.replace("\n{\n","\n{\n    __CPROVER_assert(0, \"REACHBODY\");\n",1)

def run(reqs,label):
    block="".join(r+"\n" for r in reqs)+"__CPROVER_assigns()\n"
    open("zr.c","w").write(prefix+block+body_inj)
    cc=subprocess.run(["goto-cc","-o","zr.goto","zr.c","--function","GetMatch"],capture_output=True,text=True)
    if cc.returncode!=0:
        print("%-45s CCFAIL %s"%(label,cc.stderr[-120:])); return
    subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
    subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch","--depth","200"],capture_output=True,text=True)
    reach="REACHBODY: FAILURE" in r.stdout
    print("%-45s reachable=%s"%(label,reach))

NN=['__CPROVER_requires(scan != NULL)','__CPROVER_requires(end != NULL)']
SO='__CPROVER_requires(__CPROVER_same_object(scan, end))'
LE='__CPROVER_requires(scan <= end)'
SE='__CPROVER_requires(safe_end == end - 8)'
RS='__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))'
RM='__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))'

run([], "EMPTY")
run(NN, "NN")
run(NN+[SO], "NN+SO")
run(NN+[SO,LE], "NN+SO+LE")
run(NN+[SO,LE,RS], "NN+SO+LE+RS")
run(NN+[SO,LE,RS,RM], "NN+SO+LE+RS+RM")
run(NN+[SO,LE,SE], "NN+SO+LE+SE")
run(NN+[SO,LE,SE,RS,RM], "NN+SO+LE+SE+RS+RM (v3)")

print("---more---")
RS1='__CPROVER_requires(__CPROVER_r_ok(scan, 1))'
SOm='__CPROVER_requires(__CPROVER_same_object(scan, match))'
run([RS1], "r_ok(scan,1)")
run([RS1, SO], "r_ok(scan,1)+SO(scan,end)")
run([SO], "SO(scan,end) alone")
run([SOm], "SO(scan,match) alone")
run([RS], "RS(scan,end-scan) alone")
