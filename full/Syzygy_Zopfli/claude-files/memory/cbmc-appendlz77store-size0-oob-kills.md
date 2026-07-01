---
name: cbmc-appendlz77store-size0-oob-kills
description: ZopfliAppendLZ77Store verifies kill 0.6 by pinning store->size==0; size>=1 hits depth wall
metadata: 
  node_type: memory
  type: project
  originSessionId: fd1e4adf-643e-47e8-8882-e92375dd4f80
---

ZopfliAppendLZ77Store (zopfli.c) is a pure loop: `for (i=0;i<store->size;i++) ZopfliStoreLitLenDist(..., target)`. The callee [[cbmc-storelitlendist-depth-vacuity]] requires `target->size==3` and grows it to 4, so legally at most ONE iteration — `store->size` must be 0 or 1.

- size==1: single callee call's 12 is_fresh objects (4 source + 8 target incl. 288-wide ll_counts) exhaust CBMC depth-200; post-loop ensures never reached → vacuous, kill 0/5 (all 5 are the loop-condition `<` operator mutants; even `< -> >` giving 0 iters "passes" `size==old+1`, proving exit unreachable).
- **size==0 (winner, kill 3/5 = 0.6):** loop runs 0 times so function exit is reachable and non-vacuous. Mutants forcing an extra iteration (`< -> <=`, `>=`, `==`) deref `store->litlens[0]` OUT OF BOUNDS (source arrays not allocated) and die on memory safety, shallow — before any callee is_fresh. Contract: only `is_fresh(store)`, `is_fresh(target)`, `store->size==0`, `assigns(target->size)` (NOT object_whole on target arrays — they're nondet pointers so object_whole validity FAILS), ensures `target->size==old(target->size)`.

Survivors `< -> >` and `< -> !=`: both give 0 iters at size 0 (0>0, 0!=0 both false) = behaviorally equivalent to original → unkillable without size>=1, which reintroduces the depth wall. 0.6 is the ceiling.

**Why:** depth-200 wall makes the strong functional contract (size==1, append slot 3) vacuous; the empty-case pin trades functional strength for shallow OOB kills.
**How to apply:** for single-element-append loops whose callee pins its target size, pin the source size to 0 and let extra-iteration mutants fault on unallocated source arrays.
