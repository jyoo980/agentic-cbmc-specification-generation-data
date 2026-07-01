---
name: cbmc-inbody-malloc-enforcement-crash
description: functions that call malloc/free in their body crash goto-instrument under contract enforcement
metadata: 
  node_type: memory
  type: project
  originSessionId: 4eacc9a0-40ca-4147-a729-44d2a33e32f8
---

A function whose **body** calls `malloc()`/`free()` cannot have its contract
discharged by the `run-cbmc` harness. During *contract enforcement*,
goto-instrument aborts with:

`Invariant check failed ... instrument_spec_assigns.cpp create_car_expr:
no definite size for lvalue target ... malloc::1::1::should_malloc_fail`

**Why:** the harness runs CBMC with `--malloc-may-fail`, which introduces a bool
symbol `should_malloc_fail` in the inlined malloc model. The assigns-instrumentation
tries to build a conditionally-assignable-range for it and trips the invariant.

**How to apply:** This reproduces regardless of contract content — full contract,
`requires`+`is_fresh`, or even a bare `requires(length>=0)` with NO assigns clause
all crash identically. It is a tool limitation, not a spec defect. Don't burn the
5-attempt budget trying to work around it from source (can't change harness flags
or the C body). Write the strongest sound contract anyway and note it's
undischargeable. First hit: `OptimizeHuffmanForRle` in
`/app/Syzygy_Zopfli/c_code/zopfli.c` (mallocs `good_for_rle`).
