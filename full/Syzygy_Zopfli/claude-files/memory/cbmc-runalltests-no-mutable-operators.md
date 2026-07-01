---
name: cbmc-runalltests-no-mutable-operators
description: run_all_tests verifies as pure single_test dispatcher; no mutable operators
metadata: 
  node_type: memory
  type: project
  originSessionId: 730b7c68-cd5f-42d8-bc68-540bfc67b78f
---

`run_all_tests` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies SOUNDLY with
`requires(is_fresh(in, 9))` + `requires(in[8]=='\0')` + empty `assigns()`,
mirroring [[cbmc-singletest-no-mutable-operators]]'s precondition.

**Why:** body is nine `single_test(in, ...)` calls with only constant literal
args (btype/blocksplitting/blocksplittingmax), so mutation reports "no mutable
operators" — nothing to kill. Needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs`.

**How to apply:** for pure dispatchers, copy the callee's `is_fresh`/value
preconditions and use empty assigns; expect "no mutable operators".
