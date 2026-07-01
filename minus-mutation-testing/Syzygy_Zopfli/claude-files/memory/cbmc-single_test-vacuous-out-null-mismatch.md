---
name: cbmc-single_test-vacuous-out-null-mismatch
description: "zopfli.c single_test passes out=NULL/outsize=0 to ZopfliDeflate, violating its contract; avocado SUCCESS is vacuous (depth-200)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 37a0c924-c37e-4413-b3d5-6dc2afb597e0
---

In `/app/Syzygy_Zopfli/c_code/zopfli.c`, `single_test` calls `ZopfliDeflate(&options, btype, 1, in, strlen(in), &bp, &out, &outsize)` with literal `out = 0` (NULL) and `outsize = 0` — the real entry API where the callee mallocs/reallocs the output buffer.

ZopfliDeflate's verified contract instead REQUIRES a pre-existing buffer: `is_fresh(*out, (*outsize)+1)` and `((*outsize)&((*outsize)-1)) != 0`. With out=NULL/outsize=0 BOTH fail. Proven via a minimal probe (`/tmp/probe.c`): precondition checks for `*out` and `*outsize` → FAILURE, VERIFICATION FAILED.

`run-cbmc --function single_test` nonetheless reports SUCCESS — but it is VACUOUS. Under the tool's fixed `--depth 200`, the is_fresh/contract instrumentation exhausts the depth budget before reaching ZopfliDeflate's precondition checks; `--cover location` shows lines 6260–6283 unreachable, and `__CPROVER_ensures(0)` still "passes". Holds even with a 1-byte input buffer, so strlen is NOT the cause — the contract machinery is.

**Why:** The C code can't be changed (NULL/0 are literal) and ZopfliDeflate's contract shouldn't be weakened (it would force ZopfliDeflate onto the realloc-growth path and break ITS proof). So single_test is a genuine "CBMC cannot soundly verify otherwise-correct C" case — a false/vacuous pass, not a real one.

**How to apply:** When asked to verify a top-level test harness that calls a compressor/allocator entry point with NULL output + 0 size, check whether the callee's contract demands a pre-existing buffer — if so the pass is likely vacuous. Confirm non-vacuity with `__CPROVER_ensures(0)` and `--cover location`. See [[cbmc-depth200-truncates-large-functions]] and [[cbmc-isfresh-malloc-vacuous-pass]].
