---
name: cbmc-lz77optimalrun-free-body-crash
description: LZ77OptimalRun crashes goto-instrument under --enforce-contract because its body calls free(*path); unverifiable
metadata: 
  node_type: memory
  type: project
  originSessionId: 449b2e93-f628-4b7e-a978-f8e6c53a98ab
---

`LZ77OptimalRun` in zopfli.c (orchestrates GetBestLengths → free(*path) → TraceBackwards → FollowPath) is **unverifiable**, independent of the spec.

Under `--enforce-contract`, goto-instrument aborts during "Enforcing contracts":
```
Invariant check failed
File: instrument_spec_assigns.cpp:597 function: create_car_expr
Reason: no definite size for lvalue target ... __CPROVER_deallocate::$tmp::return_value___VERIFIER_nondet___CPROVER_bool
```
The trigger is the explicit `free(*path)` call in the body: goto-instrument's assigns-instrumentation pass can't compute a definite size for the `__CPROVER_deallocate` builtin's internal nondet bool. This is the `free()` variant of [[cbmc-malloc-body-enforce-crash]] (malloc/free in-body aborts goto-instrument under enforce). Note this LZ77OptimalRun crash is the `free()` flavor — distinct from [[cbmc-tracebackwards-append-depth-vacuity]] where ZOPFLI_APPEND_DATA's malloc only exhausts depth and does NOT crash.

NOT the function-pointer crash: even though LZ77OptimalRun takes a `CostModelFun *costmodel` param (cf. [[cbmc-getcostmodelmincost-fnptr-param]]), the crash fires earlier in goto-instrument on the deallocate, before any `costmodel$object` lookup.

A strong sound spec (union of GetBestLengths + TraceBackwards + FollowPath preconditions; ensures return>=0 && return<ZOPFLI_LARGE_FLOAT) is left in place at the def.
