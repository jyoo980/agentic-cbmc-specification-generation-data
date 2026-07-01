---
name: zic-dfcc-loopcontracts-12of16
description: "ZopfliInitCache hit 12/16 via dfcc loop contracts + alloc stub + non-quantified boundary invariants, beating depth-200 vacuity"
metadata: 
  node_type: memory
  type: project
  originSessionId: 61358d05-5218-424b-a7ac-a9564dd0ffae
---

ZopfliInitCache (3 malloc+fill loops over blocksize) scores **12/16** — far better than
the usual depth-200-vacuity 0-kill outcome for loop-heavy Zopfli funcs. The technique
(novel in this repo; reusable for other malloc-then-fill funcs):

**Flow (NOT the standard `--depth 200`):** verify UNBOUNDED with dynamic frame condition
checking + loop contracts. `--depth 200` is what makes loop bodies vacuous; loop contracts
abstract the loops so no depth bound is needed.
```
goto-cc src.c HARNESS.c /app/stubs/cprover_alloc.c --function __harness
goto-instrument --dfcc __harness --enforce-contract FN --apply-loop-contracts in.goto out.goto
cbmc out.goto --function __harness            # no --depth
```
- Need a separate HARNESS function (mallocs the struct, calls FN) — dfcc requires harness_id != contract fn.
- MUST link /app/stubs/cprover_alloc.c, else dfcc's contracts library hits "no body for free" → `Numeric exception` crash.
- Plain `--apply-loop-contracts` (non-dfcc) does NOT track locally-malloc'd arrays as writable in loop frames → assigns failures. dfcc does.

**Quantified loop invariants DON'T WORK in this CBMC.** Even the canonical array-init example
(`forall j. j<i ==> arr[j]==0`) fails loop_invariant_step: whole-array `object_whole` havoc
+ assumed `forall` isn't instantiated at the checked index. Don't chase quantified content.

**Workaround that gives the kills — NON-quantified boundary invariants/postconditions:**
- ensures: `arr[0]==v && arr[n-1]==v` (sampled first+last, not forall).
- loop invariants: `i<=n`, `__CPROVER_w_ok(arr, size)`, `(i>0)==>(arr[0]==v)`,
  `(i>=n)==>(arr[n-1]==v)`. These preserve WITHOUT quantifier instantiation.
- loop assigns: `__CPROVER_assigns(i, __CPROVER_object_whole(arr))`.
- requires `blocksize <= __CPROVER_max_malloc_size / (per-elem-bytes)` to keep CAR/malloc
  size assertions valid (symbolic, not a hardcoded CLI value).

This kills: `<=` (OOB caught by w_ok bounds), `>`/`>=`/`==` (0-iter → boundary elem unwritten
→ postcondition fails). Survivors are inherent: 3 `!=` equivalent mutants (0-start +1-step
loop ≡ `<`) + 1 `sublen!=NULL` (never-NULL malloc stub → mutant always exits → ensures vacuous).

Scripts: /app/_verify_zic.sh, /app/kill_zic.sh, /app/_zic_harness.c. Contrast [[avocado-depth200-vacuity]].
