---
name: run-all-tests-no-mutants
description: run_all_tests verifies but has 0 mutants; needed a trivial explicit contract on single_test to satisfy replace-call
metadata: 
  node_type: memory
  type: project
  originSessionId: 55de8923-25be-431f-b47f-d4733dec6eaf
---

`run_all_tests` (zopfli.c, ~line 6170) calls `single_test(in, ...)` 9× with the same `in`.
avocado generates **0 mutants** (no mutable operators) → kill score inherently 0, don't chase.

It verifies @depth200 (official flow `--enforce-contract run_all_tests`) with NO contract on
run_all_tests itself, BUT the flow does `--replace-call-with-contract single_test`, which errors
("Function 'single_test' does not have a contract... add explicit requires and ensures") unless
single_test carries an EXPLICIT contract. `__CPROVER_assigns()` alone is NOT enough — goto-instrument
demands explicit requires+ensures.

Fix: gave [[single-test-no-mutants]] a trivial sound contract:
`__CPROVER_requires(1) __CPROVER_assigns() __CPROVER_ensures(1)`.
- empty assigns is sound (single_test writes only locals out/outsize/bp/options).
- requires(1) is re-satisfiable across all 9 same-`in` call sites (is_fresh(in) would FAIL on the 2nd call).
- single_test still PASSES its own enforce-contract with this contract.

Both single_test and run_all_tests → PASS rc=0. Note: the prior single-test-no-mutants memory said
"don't add a spec" — that was true in isolation, but the caller run_all_tests forces a minimal one.
