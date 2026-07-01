import sys, os, subprocess
SRC=open('zopfli.c').read().split('\n')
# Contract region: lines 2819..2857 (1-based), i.e. index 2818..2856 inclusive.
SIG_END=2818  # line "                                     const unsigned char *safe_end)"
BODY='{'
# find body brace line index (line 2858 -> idx 2857)
assert SRC[2817].strip().endswith('*safe_end)'), SRC[2817]
assert SRC[2857].strip()=='{', repr(SRC[2857])
pre=SRC[:2818]      # up to and including signature line (idx 2817)
post=SRC[2857:]     # from body brace onward

FALSE_ENS='__CPROVER_ensures(__CPROVER_POINTER_OFFSET(__CPROVER_return_value) < __CPROVER_POINTER_OFFSET(__CPROVER_old(scan)))'

def run(contract_lines, with_false=True):
    body=list(contract_lines)
    body.append('__CPROVER_assigns()')
    if with_false:
        body.append(FALSE_ENS)
    txt='\n'.join(pre+body+post)
    open('_b.c','w').write(txt)
    for cmd in (["goto-cc","-o","_b.goto","_b.c","--function","GetMatch"],
                ["goto-instrument","--partial-loops","--unwind","5","_b.goto","_b.goto"],
                ["goto-instrument","--enforce-contract","GetMatch","_b.goto","_bc.goto"]):
        p=subprocess.run(cmd,capture_output=True,text=True)
        if p.returncode!=0:
            return "BUILDFAIL:"+(p.stdout+p.stderr)[-200:]
    r=subprocess.run(["cbmc","_bc.goto","--function","GetMatch","--depth","200"],capture_output=True,text=True)
    ok="VERIFICATION SUCCESSFUL" in r.stdout
    it=''
    for ln in r.stdout.splitlines():
        if 'iterations)' in ln: it=ln.strip()
    # with false ensures: SUCCESSFUL => vacuous; FAILED => non-vacuous (good)
    return ('VACUOUS' if ok else 'NON-VACUOUS')+' | '+it

REQ_SETS={
 'same+off': ['__CPROVER_requires(__CPROVER_same_object(scan, end))',
              '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))'],
 '+safeend': ['__CPROVER_requires(__CPROVER_same_object(scan, end))',
              '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
              '__CPROVER_requires(safe_end == end - sizeof(size_t))'],
 '+rok_scan': ['__CPROVER_requires(__CPROVER_same_object(scan, end))',
              '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
              '__CPROVER_requires(safe_end == end - sizeof(size_t))',
              '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))'],
 '+rok_match(full)': ['__CPROVER_requires(__CPROVER_same_object(scan, end))',
              '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
              '__CPROVER_requires(safe_end == end - sizeof(size_t))',
              '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
              '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))'],
}
for name,reqs in REQ_SETS.items():
    print('%-20s -> %s'%(name, run(reqs)))

print("\n=== safe_end formulation variants (with full r_ok, false ensures) ===")
base=['__CPROVER_requires(__CPROVER_same_object(scan, end))',
      '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))']
rok=['__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
     '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))']
VAR={
 'eq_literal_8': ['__CPROVER_requires(safe_end == end - 8)'],
 'offset_eq': ['__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))'],
 'offset_le': ['__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) <= __CPROVER_POINTER_OFFSET(end))'],
 'none(safe_end unconstrained)': [],
}
for name,extra in VAR.items():
    print('%-30s -> %s'%(name, run(base+extra+rok)))

print("\n=== isolate safe_end constraint ===")
ISO={
 'only safe_end==end-8': ['__CPROVER_requires(safe_end == end - 8)'],
 'only same_object(safe_end,end)': ['__CPROVER_requires(__CPROVER_same_object(safe_end, end))'],
 'safe_end==end-8 + rok(end-8)': ['__CPROVER_requires(safe_end == end - 8)',
     '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))'],
 'empty (no requires)': [],
}
for name,reqs in ISO.items():
    print('%-34s -> %s'%(name, run(reqs)))

print("\n=== pointer offset sign probe (false ensures) ===")
for expr in ['safe_end == end - 0','safe_end == end + 8','safe_end == end - 1','safe_end == end - 8',
             'safe_end == scan - 8','safe_end == safe_end']:
    print('%-26s -> %s'%(expr, run(['__CPROVER_requires(%s)'%expr])))

print("\n=== offset pinning probe (false ensures) ===")
T={
 'offset(end)>=8 only': ['__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= 8)'],
 'offset(scan)>=8 only': ['__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) >= 8)'],
 'rok(scan,end-scan)+offset(end)>=8': [
     '__CPROVER_requires(__CPROVER_same_object(scan, end))',
     '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
     '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
     '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= 8)'],
 'full + offset(end)>=8 + safe_end le': [
     '__CPROVER_requires(__CPROVER_same_object(scan, end))',
     '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
     '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
     '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
     '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))',
     '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
     '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))'],
}
for name,reqs in T.items():
    print('%-36s -> %s'%(name, run(reqs)))

