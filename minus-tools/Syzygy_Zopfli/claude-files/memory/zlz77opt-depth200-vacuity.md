---
name: zlz77opt-depth200-vacuity
description: ZopfliLZ77Optimal verifies @200 with sound Fixed-sibling spec but 0/28 kills (depth-200 vacuity)
metadata: 
  node_type: memory
  type: project
  originSessionId: cf4817c1-02eb-4ae1-a917-45d46c8332fa
---

ZopfliLZ77Optimal (zopfli.c ~5247) — the squeeze driver, sibling of
[[zlof-depth200-vacuity]] (ZopfliLZ77OptimalFixed). Verifies under the official
runner with a sound spec mirroring the Fixed sibling:
requires is_fresh(s)+is_fresh(store)+instart<=inend+ (inend-instart <
max_malloc_size/sizeof(float)-1); assigns(object_whole(s), object_whole(store)).
Unlike Fixed it never sets s->blockstart/blockend itself (GetBestLengths/
FollowPath don't either), so NO exact blockstart/blockend ensures available.

0/28 kills — inherent depth-200 vacuity, do NOT chase. Official flow is dfcc
enforce-contract with __CPROVER__start harness, all callee bodies INLINED (not
replaced), cbmc --depth 200. The whole call tree (greedy, LZ77OptimalRun→
GetBestLengths, ZopfliCalculateBlockSize→EncodeTree→BoundaryPM recursion) inlines
deep; every mutant's effect is past the depth-200 frontier:
- malloc-size mutants (blocksize-1, inend+instart): OOB only manifests deep
  inside inlined LZ77OptimalRun array writes; nothing caller-visible (arrays are
  local + freed). The zlof OBJECT_SIZE-callee-requires kill trick does NOT apply
  here because official flow INLINES (doesn't replace) LZ77OptimalRun.
- 7 verbose mutants (line `if (verbose_more || (verbose && cost<bestcost))`):
  EQUIVALENT — only affect an fprintf to stderr, no observable contract effect.
- loop-cond / cost<bestcost / i>5&&cost==lastcost / lastrandomstep mutants: all
  control which store/stats get produced, observable only after executing the
  huge inlined loop body, past depth 200.

Tooling notes: plain `goto-instrument --enforce-contract` (legacy, non-dfcc)
CRASHES here — "Recursive call to BoundaryPM during inlining / Numeric
exception". The official runner avoids it via dfcc. To measure kill score
faithfully, mutate the source and call `_run_official.py ZopfliLZ77Optimal`
directly (~5s/run) — see /app/kill_zlz77opt_official.py. Hand-built dfcc with
`--dfcc __CPROVER__start` fails ("statement type 'input' not supported" / "name
not found in symbol_table") so reproducing the flow standalone isn't worth it;
just use the official runner.
