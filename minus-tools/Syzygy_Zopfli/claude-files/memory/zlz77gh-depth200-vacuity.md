---
name: zlz77gh-depth200-vacuity
description: "ZopfliLZ77GetHistogram 0/29 kills — depth-200 vacuity, spec kills mutants at depth 600+, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: ab37735a-e3db-4a4e-85f7-368921b381ea
---

`ZopfliLZ77GetHistogram` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies with a full
memory-safety contract (is_fresh on lz77 + ll_counts/d_counts + cumulative ll_counts/d_counts
sized via `ZOPFLI_NUM_{LL,D} * (lend/N) + N` macros + ll_symbol/d_symbol/dists over size,
plus the two forall symbol-bound invariants, lend<=size). Kill score is **0/29**.

**Why:** Same structural vacuity as the sibling [[zlz77ghat-depth200-vacuity]]. The harness
runs hardcoded `cbmc --depth 200`. The 10 is_fresh clauses + 2 forall quantifiers + the two
inlined `ZopfliLZ77GetHistogramAt` calls (each 4 loops) + 2 memsets push every in-body
assertion past depth 200, so all body checks are vacuous. Proven: the OOB mutant
`for (i=lstart; i<=lend; i++)` SURVIVES at depth 200 but is KILLED (FAILED) at depth 600 and
1200. So the spec is strong, not weak.

**How to apply:** Don't chase these 29 survivors — they are unreachable under --depth 200, not
spec gaps. Kill harness is kill_zlgh.py; diagnosis used a manual depth sweep. Can't raise the
depth (harness-fixed) and must not hardcode depth-related values into the spec.

**2026-06-27 re-check (dead-ends tried, don't repeat):**
- Re-verified: still VERIFICATION SUCCESSFUL at --depth 200 (0/25355).
- The else-branch callee-precondition mutants (line 1673 `lend-1`->`lend+1`, line 1678
  `lstart-1`->`lstart+1`) flip the `lpos` arg to the replaced `GetHistogramAt`, which requires
  `lpos < lz77->size`. Hoped that precondition assertion (no memsets before it) might be reachable
  under 200 — it is NOT. Survives@200/250, killed@300. Past the depth too.
- Hypothesis "the two forall clauses eat the depth budget; remove them to free it" is WRONG.
  Removing both forall keeps the original verifying@200 (its body asserts are vacuous either way)
  but the 1673 mutant kill threshold got WORSE (250 survived, killed only @350, up from ~300).
  The is_fresh allocations of the symbolic-sized arrays are the bottleneck, not the foralls.
  Removing forall only weakens the spec for zero kills. Confirmed: 0/29 is inherent; leave spec as-is.
