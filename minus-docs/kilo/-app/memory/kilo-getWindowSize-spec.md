---
name: kilo-getwindowsize-spec
description: getWindowSize verifies vacuous (0/7); reqs now w_ok (was is_fresh) + bounded ensures rv==0=>dims in [0,0xffff] — that bound is what [[kilo-updateWindowSize-spec]] assumes to kill its -=2 overflow. ioctl/snprintf/strlen stubs dodge --add-library variadic crash.
metadata: 
  node_type: memory
  type: project
  originSessionId: 47985d35-b116-4eab-a786-b1b39030c7e0
---

`getWindowSize` in /app/kilo/kilo.c verifies but is VACUOUS (kill 0/7), confirmed via +12345 false-ensures probe.

Spec (UPDATED 2026-06): `w_ok(rows)+w_ok(cols)+assigns(*rows,*cols)+ensures(rv==0||rv==-1)+ensures(rv==0 ==> 0<=*rows<=0xffff && 0<=*cols<=0xffff)`. Switched is_fresh→w_ok so the caller [[kilo-updateWindowSize-spec]] (passes global `&E.screenrows`) satisfies the replaced-callee precondition assertion. Added the [0,0xffff] bound (legit: dims are unsigned-short ws_row/ws_col) — proven vacuously here, but ASSUMED at updateWindowSize's callsite to prove `E.screenrows-2` doesn't overflow. Still 0/7 (vacuous), bound text is the real spec.

Two obstacles:
1. nondet_callees ioctl/snprintf/strlen made avocado take the `--add-library` path, whose **variadic-aware** library model crashed `goto-instrument --enforce-contract` in `create_car_expr` (instrument_spec_assigns.cpp, "Unreachable"). Fix: added new plain nondet stubs `/app/stubs/{ioctl,snprintf,strlen}.c`. None have contracts, so they're ordinary calls (NOT the variadic-replacement crash of editorSetStatusMessage). snprintf NUL-terminates str[0]; ioctl returns {0,-1} and leaves ws untouched (uninit local → nondet fields). This is a different crash class than [[kilo-editorFind-variadic-block]].
2. Vacuity cause: the REPLACED [[kilo-getCursorPosition-spec]] contract's is_fresh requires → `__CPROVER_replace_ensures_is_fresh: malloc not declared` → UNSAT poison. Same family as the rest of kilo. Unavoidable; both getCursorPosition callsites are replaced (avocado auto-replaces callees with contracts), so the poison can't be dodged from getWindowSize's side.

0/7 kill (relational/logical mutants on the ioctl/getCursorPosition guard logic) cannot be improved — vacuous, not a weak spec.
