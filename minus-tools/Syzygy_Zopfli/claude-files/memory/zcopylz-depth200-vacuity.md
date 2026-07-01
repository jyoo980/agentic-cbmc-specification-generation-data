---
name: zcopylz-depth200-vacuity
description: "ZopfliCopyLZ77Store verifies (official PASS) but 0/18 kills under the official --depth-200 grader; loop 2 always >=288 iters, inherent vacuity"
metadata: 
  node_type: memory
  type: project
  originSessionId: 8a982c05-2ea3-49e7-86a0-597ce03e7d42
---

ZopfliCopyLZ77Store (zopfli.c ~5058): deep-copy, 3 malloc-then-fill loops over
source->size, llsize, dsize. Already carries a strong dfcc-style spec (is_fresh
preconditions on all 7 source + 7 dest arrays, object_whole assigns, sampled
boundary `[0]`/`[n-1]` ensures, and per-loop `__CPROVER_loop_invariant` /
`__CPROVER_assigns` / `w_ok` — a copy of the [[zic-dfcc-loopcontracts-12of16]]
technique).

**It verifies:** `tools.run_cbmc` → PASS.

**Kill score under the OFFICIAL grader = 0/18 (all survive).** The official
`run_cbmc` flow caps loops (`--partial-loops --unwind 5`) then runs `--depth 200`,
and does NOT apply loop contracts — proven decisively: a mutant the
[[zic-dfcc-loopcontracts-12of16]] note records as KILLED under the manual dfcc
flow (`ZopfliInitCache` loop `i < blocksize` → `i <= blocksize`) ALSO SURVIVES
under `run_cbmc`. So the loop-contract annotations are inert to the grader.

**Why 0 is inherent here:** even with loops capped at 5 iters, this function's
own precondition expands ~14 `is_fresh` calls (7 source + 7 dest arrays) plus the
replace-call preconditions of ZopfliCleanLZ77Store / ZopfliInitLZ77Store / CeilDiv
×2, pushing the loop bodies and ALL final `__CPROVER_ensures` (which sit at
function exit after all 3 loops) past the depth-200 frontier → vacuous. Same
is_fresh-prologue mechanism as [[zslld-depth200-vacuity]] (7 nested-array is_fresh
→ body at depth ~550) and [[silmc-depth200-vacuity]] (6 is_fresh + assigns snapshot
~450). Loop 2 (ll_counts) also always runs llsize >= ZOPFLI_NUM_LL == 288 iters,
so dropping --unwind would leave it incomplete regardless. The 18 mutants:
- 15 loop-bound (`<=`,`>`,`>=`,`==`,`!=` on each of 3 loops): 12 killable only via
  the dfcc flow; under --depth 200 all masked by loop-2 non-completion. The 3 `!=`
  are equivalent mutants regardless (0-start/+1-step ≡ `<`).
- 3 alloc-check `||`→`&&` (lines 5138/5142/5144): EQUIVALENT — the never-NULL
  malloc stub (/app/stubs/cprover_alloc.c) makes both branches dead.

So ceiling under the grader is 0; ceiling under the manual dfcc flow is 12/18
(4 killable per loop × 3, minus 3 `!=` + 3 `||→&&` equivalents). The spec is
already maximal for the dfcc technique. Don't chase. Same class as
[[zslld-depth200-vacuity]], [[zuh-depth200-vacuity]], [[silmc-depth200-vacuity]],
[[zstc-depth200-vacuity]].

Scripts: /app/_verify_zcopylz.sh + /app/_zclz_harness.c (dfcc flow, SLOW: original
>10min — 7 arrays), /app/_kill_zcopylz.py (official run_cbmc kill harness → 0/18).
