---
name: cbmc-zopflifindlongestmatch-depth-vacuity
description: "ZopfliFindLongestMatch verifies soundly but kill 0/157; reproduces all 3 depth-vacuous callees plus 8+ hash is_fresh objects, exhausts depth 200"
metadata: 
  node_type: memory
  type: project
  originSessionId: 958f8a85-cdda-447c-9a56-b3f4679fe529
---

`ZopfliFindLongestMatch` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies soundly
(memory-safety + assigns + `*length<=ZOPFLI_MAX_MATCH` + `pos+*length<=size`
postconditions) but mutation kill score is **0/157** (1 compile-fail), all survive.

**Why vacuous:** it is the worst depth-200 case in this codebase. It calls three
callees that are *each already depth-vacuous on their own* —
[[cbmc-trygetfromlongestmatchcache-depth-vacuity]],
[[cbmc-storeinlongestmatchcache-depth-vacuity]], [[cbmc-getmatch-param-aliasing-wall]] —
with their contracts replaced. On top of that the caller's own precondition needs
~16 is_fresh objects: s/lmc/length/dist/sublen, h, head+head2 (65536 ints each),
prev/prev2/hashval/hashval2/same (ZOPFLI_WINDOW_SIZE each), array(size),
distance, length, sublen(MAX_MATCH+2). TryGetFromLongestMatchCache runs FIRST and
on its own exhausts the depth-200 object budget, so the entire loop body
(GetMatch walk, hash-switch, StoreInLongestMatchCache) is unreachable — the false
ensures would still pass. See [[cbmc-depth-200-object-limit]].

Not fixable: cannot drop any is_fresh (every pointer is dereferenced), and the
binding constraint is object count + replaced contracts, not array size, so
concrete-pinning array (cf [[cbmc-getbyterange-concrete-pin-kills]]) does not help.

Build note: needs `-I /app/Syzygy_Zopfli/stubs` for the
`<x86_64-linux-gnu/bits/types/FILE.h>` include (cf [[cbmc-zopflilz77gethistogram-depth-vacuity]]).
