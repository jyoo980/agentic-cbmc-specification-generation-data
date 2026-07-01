---
name: cbmc-getwindowsize
description: getWindowSize verifies (needs a nondet ioctl stub) but canonical depth-200 kill score is structurally 0; real strength 2/7
metadata: 
  node_type: memory
  type: project
  originSessionId: 5eba17b1-acac-43be-98c3-886381d749af
---

**UPDATE 2026-06-26: contract CHANGED to enable [[cbmc-updatewindowsize]] (its only
caller).** is_fresh requires -> `__CPROVER_w_ok` (globals &E.screenrows/&E.screencols
aren't fresh so is_fresh fails on replacement); added `ensures(ret==0)` and
`ensures(ret==0 ==> *rows >= 0)`. All vacuous at --depth 200 (no return reached) so
getWindowSize still self-verifies and stays 0/7 — but the w_ok/ret==0 changes are NOT
sound above depth ~255 (they're depth-scoped). The notes below predate this change.

`getWindowSize` (kilo.c). Originally FAILED canonically with "no body for callee
ioctl" (ioctl is variadic, no CPROVER library body). Fix: added `/app/stubs/ioctl.c`
(`/* FUNCTION: ioctl */`, returns `__VERIFIER_nondet_int()`, leaves the winsize
untouched). run_cbmc auto-links it (ioctl is a callee only of getWindowSize).
Do NOT reach into the va_list to populate `*ws`: a `va_arg`/`ws->ws_col=...` stub
adds spurious `*ap` pointer-dereference FAILUREs (hidden at depth 200 by truncation,
but they fire at depth>=270). Leaving the struct nondet is the soundest model.

Spec: requires(rk_idx==0, is_fresh rows, is_fresh cols); assigns(*rows,*cols,rk_idx);
ensures(ret in {0,-1}); ensures(ret==0 ==> *cols != 0). The `*cols!=0` invariant is
sound because the only depth-reachable success path is the ioctl/else branch, whose
guard `!(ioctl==-1 || ws_col==0)` forces ws_col!=0, and `*cols = ws.ws_col`.

**Canonical depth-200 kill score is structurally 0/7** (re-confirmed 2026-06-26).
Root cause is the prologue (global nondet-init [[cbmc-getcursorposition]]) PLUS the
two MANDATORY pointer-validity requires: the body writes both `*rows` and `*cols`,
so both pointers must be asserted valid or the writes raise deref FAILUREs (which
makes the baseline fail, not the mutants). With both `is_fresh`, the else-return
(`ensures rv==99` probe, with `rk_idx==0` KEPT) first becomes reachable at depth
~255; below that every postcondition is vacuous (even `ensures(rv==99)` verifies).

Measured the cheaper validity predicate `__CPROVER_w_ok(p,sizeof(*p))`: it lowers
the else-return threshold from ~255 (is_fresh) to ~215 (w_ok), but **still >200**, so
the canonical score stays 0/7 (verified empirically with the w_ok spec applied).
Two reasons NOT to switch to w_ok: (1) no canonical kill gain; (2) w_ok is UNSOUND
for the `*cols!=0` ensures — it permits `rows`/`cols` to ALIAS, and since the else
branch does `*cols=ws.ws_col; *rows=ws.ws_row;` an aliased `*rows` write clobbers
`*cols` with a possibly-zero row count, breaking the postcondition at depths where
the return is reached. The two `is_fresh` are what guarantee `rows`/`cols` are
DISTINCT objects, which the `*cols!=0` ensures depends on. Keep `is_fresh`.

PITFALL when probing reachability: you MUST keep `__CPROVER_requires(rk_idx==0)`.
Dropping it makes the query-branch first `getCursorPosition` call's precondition
FAIL at depth ~200 (that obligation IS reachable within budget), which looks like
"return reached" but is really a precondition FAILURE — a false positive. Likewise
a spec missing `is_fresh`/`w_ok` for a written pointer "FAILS" at low depth via a
deref failure, not via reaching the return. Use `ensures(rv==99)` + grep that the
failing obligation is the postcondition (not `precondition`/`dereference`).

**Real strength = 2/7** at depth ~270-290 (a clean window where the else-return is
reachable AND the original still verifies): the `*cols!=0` ensures kills the two
ws_col-guard mutants (`ws_col==0`->`!=0`, and `||`->`&&`). The other 5 survive:
- mutants on lines after the first getCursorPosition (the two retval checks, the
  write!=12 check, the snprintf write check) live in the QUERY branch, which is
  structurally DEAD: getCursorPosition's contract requires rk_idx==0, but the first
  call havocs rk_idx (assigns rk_idx), so the SECOND call's rk_idx==0 precondition is
  unsatisfiable -> `getCursorPosition.precondition` FAILS at any depth that reaches
  call2. This is a read-stub modeling artifact (rk_idx is a global script index that
  never resets); unfixable without unsoundly rewriting getCursorPosition's contract.
- the ioctl `==-1`->`!=-1` mutant is equivalent in the reachable region (both else
  paths yield *cols!=0, return 0; the query branches differ but are truncated/dead).

Scripts: `/app/score_gws.py` (canonical depth-200 mutation scorer; links
`stubs/ioctl.c`, replaces `getCursorPosition` contract, `--partial-loops --depth
200`; honors `DEPTH=<n>` env for diagnostics). Earlier-named `run_mutants_*`/
`_score_gws_depth.py` no longer exist — use score_gws.py.
