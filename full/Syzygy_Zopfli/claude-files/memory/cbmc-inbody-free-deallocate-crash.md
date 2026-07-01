---
name: cbmc-inbody-free-deallocate-crash
description: in-body free() crashes goto-instrument in __CPROVER_deallocate under any contract (ZopfliCleanCache)
metadata: 
  node_type: memory
  type: project
  originSessionId: ecdf3aa9-c60b-4acf-aca1-5ee3c79eccf4
---

The in-body `free()` analog of [[cbmc-inbody-malloc-enforcement-crash]]. A function
whose body calls `free()` cannot have its contract discharged by `run-cbmc`.
During *Enforcing contracts*, goto-instrument aborts with an invariant violation in
`__CPROVER_deallocate` referencing
`return_value___VERIFIER_nondet___CPROVER_bool` (the `--malloc-may-fail` nondet
free model).

**Why:** same root cause as the malloc crash — the harness runs `--malloc-may-fail`,
and the assigns/free instrumentation trips an invariant on the deallocate model.

**How to apply:** reproduces regardless of contract — full contract with
`frees`+`is_fresh`+`was_freed`, or just `requires`+`assigns()`+`frees`, all crash
identically. Tool limitation, not a spec defect. Write the strongest sound contract
(is_fresh on lmc + each pointer field, `__CPROVER_frees(...)`, `was_freed` ensures)
and note it's undischargeable. First hit: `ZopfliCleanCache` in
`/app/Syzygy_Zopfli/c_code/zopfli.c` (frees length/dist/sublen). Needs
`-I /app/Syzygy_Zopfli/stubs` for FILE.h.
