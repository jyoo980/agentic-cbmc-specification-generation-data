import subprocess
base=open("zopfli.c").read()
marker='__CPROVER_old(scan)[k] == __CPROVER_old(match)[k]) })\n{\n'
assert base.count(marker)==1, base.count(marker)
inj=base.replace(marker, marker[:-1]+'    __CPROVER_assert(0, "REACHBODY");\n',1)
open("zr.c","w").write(inj)
cc=subprocess.run(["goto-cc","-o","zr.goto","zr.c","--function","GetMatch"],capture_output=True,text=True)
print("CC rc",cc.returncode, cc.stderr[-200:])
subprocess.run(["goto-instrument","--partial-loops","--unwind","5","zr.goto","zr.goto"],capture_output=True)
subprocess.run(["goto-instrument","--enforce-contract","GetMatch","zr.goto","zrc.goto"],capture_output=True)
for args in (["--depth","200"],["--depth","1000"]):
    r=subprocess.run(["cbmc","zrc.goto","--function","GetMatch"]+args,capture_output=True,text=True)
    line=[l for l in r.stdout.splitlines() if "REACHBODY" in l]
    v=[l for l in r.stdout.splitlines() if "VERIFICATION" in l]
    print(args, line, v)
