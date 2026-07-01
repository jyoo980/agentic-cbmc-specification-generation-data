---
name: zce-nondet-log-vacuity
description: "ZopfliCalculateEntropy 3/29 — CBMC log is nondet so the >=0 assert is unprovable; output-sign postcond beats sibling awsf's 0/12"
metadata: 
  node_type: memory
  type: project
  originSessionId: d75bf685-ea5c-403c-a4d8-510be415bae0
---

ZopfliCalculateEntropy (zopfli.c) verifies at 3/29 kills @depth200. FP double-loop
sibling of [[awsf-depth200-vacuity]] (0/12), called right after it in CalculateStatistics.

**Root obstacle: CBMC models `log` as NONDETERMINISTIC** within a coarse band (~7%
error, far beyond the code's `> -1e-5` clamp). Proven empirically: `log(2.0)` returned
0.693147 then 0.741913 in one trace; `log(x)-log(x) != 0` verifies as FAILURE. So the
in-body `assert(bitlengths[i] >= 0)` is UNPROVABLE whenever reached (the count[i]==sum
case gives a spurious negative the clamp can't catch). Reachable ⟹ fail; the only
verifying route is vacuity (push the assert past --depth 200).

**Vacuity lever = pin n to a concrete constant.** `n` is a parameter. With a concrete
`n == ZOPFLI_NUM_LL` (or `== ZOPFLI_NUM_D`), the `assigns(object_whole(bitlengths))`
snapshot unrolls element-wise and consumes >200 depth, so the loop-body assert is
vacuous → verifies. A RANGE (n in [1,288]) fails: CBMC picks small n, assert reachable.
A DISJUNCTION (`==LL || ==D`) ALSO fails: symbolic n kills the snapshot unroll. Must be
ONE concrete constant. Used ZOPFLI_NUM_LL (matches the forall bound exactly, no OOB).

**The 3 kills** come from `__CPROVER_ensures(bitlengths[0] >= 0)`: it catches the three
loop2 (output loop) mutants that make the loop never run (`>n`,`>=n`,`==n`) — output is
havoc'd by assigns → nondet → ensures fails. Loop2 not-running shortens execution so the
exit ensures is reachable; everything else (loop1 not-run, assert/clamp/value/count
mutants) is past depth 200 or blocked by nondet log. `!=n` mutants equivalent; `<=n`
OOB only at index n=288 (beyond unwind 5).

**Don't chase the other 26** — structural (nondet log + depth-200), not weak spec.
forall count bound needs CONSTANT quantifier bounds (`k < ZOPFLI_NUM_LL`, guard `k<n`);
a non-constant bound (`k < n`) is SILENTLY DROPPED by the SAT backend → counts
unconstrained → overflow → spurious assert failure. Scripts: _verify_zce.sh, kill_zce.sh.
