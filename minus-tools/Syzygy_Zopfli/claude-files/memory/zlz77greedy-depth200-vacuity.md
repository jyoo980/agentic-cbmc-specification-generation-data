---
name: zlz77greedy-depth200-vacuity
description: "ZopfliLZ77Greedy verifies w/ full memory-safety frame but 0/74 kills, vacuous even @depth1000"
metadata: 
  node_type: memory
  type: project
  originSessionId: e6032517-e984-44b5-8047-1fcc92b324ec
---

ZopfliLZ77Greedy (the gzip-style lazy-matching LZ77 driver) verifies @depth 200
with a sound full-frame contract but scores **0/74 kills** — deep depth-200
vacuity, the worst-reachability case in the family. Don't chase the kills.

**Why:** all 74 mutants sit in the main parse loop body or its guards, behind a
~17-object `is_fresh` prologue (s, h+7 chain/value tables, store+7 parallel
buffers, in). The 7 callees have loop-incompatible real contracts (ZopfliUpdateHash
requires pos==8, ZopfliFindLongestMatch a fully-determined hash chain), so they
must be weak-stubbed. Even with cheap stubs the loop body is unreachable: the
killable in-loop asserts `assert(i < inend)` need either a 257-iteration
match-copy inner loop (leng forced to 258 to skip the else-if `continue`) or
multiple main-loop iterations. Confirmed empirically: `assert(i > inend)` mutant
SURVIVES at depth 200/400/700/**1000**. The control-flow/boundary mutants
(early-return guard, windowstart computation, loop-guard comparisons) are
**inherently unkillable** by any memory-safety spec — the greedy parse's output
(store->size) is data-driven with no closed form, and size-monotonicity doesn't
discriminate (lazy matching can `continue` without storing). Like [[zlz77gh-depth200-vacuity]]
and [[silmc-depth200-vacuity]] but unreachable even at 1000, not just 200.

**How to apply:** leave the sound spec in place (verifies @200). Artifacts:
`/app/stubs/greedy_callees.c` (weak stubs for Reset/Warmup/Update/FindLongestMatch/
VerifyLenDist; GetLengthScore keeps real contract; reuse `/app/stubs/zslld_weak.c`),
`/app/_greedy_harness.c`, `/app/_verify_greedy.sh`, `/app/kill_greedy.sh` (generic
avocado-diff parser, applies all mutants by line). Weak-stub linking = the
[[zalz-weakstub-4of5]] pattern: goto-cc source → remove-function-body each callee →
link weak.goto first → enforce-contract greedy + replace-call all callees. r_ok/w_ok
levers can't shrink the prologue (nested fields h->head, store->litlens). Don't chase.
