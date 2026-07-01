import subprocess
base=open("zopfli.c").read()
sig='''static const unsigned char *GetMatch(const unsigned char *scan,
                                     const unsigned char *match,
                                     const unsigned char *end,
                                     const unsigned char *safe_end)
'''
idx=base.index(sig)+len(sig)
brace_pos=base.index("{", idx)
prefix=base[:idx]; suffix=base[brace_pos:]
reqs=['__CPROVER_requires(scan != NULL)','__CPROVER_requires(end != NULL)',
      '__CPROVER_requires(__CPROVER_same_object(scan, end))','__CPROVER_requires(scan <= end)']
block="".join(r+"\n" for r in reqs)+"__CPROVER_assigns()\n"
newsuffix=suffix.replace("{","{\n    __CPROVER_assert(0, \"REACH\");",1)
open("zr.c","w").write(prefix+block+newsuffix)
subprocess.run(["goto-cc","-o","zr.goto","zr.c","--function","GetMatch"],capture_output=True)
subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
for args in (["--depth","200"],[]):
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch"]+args,capture_output=True,text=True)
    print("args",args)
    for ln in r.stdout.splitlines():
        if "REACH" in ln or "VERIFICATION" in ln: print("   ",ln)
