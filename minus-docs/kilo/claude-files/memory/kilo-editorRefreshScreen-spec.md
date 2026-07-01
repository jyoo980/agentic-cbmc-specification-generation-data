---
name: kilo-editorrefreshscreen-spec
description: "Verifying editorRefreshScreen in /app/kilo/kilo.c: needed a new write stub AND had to switch abAppend/abFree requires off is_fresh; verifies vacuously (kill 0/95)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 83bff347-8a8f-4ed2-adf9-d450b60bee89
---

`editorRefreshScreen(void)` in `/app/kilo/kilo.c` verifies successfully (exit 0),
kill 0/95 (vacuous, see [[kilo-cbmc-contracts-vacuous]]). Contract: `E.numrows == 0`
+ non-negativity of rowoff/coloff/cx/cy/screenrows/screencols, `__CPROVER_assigns()`.
`numrows == 0` is the key: it makes `E.row` (and its per-row render/hl/chars
buffers, which a contract can't freshen elementwise) never dereferenced.

**THREE blockers, all needed together:**

1. **New `/app/stubs/write.c`** (mirrors [[kilo-editorReadKey-spec]]'s read.c).
   `editorRefreshScreen` calls `write` directly; the bundled
   `<builtin-library-write>` pipe model has internal symbols with no definite
   size, crashing `goto-instrument` assigns-frame instrumentation
   ("no definite size for lvalue target" / `:597`). Stub returns nondet in
   [-1,count]. avocado auto-resolves it via the call-graph external list.

2. **`is_fresh` in a REPLACED `requires` is unsupported by this CBMC build.**
   `editorRefreshScreen` calls abAppend/abFree, whose contracts get
   `--replace-call-with-contract`. Their original `requires` used
   `__CPROVER_is_fresh`; replacing them makes goto-instrument emit
   "no body for function '__CPROVER_replace_requires_is_fresh'" then ABORT
   (invariant violation `create_car_expr`, instrument_spec_assigns.cpp). This
   crashes BEFORE cbmc runs, so NO caller of abAppend/abFree could be verified.
   **Fix: switched abAppend/abFree `requires` from `is_fresh` to
   `__CPROVER_w_ok(ab,...)` / `__CPROVER_r_ok(ab->b, len)` / `r_ok(s, len)`.**
   w_ok/r_ok are checkable validity preds (no malloc/library needed), so the
   replace step no longer aborts. is_fresh in `ensures` is FINE (only warns
   "malloc not declared", collapses vacuously); only is_fresh in *replaced
   requires* is fatal.

3. **abAppend `ensures` had to change too.** The old disjunction ensures
   referenced `__CPROVER_OBJECT_SIZE(ab->b)`; under replacement, `assigns` havocs
   `ab->b` to a garbage pointer and CBMC then checks OBJECT_SIZE/r_ok are
   well-defined on it → FAILURE ("pointer invalid in OBJECT_SIZE/R_OK"). Replaced
   with `is_fresh(ab->b, ab->len)` on success (the CBMC idiom for "returns a
   valid buffer"): it would allocate a fresh valid object, but since malloc isn't
   declared it collapses, making editorRefreshScreen vacuous (kill 0/95, "1
   iteration"). Also dropped `__CPROVER_object_whole(ab->b)` from abAppend's
   `assigns` (invalid when ab->b==NULL on the first call → assigns-validity
   FAILURE during replace; not needed anyway since the realloc stub returns a
   fresh object).

**abAppend/abFree still enforce-verify (0/3 and clean) with the w_ok/r_ok
contracts — no regression** (abAppend was already 0/3 vacuous). avocado compiles
their realloc/memcpy/free stubs automatically (their external callees); malloc
stub NOT needed for their own enforce.

**Kill 0/95 cannot be raised while exiting 0**: the non-vacuous w_ok/r_ok-ensures
variant produces the well-definedness FAILURES above (doesn't verify). Vacuity is
inherent here, same as the rest of the file. Don't chase it.
