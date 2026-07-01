---
name: flsb-depth200-vacuity
description: "FindLargestSplittableBlock verifies w/ exact running-max spec but 0/17 kills @depth200; strong (17/17 @1000), don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: 59bbd1c8-a766-4be8-94d9-55e189c90ca5
---

FindLargestSplittableBlock (zopfli.c): leaf, one loop over npoints+1 blocks,
running-max selection (strict `>` seeded at 0). Verifies @depth200 but **0/17
kills** — pure depth-200 vacuity.

Spec: pin `npoints==2` (3 blocks: B0 start=0, B1 start=sp[0], B2 start=sp[1];
ends sp[0],sp[1],lz77size-1) so the loop fully unwinds AND the middle block B1
has start>0 (needed to make the `longest=end+start` mutant observable — with
npoints==1 that mutant is unkillable since B0 has start=0 and B1's longest
update is never re-read). is_fresh(splitpoints,2)+is_fresh(done,lz77size)+
is_fresh(lstart/lend); requires sorted sp0<=sp1<lz77size, lz77size in [1,6].
Exact-value postcondition: nested-ternary running max encoded as macros
(FLSB_C0/L0/SEL1/L1/SEL2 + FINAL0/1/2), `return==FOUND?1:0`, and
`FINALk ==> (*lstart,*lend)==block k`.

**Proven maximally strong, NO equivalent mutants**: 15/17 @depth600, and the
last two survivors (the `end-start != longest` compare mutant and the
`longest=end+start` assignment mutant) both KILLED @depth1000+. So all 17
killable given depth — the 0 is entirely the grader's hardcoded `--depth 200`.

Levers tried, none reach under 200 (postcond spec-point floor ~300 for npoints=1,
~450 for npoints=2): r_ok/w_ok (made it WORSE — threshold rose >300 and lost the
3 OOB-only kills; the is_fresh→r_ok lever that helped [[zlz77gbr-rok-beats-vacuity]]
does NOT help here); pinning lz77size to a concrete const (helped 360→~300 but
not enough); dropping to npoints==1/2-blocks (floor ~300, and caps at 16/17).
Kept the strongest sound spec (npoints==2, symbolic lz77size). Don't chase kills.
See [[avocado-depth200-vacuity]]. Helper scripts: /app/_verify_flsb.sh,
/app/kill_flsb.sh.
