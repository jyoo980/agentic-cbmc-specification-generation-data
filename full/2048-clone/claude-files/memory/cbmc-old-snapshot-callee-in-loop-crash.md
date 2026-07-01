---
name: cbmc-old-snapshot-callee-in-loop-crash
description: "CBMC goto-instrument aborts (create_car_expr Unreachable) when a callee whose contract uses __CPROVER_old is contract-replaced inside a caller's loop"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 4b358302-f4cd-47c8-ac45-8d7d4bda6092
---

Verifying a caller crashes CBMC with `instrument_spec_assigns.cpp:615 function: create_car_expr Reason: Unreachable` (Aborted, during goto-instrument "Enforcing contracts") when a contract-replaced callee is invoked **inside a `for` loop** AND that callee's contract carries a rich two-clause ensures combining a plain predicate with a history-snapshot one (e.g. slideArray's `SA_PACKED` + `SA_SUM == SA_OLD_SUM` where SA_OLD_SUM uses `__CPROVER_old(array[i])`).

Narrowed empirically: the same call OUTSIDE a loop instruments fine; a single-clause variant (only the old-sum, or only one `__CPROVER_old`) inside a loop instruments fine. Crash needs loop + the full two-clause contract together. Independent of the caller's own contract (empty assigns, ensures-only, or none all crash the same way).

Implication: such a caller (e.g. `testSucceed` in /app/2048-clone/2048.c, which loops calling slideArray) cannot be verified — it's a CBMC tool defect, not a spec problem. Don't weaken the verified callee contract just to dodge it (degrades other callers). Best response: strongest sound caller contract (empty `__CPROVER_assigns()` for a no-side-effect harness — printf does NOT violate empty assigns) + a comment citing the crash and "CBMC cannot verify all correct C code."

Related: [[io-func-static-state-unkillable]].
