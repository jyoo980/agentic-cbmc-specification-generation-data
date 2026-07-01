import subprocess
SRC=open('zopfli.c').read()
START='// clang-format off\n__CPROVER_requires((bit == 0'
END='// clang-format on\n{\n    if (*bp == 0)'
pre=SRC[:SRC.index(START)]; post=SRC[SRC.index(END)+len('// clang-format on'):]
REQ=('__CPROVER_requires((bit == 0 || bit == 1) &&\n    __CPROVER_is_fresh(bp, sizeof(*bp)) && *bp != 0 && *bp <= 7 &&\n    __CPROVER_is_fresh(outsize, sizeof(*outsize)) && *outsize >= 1 && *outsize <= 8 &&\n    __CPROVER_is_fresh(out, sizeof(*out)) &&\n    __CPROVER_is_fresh(*out, *outsize))\n')
def test(name, asg, ens):
    open('p.c','w').write(pre+'// clang-format off\n'+REQ+asg+ens+'// clang-format on'+post)
    for cmd in (["goto-cc","-o","p.goto","p.c","--function","AddBit"],
                ["goto-instrument","--partial-loops","--unwind","5","p.goto","p.goto"],
                ["goto-instrument","--enforce-contract","AddBit","p.goto","cp.goto"]):
        subprocess.run(cmd,capture_output=True)
    r=subprocess.run(["cbmc","cp.goto","--function","AddBit","--depth","200"],capture_output=True,text=True)
    print("%-40s %s" % (name, "PASS" if "VERIFICATION SUCCESSFUL" in r.stdout else "FAIL"))
# add *outsize to assigns, false ensures on outsize -> if PASS, assigns-makes-vacuous confirmed
test("outsize in assigns + false outsize-ensures",
     '__CPROVER_assigns(*bp, *outsize, (*out)[*outsize - 1])\n',
     '__CPROVER_ensures(*outsize == __CPROVER_old(*outsize) + 100)\n')
# outsize NOT in assigns, false ensures -> should FAIL (control)
test("outsize NOT in assigns + false outsize-ensures (control)",
     '__CPROVER_assigns(*bp, (*out)[*outsize - 1])\n',
     '__CPROVER_ensures(*outsize == __CPROVER_old(*outsize) + 100)\n')
print("--- vacuity test: literal false ensures with valid assigns ---")
test("ensures(1==0)", '__CPROVER_assigns(*bp, (*out)[*outsize - 1])\n', '__CPROVER_ensures(1 == 0)\n')
