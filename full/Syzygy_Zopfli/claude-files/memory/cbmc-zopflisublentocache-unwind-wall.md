---
name: cbmc-zopflisublentocache-unwind-wall
description: ZopfliSublenToCache verifies at kill 0.26; survivors are unwind-5/internal-assert bound
metadata: 
  node_type: memory
  type: project
  originSessionId: e2685254-c6e5-46dc-aaa6-2af386592ec1
---

ZopfliSublenToCache (zopfli.c ~2799) verifies SOUNDLY at kill 0.2632 (10/38) — well above the kill-0 norm for loop-heavy funcs here.

Working contract: `requires pos < ZMCS_MAXPOS` (=4, reuse the callee macro), `length <= ZOPFLI_MAX_MATCH`, `is_fresh(lmc, sizeof)`, `is_fresh(lmc->sublen, ZOPFLI_CACHE_LENGTH*ZMCS_MAXPOS*3)` (96 bytes — matches the replaced ZopfliMaxCachedSublen contract called in the final assert), `is_fresh(sublen, (length+2)*sizeof(short))`, and `assigns(object_upto(&lmc->sublen[24*pos], 24))`. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.

The 10 kills come from memory-safety/assigns checks on the early-iteration cache writes that are reachable within unwind 5.

**Why** the other 28 can't be killed: the loop runs `i=3..length` (up to 258) but `--partial-loops --unwind 5` only covers 5 bodies, and the `j>=ZOPFLI_CACHE_LENGTH` (8) break needs 8 if-takes — unreachable. So loop-control relational mutants (length<3, sublen[i]!=sublen[i+1], j>=8, j<8) and the in-body asserts (bestlength==length, bestlength<=length, bestlength==ZopfliMaxCachedSublen) are indistinguishable, and no postcondition can pin the loop's cumulative byte writes.

**How to apply:** sizing sublen symbolically by `length+2` vs fixed `ZOPFLI_MAX_MATCH+2` gave IDENTICAL score — depth-200 is NOT the binding constraint here; the unwind-5 wall is. Don't burn attempts chasing the post-loop/assert mutants. See [[cbmc-harness-ignores-loop-contracts]], [[cbmc-getbyterange-concrete-pin-kills]].
