#!/usr/bin/env python3
# Experiment: swap the CalculateBlockSymbolSizeGivenCounts contract block
# (lines 351..381 of the backup) with a chosen variant, then report
# original-verifies + mutant#1-killed at depth 200.
import subprocess, sys, os
BACK='/tmp/zopfli_backup.c'
SRC='zopfli.c'
F='CalculateBlockSymbolSizeGivenCounts'

base=open(BACK).read().split('\n')
# block is lines 351..381 inclusive (1-based) -> indices 350..380
pre=base[:350]
post=base[381:]   # from line 382 ('{') onward

def build(contract, mutate_if=False):
    lines=pre+contract+post
    if mutate_if:
        for i,l in enumerate(lines):
            if l=='    if (lstart + ZOPFLI_NUM_LL * 3 > lend)':
                lines[i]='    if (lstart + ZOPFLI_NUM_LL * 3 < lend)'
                break
    open(SRC,'w').write('\n'.join(lines))

def run(depth=200):
    if subprocess.run(['goto-cc','-o','/tmp/m.goto',SRC,'--function',F],
                      capture_output=True).returncode!=0:
        return 'BUILDFAIL'
    subprocess.run(['goto-instrument','--partial-loops','--unwind','5','/tmp/m.goto','/tmp/m.goto'],capture_output=True)
    subprocess.run(['goto-instrument','--replace-call-with-contract','CalculateBlockSymbolSizeSmall',
        '--replace-call-with-contract','ZopfliGetLengthSymbolExtraBits',
        '--replace-call-with-contract','ZopfliGetDistSymbolExtraBits',
        '--enforce-contract',F,'/tmp/m.goto','/tmp/c.goto'],capture_output=True)
    out=subprocess.run(['cbmc','/tmp/c.goto','--function',F,'--depth',str(depth)],capture_output=True,text=True).stdout
    ba=[l for l in out.split('\n') if 'GivenCounts.assertion' in l]
    if 'VERIFICATION SUCCESSFUL' in out: res='SUCCESS'
    elif 'VERIFICATION FAILED' in out: res='FAILED'
    else: res='OTHER'
    return res, ba

VARIANTS={}
VARIANTS['arrays4_ens']=[
'__CPROVER_requires(__CPROVER_is_fresh(ll_counts, ZOPFLI_NUM_LL * sizeof(*ll_counts)))',
'__CPROVER_requires(__CPROVER_is_fresh(d_counts, ZOPFLI_NUM_D * sizeof(*d_counts)))',
'__CPROVER_requires(__CPROVER_is_fresh(ll_lengths, ZOPFLI_NUM_LL * sizeof(*ll_lengths)))',
'__CPROVER_requires(__CPROVER_is_fresh(d_lengths, ZOPFLI_NUM_D * sizeof(*d_lengths)))',
'__CPROVER_requires(lstart <= lend)',
'__CPROVER_assigns()',
'__CPROVER_ensures(__CPROVER_return_value >= ll_lengths[256])',
]
VARIANTS['min_scalar']=[
'__CPROVER_requires(lstart <= lend)',
'__CPROVER_assigns()',
]
VARIANTS['two_fresh']=[
'__CPROVER_requires(__CPROVER_is_fresh(ll_counts, ZOPFLI_NUM_LL * sizeof(*ll_counts)))',
'__CPROVER_requires(__CPROVER_is_fresh(ll_lengths, ZOPFLI_NUM_LL * sizeof(*ll_lengths)))',
'__CPROVER_requires(lstart <= lend)',
'__CPROVER_assigns()',
]

name=sys.argv[1]
c=VARIANTS[name]
build(c,mutate_if=False)
print('ORIG :',run())
build(c,mutate_if=True)
print('MUT1 :',run())
# restore backup
open(SRC,'w').write(open(BACK).read())
