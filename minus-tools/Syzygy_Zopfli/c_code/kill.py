import subprocess, sys
SRC=open('zopfli.c').read()
START='// clang-format off\n__CPROVER_requires((bit == 0'
END='// clang-format on\n{\n    if (*bp == 0)'
pre=SRC[:SRC.index(START)]; post=SRC[SRC.index(END)+len('// clang-format on'):]
MUT={
 'orig': SRC,
 'mut-if':  None,'mut-dec':None,'mut-oob':None}
def build_src(contract):
    return pre+'// clang-format off\n'+contract+'\n// clang-format on'+post
def run(src,fn='AddBit'):
    open('k.c','w').write(src)
    for cmd in (["goto-cc","-o","k.goto","k.c","--function",fn],
                ["goto-instrument","--partial-loops","--unwind","5","k.goto","k.goto"],
                ["goto-instrument","--enforce-contract",fn,"k.goto","ck.goto"]):
        p=subprocess.run(cmd,capture_output=True,text=True)
    r=subprocess.run(["cbmc","ck.goto","--function",fn,"--depth","200"],capture_output=True,text=True)
    return "VERIFICATION SUCCESSFUL" in r.stdout
def evaluate(contract):
    base=build_src(contract)
    res={}
    res['orig']= 'PASS' if run(base) else 'FAIL'
    muts=[('if','\n    if (*bp == 0)','\n    if (*bp != 0)'),
          ('dec','\n    *bp = (*bp + 1) & 7;','\n    *bp = (*bp - 1) & 7;'),
          ('oob','\n    (*out)[*outsize - 1] |= bit << *bp;','\n    (*out)[*outsize + 1] |= bit << *bp;')]
    for n,o,m in muts:
        assert base.count(o)==1,(n,o)
        res[n]= 'KILLED' if not run(base.replace(o,m)) else 'survived'
    return res

if __name__=='__main__':
    contract=open('contract.txt').read()
    print(evaluate(contract))
