import subprocess
SRC=open('zopfli.c').read()
START='// clang-format off\n__CPROVER_requires((bit == 0'
END='// clang-format on\n{\n    if (*bp == 0)'
pre=SRC[:SRC.index(START)]
post=SRC[SRC.index(END)+len('// clang-format on'):]
def test(name, contract):
    open('p.c','w').write(pre+'// clang-format off\n'+contract+'\n// clang-format on'+post)
    for cmd in (["goto-cc","-o","p.goto","p.c","--function","AddBit"],
                ["goto-instrument","--partial-loops","--unwind","5","p.goto","p.goto"],
                ["goto-instrument","--enforce-contract","AddBit","p.goto","cp.goto"]):
        subprocess.run(cmd,capture_output=True)
    r=subprocess.run(["cbmc","cp.goto","--function","AddBit","--depth","200"],capture_output=True,text=True)
    print("%-22s %s" % (name, "PASS" if "VERIFICATION SUCCESSFUL" in r.stdout else "FAIL"))
REQ=('__CPROVER_requires((bit == 0 || bit == 1) &&\n'
 '    __CPROVER_is_fresh(bp, sizeof(*bp)) && *bp != 0 && *bp <= 7 &&\n'
 '    __CPROVER_is_fresh(outsize, sizeof(*outsize)) && *outsize >= 1 && *outsize <= 8 &&\n'
 '    __CPROVER_is_fresh(out, sizeof(*out)) &&\n'
 '    __CPROVER_is_fresh(*out, *outsize))\n')
ASG='__CPROVER_assigns(*bp, (*out)[*outsize - 1])\n'
# false bp ensures
test("false-bp-ensures", REQ+ASG+'__CPROVER_ensures(*bp == ((__CPROVER_old(*bp) + 1) & 7) + 5)\n')
# true bp ensures only
test("true-bp-ensures", REQ+ASG+'__CPROVER_ensures(*bp == ((__CPROVER_old(*bp) + 1) & 7))\n')
print("--- concrete (no old) ---")
test("bp==100 (false)", REQ+ASG+'__CPROVER_ensures(*bp == 100)\n')
test("bp<=7 (true?)",   REQ+ASG+'__CPROVER_ensures(*bp <= 7)\n')
test("bp>=1 (false:can be 0)", REQ+ASG+'__CPROVER_ensures(*bp >= 1)\n')
print("--- outsize-elem false ---")
test("buf-false", REQ+ASG+'__CPROVER_ensures((*out)[*outsize-1] == 123)\n')
