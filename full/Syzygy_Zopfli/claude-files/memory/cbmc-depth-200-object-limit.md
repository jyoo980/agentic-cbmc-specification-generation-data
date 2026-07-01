---
name: cbmc-depth-200-object-limit
description: "run-cbmc uses --depth 200, which caps how many fresh pointer-heavy objects a contract can set up before postconditions become unreachable"
metadata: 
  node_type: memory
  type: project
  originSessionId: fa0af406-5648-4eb0-808f-80c6e5fe0b93
---

In the Syzygy_Zopfli CBMC harness, `run-cbmc` runs the final check with `cbmc ... --depth 200` and `--unwind 5` (see `/app/tools/run_cbmc_and_mutation_testing.py`, `_CBMC_DEPTH`/`_CBMC_UNWIND`). The depth bound counts symbolic-execution steps along the path, and the `__CPROVER_is_fresh` contract setup runs on that path.

**Why it matters:** each `is_fresh` on a struct containing a pointer field (e.g. `Node {size_t weight; Node *tail; int count;}`) costs ~50 depth steps because CBMC models the nondeterministic pointer field. Only ~3-4 such fresh objects fit before `--depth 200` is exhausted; once exhausted, paths are truncated and CBMC reports `VERIFICATION SUCCESSFUL` **vacuously** (postconditions never reached, mutation kill score 0). A struct with no pointer field, or `char*`, is much cheaper (4+ fit). Confirmed by running `cbmc --depth 200` vs `--depth 100000` on the same goto-binary: 200 = vacuous pass, 100000 = real result.

**Also the body itself can blow the budget:** `ExtractBitLengths` needs only ~2 fresh pointer-heavy objects (chain Node + leaves), yet is *still* vacuous (false `ensures(0==1)` passes, kill 0) even after shrinking to a single fresh leaf and dropping the `forall`. Cause is the BODY: two nested loops, each unwound 5× under `--partial-loops --unwind 5`, generate ~25 inner + ~5 outer + ~5 first-loop guard/index evaluations (~150 depth steps) which, plus minimal is_fresh setup, exhaust `--depth 200` before the postcondition at function end is reached. So nested-loop bodies hit the wall regardless of how cheap the contract is.

**Confirmed: `ZopfliLengthsToSymbols`** (four sequential loops over n / maxbits + two mallocs). Vacuous regardless of spec or pinned bounds: with a minimal contract pinned to n==2, maxbits==1 and only `ensures(0==1)`, binary search on `cbmc --depth` shows the function exit is first reached at depth ~500 (SUCCESS at 400, FAILURE at 600) — far above the harness's fixed 200. Pinning n/maxbits does NOT help because goto-instrument unwinds each loop 5x *structurally* (independent of the symbolic n), so the per-path step count to reach the postcondition is dominated by the unwound loop copies + global static init, not by n. Best achievable = strongest *correct* general spec (zero-length⇒0; same-nonzero-length codes strictly increasing by index), verifies vacuously, kill 0.

**Confirmed cases:** `InitLists` is also vacuous (false `ensures(0==1)` passes) — it needs pool + pool->next(2-node block) + leaves(2-node block) + lists ≈ 5 pointer-heavy fresh objects plus an init loop; shrinking maxbits bound to 2 didn't help. Aside: its 5 surviving mutants are all on `for (i=0; i<maxbits; ...)`, and `< -> !=` there is an *equivalent* mutant (identical behavior for `maxbits>=0`), so at most 4/5 is ever killable even non-vacuously.

**How to apply:** A function needing many valid pointer-heavy objects (like `BoundaryPMFinal`, which needs lists + leaves + pool + pool->next + 3 chain nodes = ~7, or `InitLists` ≈ 5), OR a function with nested loops unwound 5× (like `ExtractBitLengths`), cannot be non-vacuously verified here; it passes vacuously with kill 0, and no spec rewrite fixes it. Write the strongest *correct* contract anyway (it documents intent and exits 0). Don't burn attempts chasing kill score once you've confirmed the depth wall. See [[cbmc-is-fresh-gotchas]].
