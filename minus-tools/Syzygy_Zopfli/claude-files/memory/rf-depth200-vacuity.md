---
name: rf-depth200-vacuity
description: RandomizeFreqs verifies w/ mem-safety spec but 0/8 kills @depth200 (depth-200 vacuity)
metadata: 
  node_type: memory
  type: project
  originSessionId: 3699171c-3b9a-4bdc-be78-7cdca7200aa5
---

RandomizeFreqs (zopfli.c, static) — random freq-shuffle loop calling Ran twice
per iter. Verifies @depth200 with the standard static-func flow: stub +
`--add-library` + `--partial-loops --unwind 5` + `--replace-call-with-contract Ran`
+ `--enforce-contract`. Contract is pure memory-safety: `requires(n>0)`,
`is_fresh(state)`, `is_fresh(freqs, (size_t)n*sizeof(size_t))`,
`assigns(object_whole(freqs), *state)`.

**0/8 @depth200** — classic [[avocado-depth200-vacuity]]. Ceiling is **2/8**, only
@depth>=400: the OOB-read mutant `Ran(state)*n` killed @300, the OOB-write mutant
`i<=n` killed @400. First reachable spec point (OOB read in the `if` branch) sits
at the ~290 frontier; lowering unwind (1/2/3), inlining Ran instead of
replace-call, all leave it past 200. r_ok lever is useless here (OOB detection
needs is_fresh's exact object size, not w_ok/r_ok).

Other 6 mutants inherently unkillable by any sound spec: `i>n`/`i>=n`/`i==n` skip
the loop, differ only in final RNG state (data-dependent call count, not
pinnable); `i!=n` ≡ `i<n` (equivalent); the two `if`-condition mutants (`%3!=0`,
`*3==0`) only change which random entries copy, every freqs value stays in the
original set so no value postcond distinguishes them. Don't chase.

NB: dfcc + loop-contracts path is a dead end — RandomizeFreqs is `static`, so an
external dfcc harness can't link it ("not found or has no body"); plain
`--enforce-contract` + `--apply-loop-contracts` aborts ("Loops remain"). Static
loop funcs here use `--partial-loops --unwind N`, never loop contracts.
