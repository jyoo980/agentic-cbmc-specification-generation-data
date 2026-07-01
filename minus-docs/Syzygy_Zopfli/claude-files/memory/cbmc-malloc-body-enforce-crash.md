---
name: cbmc-malloc-body-enforce-crash
description: zopfli.c functions that call malloc() directly in their body cannot be verified — goto-instrument crashes under --enforce-contract
metadata: 
  node_type: memory
  type: project
  originSessionId: fb9c9a17-02a2-4e4c-870b-ee03d8bf9757
---

`ZopfliLengthLimitedCodeLengths`, `ZopfliLengthsToSymbols` (confirmed 2026-06-25 — body mallocs `bl_count`/`next_code`), `OptimizeHuffmanForRle` (confirmed 2026-06-25 — body mallocs/frees `good_for_rle`), `ZopfliCleanCache` (confirmed 2026-06-25 — free-only body: `free(lmc->length/dist/sublen)`; crashes on the `__CPROVER_deallocate::...return_value___VERIFIER_nondet___CPROVER_bool` lvalue rather than malloc's `should_malloc_fail`, same root cause), `ZopfliCleanLZ77Store` (confirmed 2026-06-26 — free-only body freeing 7 store pointers; same `__CPROVER_deallocate` crash; crashes with OR without an `assigns`/`frees` clause), and any `Syzygy_Zopfli/c_code/zopfli.c` function whose **body calls `malloc`/`free`/`realloc` directly** cannot be verified by `run-cbmc`. goto-instrument aborts BEFORE cbmc runs, with:

```
instrument_spec_assigns.cpp:597 function: create_car_expr
Condition: size.has_value()
Reason: no definite size for lvalue target: malloc::1::1::should_malloc_fail  (type bool, <builtin-library-malloc>)
```

Root cause (proven by elimination, not clause-specific): `run-cbmc` always does `goto-instrument --enforce-contract <fn>` plus `--add-library` (which links CBMC's malloc model). When the function body calls malloc, the model's internal `should_malloc_fail` bool write gets caught by the frame-condition / car (conditional-address-range) instrumentation, which can't size it → invariant violation → `Aborted`. It is INDEPENDENT of contract content:
- removed caller `assigns` → crash; removed ALL `assigns` in file → crash
- callee memory-predicates (`is_fresh`→`rw_ok`/`1`) neutralized → crash
- callees given trivial `requires(1)/ensures(1)` → crash
- caller `is_fresh` requires removed → crash

So NO specification on the function can avoid it, and you can't remove the malloc calls (no C edits). goto-cc still ACCEPTS the contract (compile passes); the abort is purely in the goto-instrument enforce step. Contrast: the documented is_fresh/depth-200 vacuity wall ([[cbmc-depth200-isfresh-vacuity]]) hits functions that take is_fresh pointer params but DON'T call malloc themselves — those at least reach cbmc.

Deliverable for `ZopfliLengthLimitedCodeLengths`: left a sound, strong contract anyway (requires `n>=0`, `1<=maxbits<=15`, is_fresh on `frequencies`/`bitlengths` of `n` elems; assigns object_whole(bitlengths); ensures return ∈{0,1} and `forall k<n: bitlengths[k] <= maxbits`). It is correct/sound but the tool can't check it. Callees `InitLists/BoundaryPM/BoundaryPMFinal/ExtractBitLengths` already have contracts. See [[cbmc-aarch64-fileh-stub]] (always pass `-I /tmp/cbmc-inc`) and [[cbmc-treesitter-loopcontract-misparse]].
