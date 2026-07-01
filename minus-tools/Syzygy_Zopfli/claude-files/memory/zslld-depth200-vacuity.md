---
name: zslld-depth200-vacuity
description: "ZopfliStoreLitLenDist verifies w/ full is_fresh contract but 0/31 kills @depth200 (8 fresh buffers push body past frontier ~550), inherent vacuity"
metadata: 
  node_type: memory
  type: project
  originSessionId: 47efee94-0504-472e-bab2-8b94694c6fc8
---

ZopfliStoreLitLenDist (zopfli.c) — cumulative-histogram LZ77 append. Had NO
contract; now verifies (VERIFICATION SUCCESSFUL @ depth 200) with a full
memory-safety + exact-append contract in the no-wrap regime:
`store->size in (0, ZOPFLI_NUM_D)`, `(size & (size-1)) != 0`, `length < 259`,
7 `is_fresh` arrays (litlens/dists/pos/ll_symbol/d_symbol sized size+1;
ll_counts=ZOPFLI_NUM_LL, d_counts=ZOPFLI_NUM_D) + `is_fresh(store)`, 8
`object_whole` assigns (NO pointer fields — so loop-entry mutants trip the
frame), ensures size+1 & litlens/dists/pos payload.

**0/31 kills @depth200 — inherent vacuity, don't chase.** The 7 nested-array
`is_fresh` allocations alone cost ~350 depth; body assert(length<259) at line
3396 is first reached at depth ~550 (SUCCESSFUL @500, FAILED @600 for the
assert>259 mutant). Pipeline: stub + `--add-library` + `--partial-loops
--unwind 5` + `--enforce-contract`, `cbmc --depth 200`.

Verified the spec is genuinely strong (not weak) at higher depth — 11 killable:
- @600: asserts >259/>=259/==259 (3); the two `% NUM == 0` → `!= 0` mutants
  (loop-entry → realloc writes a pointer field → assigns violation).
- @1200: `dist==0`→`!=0` (d_counts[-1] OOB); the 3 index sign-flips
  `[llstart-length]`/`[llstart-sym]`/`[dstart-sym]` (size_t underflow OOB,
  llstart==dstart==0); the 2 `/`→`*` start-offset mutants (huge index OOB).
Inherent survivors: dead-code loop-body mutants (loop bounds, ternaries,
offsets — only reachable inside the skipped 288/32-iter loops, blow depth) and
equivalents (assert<=259, assert!=259, and `%`→`*` which equals `%`==0 false in
the >0 regime).

Re-tested 2026-06-27: dropping ll_counts/d_counts from is_fresh+assigns (they
LOOK unused since both histogram loops are skipped in no-wrap) shifts the
frontier only ~550→~450 (still ≫200, still 0/31) AND is UNSOUND — ll_counts/
d_counts ARE written/read at lines 3447/3457-3458 (`store->ll_counts[...]++`),
so the slim spec only "passes" vacuously because depth 200 never reaches them.
Don't slim the frame.

The r_ok/w_ok prologue-shrink lever ([[zlz77gbr-rok-beats-vacuity]]) FAILS here:
w_ok is asserted (not assumed) on the nested field pointers, which aren't
allocated unless is_fresh allocates them → original fails W_OK. Nested struct
arrays need is_fresh's allocation; no cheaper validity exists. Same class as
[[silmc-depth200-vacuity]] (6 fresh, frontier ~450) but heavier.
Scripts: /app/_verify_zslld.sh, /app/kill_zslld.sh.
