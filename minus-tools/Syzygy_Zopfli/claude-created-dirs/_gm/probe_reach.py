import subprocess, sys, textwrap
base=open("zopfli.c").read()
# extract everything; we will rebuild the contract block
# find signature end and body start
sig='''static const unsigned char *GetMatch(const unsigned char *scan,
                                     const unsigned char *match,
                                     const unsigned char *end,
                                     const unsigned char *safe_end)
'''
assert sig in base
# locate the existing contract block up to "{\n"
import re
idx=base.index(sig)+len(sig)
body_idx=base.index("\n{\n", idx)  # start of "{"
contract_old=base[idx:body_idx+1]  # includes up to "{" ? 
# Actually we want to replace from idx up to and including the "{" line
brace_pos=base.index("{", idx)
prefix=base[:idx]
suffix=base[brace_pos:]  # starts at "{"

def run(reqs, label):
    block="".join(r+"\n" for r in reqs)
    block+="__CPROVER_assigns()\n"
    # add a body reach assert right after "{"
    newsuffix=suffix.replace("{","{\n    __CPROVER_assert(0, \"REACH\");",1)
    src=prefix+block+newsuffix
    open("zr.c","w").write(src)
    subprocess.run(["goto-cc","-o","zr.goto","zr.c","--function","GetMatch"],capture_output=True)
    subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
    subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch","--depth","200"],capture_output=True,text=True)
    reach = "REACH: FAILURE" in r.stdout
    print("%-40s body-reachable=%s" % (label, reach))

R_nn=['__CPROVER_requires(scan != NULL)','__CPROVER_requires(end != NULL)']
R_so='__CPROVER_requires(__CPROVER_same_object(scan, end))'
R_le='__CPROVER_requires(scan <= end)'
R_se='__CPROVER_requires(safe_end == end - 8)'
R_rs='__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))'
R_rm='__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))'
R_os_s='__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) == 0)'
R_sz_s='__CPROVER_requires(__CPROVER_OBJECT_SIZE(scan) == (size_t)(end - scan))'

run(R_nn+[R_so,R_le,R_se,R_rs,R_rm], "v3 (no objsize)")
run(R_nn+[R_so,R_le,R_rs,R_rm], "v3 minus safe_end==end-8")
run(R_nn+[R_so,R_le,R_se,R_rs,R_rm,R_os_s,R_sz_s], "with scan objsize/offset")
run(R_nn+[R_so,R_le,R_rs], "minimal: nn+so+le+rok_scan")
run(R_nn+[R_so,R_le], "nn+so+le only")
