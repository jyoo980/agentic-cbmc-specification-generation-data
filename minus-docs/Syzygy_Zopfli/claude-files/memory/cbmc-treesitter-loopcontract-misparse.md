---
name: cbmc-treesitter-loopcontract-misparse
description: read_stdin_to_bytes loop contract crashes the mutation-testing tree-sitter parser for all functions in zopfli.c
metadata: 
  node_type: memory
  type: project
  originSessionId: 30f42bc6-3f06-4c1c-b5f4-9fa318a6bb67
---

In `Syzygy_Zopfli/c_code/zopfli.c`, `read_stdin_to_bytes` has a `while (...) __CPROVER_assigns(...) __CPROVER_loop_invariant(...) { ... }` loop contract (around line 3553). Tree-sitter's C grammar mis-parses the `__CPROVER_loop_invariant(...)` annotation immediately before the `{` as a `function_definition` with a `parenthesized_declarator`. This makes `tools/util/tree_sitter_utils.py:_get_function_definition_name` hit `AssertionError: Unexpected declarator shape: parenthesized_declarator`.

Consequence: `generate_mutants_and_compute_score` (the mutation-testing step of `run-cbmc`) parses the WHOLE file and crashes — so it crashes for EVERY function in zopfli.c, even though CBMC verification itself (goto-cc/cbmc) succeeds first. The crash happens only AFTER `is_function_verified` is True, so the function did verify.

Function-level contracts parse fine; only loop-level (`__CPROVER_loop_invariant`) annotations trigger it.

FIX FOUND (2026-06-25): the misparse comes specifically from the LAST loop-contract clause before `{` being shaped like `name(...)` containing a NESTED call — `__CPROVER_loop_invariant(__CPROVER_is_fresh(buffer, buffer_size))` parses as a nested `function_definition` whose declarator is the parenthesized `(__CPROVER_is_fresh(...))`. Reordering the invariants so a clause with NO nested call sits last before `{` (e.g. end with `__CPROVER_loop_invariant(total_read <= buffer_size)` and move the `__CPROVER_is_fresh` one earlier) makes the whole file parse cleanly. The invariant SET is unchanged, so CBMC semantics are identical. This is now applied in zopfli.c, so mutation testing works for all functions again. Verify a reorder before applying by parsing with tree_sitter_c and checking for `function_definition` nodes whose declarator chain dead-ends at a `parenthesized_declarator`.

Older fallback (if the reorder ever regresses): copy zopfli.c to a scratch file, strip the `__CPROVER_assigns(ch,...)`/`__CPROVER_loop_invariant(...)` lines, and run `run-cbmc --function F` on the copy — loop contracts aren't applied during F's verification anyway. See [[cbmc-depth200-isfresh-vacuity]].
