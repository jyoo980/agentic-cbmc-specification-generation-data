---
name: zlz77gbr-rok-beats-vacuity
description: ZopfliLZ77GetByteRange 5/5 kills — r_ok instead of is_fresh shrank prologue under depth-200
metadata: 
  node_type: memory
  type: project
  originSessionId: 577af9d0-4707-4475-8c0e-6e7aa05ba5b0
---

ZopfliLZ77GetByteRange (zopfli.c) verifies with an exact-value ensures and kills all 5/5 mutants.

**Key trick:** with `is_fresh` for the 4 pointers (lz77, pos, dists, litlens), the exact-value
postcondition landed PAST `--depth 200` (vacuous@200, killed@400) — the classic depth-200 vacuity
pattern. Switching all `__CPROVER_is_fresh` requires to `__CPROVER_r_ok` shrank the prologue (no
fresh allocation / disjointness map) enough to bring the value-comparison postcondition back within
depth 200 → 5/5 kills. The body only reads, so r_ok (readability) is sound and sufficient; no
distinctness needed.

**Also needed:** `__CPROVER_requires(lz77->size <= ((size_t)-1) / sizeof(*lz77->pos))` to prevent
`size * sizeof` overflowing to a 0-byte extent (counterexample had size=2^63 → pos OOB).

**How to apply:** when a small leaf function with an exact-value postcondition shows depth-200
vacuity (verifies but 0 kills, mutants die at depth 400+), try replacing is_fresh with r_ok before
concluding it's inherent. See [[avocado-depth200-vacuity]], [[zlz77ghat-depth200-vacuity]].
Kill script: /app/kill_zlz77gbr.sh
