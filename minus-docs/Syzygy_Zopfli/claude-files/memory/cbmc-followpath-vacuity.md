---
name: cbmc-followpath-vacuity
description: "FollowPath in zopfli.c verifies but 0/52 kills — ~22 is_fresh objects exceed CBMC depth 200, body unreachable"
metadata: 
  node_type: memory
  type: project
  originSessionId: ca2b2b17-60e7-4837-9649-eaf4d6541b55
---

`FollowPath` (static, zopfli.c ~line 3132) verifies with `run-cbmc` but scores 0/52 mutation kills. Same depth-bound vacuity as [[cbmc-depth200-isfresh-vacuity]] and [[cbmc-findlongestmatch-vacuity]].

It needs ~22 `is_fresh` objects to satisfy its callees' memory-safety preconditions: `s`, `s->lmc` + cache arrays `length`/`dist`/`sublen`, hash `h` + its 8 arrays (head/prev/hashval/head2/prev2/hashval2/same), `store` + 8 arrays, `in`, and `path`. That far exceeds the ~17 in `ZopfliFindLongestMatch` (which it calls and which itself verifies vacuously). The is_fresh+malloc harness exhausts `--depth 200`, so the loop body is never reached → all 52 mutants survive.

No non-vacuous middle ground: every is_fresh is required for soundness (drop any and a callee precondition / OOB check fails). Also note loop-carried contract-replacement issues would block non-vacuous verification anyway: UpdateHash havocs the whole head[] table (object_whole assigns) so the next UpdateHash's "all head in [-1,WINDOW)" precondition can't be re-established; StoreLitLenDist requires store->size==3 each call but size grows; FindLongestMatch is called with sublen=NULL (its contract requires is_fresh(sublen,...)). Vacuity masks all of these.

Gotcha fixed during specification: the `__CPROVER_forall { path[kp] <= 258 }` precondition gave a real OOB FAILURE until `pathsize` was bounded (`pathsize >= 1 && pathsize <= inend - instart`) — unbounded pathsize lets `pathsize*sizeof` overflow and under-allocate the is_fresh object. Strong sound spec left in place.
