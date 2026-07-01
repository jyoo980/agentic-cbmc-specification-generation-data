---
name: cbmc-isfresh-mutation-kills
description: How to write CBMC specs that kill content-dependent mutants for pointer/array C functions (kilo.c)
metadata: 
  node_type: memory
  type: reference
  originSessionId: abeb042a-ff49-4bd5-a5d1-8c4a462ef622
---

For CBMC `--enforce-contract` mutation killing on functions reading buffers (e.g. `kilo/kilo.c`):

- `__CPROVER_is_fresh(p, SIZE)` with a **symbolic** SIZE (e.g. `is_fresh(row->render, row->rsize)`) makes individual byte contents NOT solver-controllable, so content-dependent mutants survive. Use a **constant** size and bound the index separately: `is_fresh(row->render, 4)` + `requires(row->rsize >= 0 && row->rsize <= 4)`.
- Strongest ensures = exact functional duplicate: `__CPROVER_ensures(__CPROVER_return_value == ( <original condition> ? 1 : 0))`. Re-derefs the same memory; any output-changing mutant violates it.
- For nullable pointers use the disjunction `requires(p == NULL || __CPROVER_is_fresh(p, N))` — needed to kill null-check mutants. Plain `is_fresh` (always non-null) misses them.
- **Depth is the limiter, not spec strength.** Contract enforcement with multiple `is_fresh` objects (struct + 2 buffers) needs `cbmc --depth` > ~260 to find postcondition counterexamples. The docs' `--depth 200` (docs/running-cbmc-directly.md) is too shallow — caps content-mutant kills. Buffer/struct size reductions do NOT lower this; it is structural overhead.
- You still need `is_fresh(row, sizeof(*row))` for soundness (else the original fails memory safety on the postcondition's snapshot deref). Dropping it kills more mutants at depth 200 but makes the original unverifiable — not acceptable.
- Result for `editorRowHasOpenComment`: strong spec verifies soundly at any depth; kills 16/16 mutants at depth ≥ ~280, 9/16 at depth 200.
