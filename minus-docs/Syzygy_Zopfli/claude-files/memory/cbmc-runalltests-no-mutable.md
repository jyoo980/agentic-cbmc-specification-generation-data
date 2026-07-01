---
name: cbmc-runalltests-no-mutable
description: "run_all_tests in zopfli.c verifies with \"no mutable operators\"; mirrors single_test"
metadata: 
  node_type: memory
  type: project
  originSessionId: 6ba01968-1684-4557-a282-a52ab6ac6ef4
---

`run_all_tests` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies NON-vacuously and reports "no mutable operators" (strongest outcome). Body is 9 `single_test(in, ...)` calls with integer literals only — nothing to mutate. Sole precondition needed: `__CPROVER_requires(__CPROVER_is_fresh(in, 1))`, matching its callee [[cbmc-singletest-no-mutable]] (single_test requires is_fresh(in,1)).

**Why:** Confirms the test-driver pattern at the top of the call chain.
**How to apply:** For pure forwarding drivers, copy the callee's is_fresh precondition; no assigns/ensures needed.
