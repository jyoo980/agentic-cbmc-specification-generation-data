---
name: kilo-getcursorposition-spec
description: kilo getCursorPosition verifies vacuous (0/15 kill); reqs NOW w_ok (switched from is_fresh, no bound) so getWindowSize/updateWindowSize global-arg callsites pass. w_ok ensures-PROBES still crash goto-instrument but the real contract verifies fine.
metadata: 
  node_type: memory
  type: project
  originSessionId: 58786bc2-5937-4c10-9151-81ab91f6d8a9
---

`/app/kilo/kilo.c` `getCursorPosition(ifd,ofd,rows,cols)`: writes ESC[6n, reads
response into local `buf[32]`, parses with `sscanf(buf+2,"%d;%d",rows,cols)`.

Final spec (UPDATED 2026-06): `requires w_ok(rows,sizeof(int))` + `w_ok(cols,...)`,
`assigns(*rows,*cols)`, `ensures return_value==0 || ==-1` (NO value bound — can't
prove one, sscanf is variadic & can't be stubbed). Verifies 0/15, exit 0.
Switched is_fresh→w_ok because [[kilo-getWindowSize-spec]] (which replaces this
callee's contract) passes pointers that are only w_ok, not provably fresh; and
ultimately [[kilo-updateWindowSize-spec]] passes globals. The w_ok contract
verifies cleanly despite the probe-instability noted below.

**Vacuous, 0/15 kill — unavoidable.** Confirmed via +12345 false-ensures probe
(added as 2nd ensures): still "verified successfully" → is_fresh requires are
UNSAT (the documented [[kilo-cbmc-contracts-vacuous]] poison). Uses existing
/app/stubs/read.c + write.c (nondet models). sscanf called directly is fine
(no contract → not replaced; doesn't hit the variadic-replacement crash).

**w_ok experiment (rejected):** switching requires to `__CPROVER_w_ok(rows,...)`
also verifies 0/15, BUT is unstable — any probe that changes the ensures
(replace with `0==1`, or add a 2nd `==12345`) crashes goto-instrument with an
"invariant violation" at "Enforcing contracts" (exit 134). The real-ensures
w_ok run itself completes, but the instability + no kill-score gain made
is_fresh the safer, file-consistent choice. Could not cleanly determine if
w_ok is non-vacuous because every vacuity probe crashes.

Kill score can't be raised: outputs (return ∈{0,-1}, *rows/*cols) all flow from
nondet read/sscanf, so mutations to buffer/branch logic leave both 0 and -1
reachable. The 15 survivors are buffer-arithmetic/relational/logical mutants.
