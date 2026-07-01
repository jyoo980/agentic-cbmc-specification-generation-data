import subprocess, re
SRC=open('zopfli.c').read()
# locate the contract block and replace it wholesale for each experiment
START='// clang-format off\n__CPROVER_requires(bit == 0 || bit == 1)'
END='// clang-format on\n{\n    if (*bp == 0)'
pre=SRC[:SRC.index(START)]
post=SRC[SRC.index(END)+len('// clang-format on'):]  # keep {... body

def test(name, contract):
    src=pre+'// clang-format off\n'+contract+'\n// clang-format on'+post
    open('p.c','w').write(src)
    for cmd in (["goto-cc","-o","p.goto","p.c","--function","AddBit"],
                ["goto-instrument","--partial-loops","--unwind","5","p.goto","p.goto"],
                ["goto-instrument","--enforce-contract","AddBit","p.goto","cp.goto"]):
        subprocess.run(cmd,capture_output=True)
    r=subprocess.run(["cbmc","cp.goto","--function","AddBit","--depth","200"],capture_output=True,text=True)
    vac = "VERIFICATION SUCCESSFUL" in r.stdout
    print("%-30s falseEnsuresPasses=%s -> %s" % (name, vac, "VACUOUS" if vac else "OK(non-vacuous)"))

FALSE='__CPROVER_ensures(*outsize == __CPROVER_old(*outsize) + 100)'
# Each variant: a set of requires + the FALSE ensures
variants = {
 "bp-only":
   '__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))\n__CPROVER_requires(*bp != 0 && *bp <= 7)\n__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))\n__CPROVER_requires(*outsize >= 1)\n'+FALSE,
 "+out-fresh":
   '__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))\n__CPROVER_requires(*bp != 0 && *bp <= 7)\n__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))\n__CPROVER_requires(*outsize >= 1)\n__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))\n'+FALSE,
 "+buf-fresh":
   '__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))\n__CPROVER_requires(*bp != 0 && *bp <= 7)\n__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))\n__CPROVER_requires(*outsize >= 1)\n__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))\n__CPROVER_requires(__CPROVER_is_fresh(*out, *outsize))\n'+FALSE,
}
for n,c in variants.items(): test(n,c)

FALSE='__CPROVER_ensures(*outsize == __CPROVER_old(*outsize) + 100)'
base='__CPROVER_requires(__CPROVER_is_fresh(bp, sizeof(*bp)))\n__CPROVER_requires(*bp != 0 && *bp <= 7)\n__CPROVER_requires(__CPROVER_is_fresh(outsize, sizeof(*outsize)))\n__CPROVER_requires(__CPROVER_is_fresh(out, sizeof(*out)))\n'
test("bounded<=8", base+'__CPROVER_requires(*outsize >= 1 && *outsize <= 8)\n__CPROVER_requires(__CPROVER_is_fresh(*out, *outsize))\n'+FALSE)
test("exact==1", base+'__CPROVER_requires(*outsize == 1)\n__CPROVER_requires(__CPROVER_is_fresh(*out, *outsize))\n'+FALSE)

FALSE='__CPROVER_ensures(*outsize == __CPROVER_old(*outsize) + 100)'
combined=('__CPROVER_requires((bit == 0 || bit == 1) &&\n'
 '    __CPROVER_is_fresh(bp, sizeof(*bp)) && *bp != 0 && *bp <= 7 &&\n'
 '    __CPROVER_is_fresh(outsize, sizeof(*outsize)) && *outsize >= 1 && *outsize <= 8 &&\n'
 '    __CPROVER_is_fresh(out, sizeof(*out)) &&\n'
 '    __CPROVER_is_fresh(*out, *outsize))\n')
test("combined-falseEnsures", combined+FALSE)
GOOD='__CPROVER_ensures(*outsize == __CPROVER_old(*outsize))'
test("combined-trueEnsures", combined+GOOD)

ASG='__CPROVER_assigns(*bp, (*out)[*outsize - 1])\n'
ENS=('__CPROVER_ensures(*bp == ((__CPROVER_old(*bp) + 1) & 7))\n'
 '__CPROVER_ensures(*outsize == __CPROVER_old(*outsize))\n'
 '__CPROVER_ensures(*out == __CPROVER_old(*out))\n'
 '__CPROVER_ensures((*out)[*outsize - 1] ==\n'
 '    (unsigned char)(__CPROVER_old((*out)[*outsize - 1]) | (bit << __CPROVER_old(*bp))))\n')
test("FULL", combined+ASG+ENS)
