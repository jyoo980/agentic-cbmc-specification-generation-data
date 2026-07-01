---
name: getfixedtree-depth200-vacuity
description: "GetFixedTree maxes at 3/25 under --unwind 5 --depth 200 via post-loop-1 assert; loops 2-5 past depth frontier, don't chase further"
metadata: 
  node_type: memory
  type: project
  originSessionId: 4be0d5d1-5361-4256-b5c9-8c89efa3a1d4
---

GetFixedTree (zopfli.c) fills ll_lengths[0..287] (8/9/7/8 split) and d_lengths[0..31]=5 via 5 fixed-bound loops. avocado generates 25 mutants, all relational changes to the 5 loop bounds (`< B` -> `<=,>,>=,==,!=`).

Verifies SUCCESSFUL under the harness with a full-strength contract: is_fresh on both arrays (ZOPFLI_NUM_LL=288, ZOPFLI_NUM_D=32), assigns object_whole on both, and exact-value forall ensures per loop segment, plus in-loop index-bound asserts.

CORRECTION (2026-06-27): the prior "0/25 inherent" was WRONG — it only considered in-loop asserts + the final forall (both vacuous). Adding a **post-loop "loop ran" assert** after each loop (`assert ll_lengths[FIRST]==EXPECTED`) raises the score to **3/25**. A zero-iteration bound mutant (>,>=,==) skips the loop, leaving the first element at its havoced object_whole value, so the post-loop assert fails → killed. Verifies SUCCESSFUL on the original (loops run >=1 of their 5 unwound iters, writing that element).

Why only 3, not 15: depth-bounded reachability. Measured frontier on the real file — prologue (is_fresh + object_whole havoc over the 288- and 32-elt arrays) ≈ depth 80-120; each unwound loop ≈ +100. Post-loop-1 assert reachable in (180,200]; post-loop-2 in (250,300]; post-loop-3 in (300,400]; loops 4-5 past 400. So at the fixed --depth 200 only loop-1's post-assert is reached → kills mutants 1,2,3 (loop1 >,>=,==). Loops 2-5's post-asserts pass vacuously (past frontier). Can't shrink prologue without weakening the required memory contract.

Remaining 22 unkillable under --unwind 5: the five `<=` extra-iteration mutants only diverge at a boundary index beyond the 5-iter frontier; the five `!=` are equivalent; loops 2-5's 12 zero-iter mutants are past the depth-200 frontier. **3/25 is the achievable max.** Same depth-frontier class as [[avocado-depth200-vacuity]] but partially beatable via post-loop asserts — worth trying this trick on other fixed-bound-loop writers.

Kill harness: /app/kill_gft.sh (generates the 25 mutants, enforce-contract, --depth 200). Reach-test trick: replace a post-loop assert's cond with `0`; SUCCESS=unreachable, FAILURE=reachable.
