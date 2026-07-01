---
name: cbmc-is-fresh-gotchas
description: How __CPROVER_is_fresh / pointer_equals behave (and fail) in the Syzygy_Zopfli run-cbmc harness
metadata: 
  node_type: memory
  type: project
  originSessionId: fa0af406-5648-4eb0-808f-80c6e5fe0b93
---

Behaviors observed with `run-cbmc` (goto-instrument `--enforce-contract`, cbmc `--depth 200`) on `/app/Syzygy_Zopfli/c_code/zopfli.c`:

- **is_fresh / pointer_equals inside a HELPER function fail.** A predicate function whose body calls `__CPROVER_is_fresh` (e.g. the repo's recursive `valid_chain_nodes`, or a non-recursive equivalent) is checked as an ordinary function where the primitive has no body → `[fn.no-body.__CPROVER_is_fresh] ... FAILURE` (verification FAILS). The CPROVER-manual "user-defined memory predicate" pattern does NOT work here. Inline the `is_fresh` calls directly in the `requires` clause instead.
- **Inline is_fresh on a symbolic-subscript lvalue WORKS** (e.g. `__CPROVER_is_fresh(lists[index][1], sizeof(Node))` with symbolic `index`): it is non-vacuous and makes the deref safe. (The in-file comment claiming symbolic subscripts are unsupported is misleading — the real blocker is depth, see [[cbmc-depth-200-object-limit]].)
- **`__CPROVER_pointer_equals` inline also hits no-body** in this config — avoid it; use a second `is_fresh` instead of aliasing.
- A vacuous (depth-truncated) run still prints `**** WARNING: Depth-bounded analysis may yield unsound verification results` and exits 0. To detect vacuity, add a deliberately-false `__CPROVER_ensures(x != x)`; if it still "verifies", the contract is vacuous.