print("\n=== incremental from working base G ===")
G=['__CPROVER_requires(__CPROVER_same_object(scan, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))']
STEPS={
 'G':[],
 'G+rok_match':['__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))'],
 'G+so(safe_end,end)':['__CPROVER_requires(__CPROVER_same_object(safe_end, end))'],
 'G+so+off_le':['__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
     '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) <= __CPROVER_POINTER_OFFSET(end))'],
 'G+so+off_eq':['__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
     '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))'],
}
for name,extra in STEPS.items():
    print('%-22s -> %s'%(name, run(G+extra)))

print("\n=== full candidate (sound, non-vacuous?) ===")
FULL=['__CPROVER_requires(__CPROVER_same_object(scan, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
   '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))']
print('FULL eq -> %s'%run(FULL))
# Now WITHOUT false ensures: does the body verify (memory-safe, no real failures)?
print('FULL eq, no-false -> %s'%run(FULL, with_false=False))

print("\n=== match-readability variants (with safe_end eq + offset(end)>=8) ===")
CORE=['__CPROVER_requires(__CPROVER_same_object(scan, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
   '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))']
V={
 'is_fresh(match,end-scan)':['__CPROVER_requires(__CPROVER_is_fresh(match, (size_t)(end - scan)))'],
 'r_ok(match, offset(end)-offset(scan))':['__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(__CPROVER_POINTER_OFFSET(end) - __CPROVER_POINTER_OFFSET(scan))))'],
 'no match req':[],
}
for name,extra in V.items():
    print('%-40s -> %s'%(name, run(CORE+extra)))
    print('%-40s    no-false -> %s'%(name, run(CORE+extra, with_false=False)))

print("\n=== ordered full sets ===")
ORD={
 'A: rok first, then offsets': [
   '__CPROVER_requires(__CPROVER_same_object(scan, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
   '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))'],
 'B: drop offset(end)>=8, le form': [
   '__CPROVER_requires(__CPROVER_same_object(scan, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))'],
}
for name,reqs in ORD.items():
    print('%-32s false-> %s'%(name, run(reqs)))
    print('%-32s sound-> %s'%(name, run(reqs, with_false=False)))

print("\n=== add non-null ===")
A2=['__CPROVER_requires(__CPROVER_same_object(scan, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
   '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(scan != NULL)',
   '__CPROVER_requires(safe_end != NULL)']
print('A2 false-> %s'%run(A2))
print('A2 sound-> %s'%run(A2, with_false=False))

print("\n=== fix NULL-on-subtraction ===")
C1=['__CPROVER_requires(scan != NULL)',
    '__CPROVER_requires(end != NULL)',
    '__CPROVER_requires(safe_end != NULL)',
    '__CPROVER_requires(__CPROVER_same_object(scan, end))',
    '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
    '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
    '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))',
    '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
    '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
    '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))']
print('C1 (nonnull first) false-> %s'%run(C1))
print('C1 sound-> %s'%run(C1, with_false=False))

C2=['__CPROVER_requires(__CPROVER_same_object(scan, end))',
    '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
    '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(__CPROVER_POINTER_OFFSET(end) - __CPROVER_POINTER_OFFSET(scan))))',
    '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(__CPROVER_POINTER_OFFSET(end) - __CPROVER_POINTER_OFFSET(scan))))',
    '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
    '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
    '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))']
print('C2 (offset-len) false-> %s'%run(C2))
print('C2 sound-> %s'%run(C2, with_false=False))

print("\n=== D: const r_ok first to establish non-null objects ===")
D=['__CPROVER_requires(__CPROVER_r_ok(scan, sizeof(size_t)))',
   '__CPROVER_requires(__CPROVER_r_ok(match, sizeof(size_t)))',
   '__CPROVER_requires(__CPROVER_same_object(scan, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan) <= __CPROVER_POINTER_OFFSET(end))',
   '__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_r_ok(match, (size_t)(end - scan)))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(end) >= (long)sizeof(size_t))',
   '__CPROVER_requires(__CPROVER_same_object(safe_end, end))',
   '__CPROVER_requires(__CPROVER_POINTER_OFFSET(safe_end) + (long)sizeof(size_t) == __CPROVER_POINTER_OFFSET(end))']
print('D false-> %s'%run(D))
print('D sound-> %s'%run(D, with_false=False))

print("\n=== why is nonnull-first vacuous? ===")
for name,reqs in {
 'end!=NULL only':['__CPROVER_requires(end != NULL)'],
 'scan!=NULL only':['__CPROVER_requires(scan != NULL)'],
 'scan!=NULL + r_ok(scan,end-scan)':['__CPROVER_requires(scan != NULL)','__CPROVER_requires(__CPROVER_same_object(scan,end))','__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan)<=__CPROVER_POINTER_OFFSET(end))','__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))'],
 'end!=NULL + r_ok(scan,end-scan)':['__CPROVER_requires(end != NULL)','__CPROVER_requires(__CPROVER_same_object(scan,end))','__CPROVER_requires(__CPROVER_POINTER_OFFSET(scan)<=__CPROVER_POINTER_OFFSET(end))','__CPROVER_requires(__CPROVER_r_ok(scan, (size_t)(end - scan)))'],
}.items():
    print('%-36s -> %s'%(name, run(reqs)))
