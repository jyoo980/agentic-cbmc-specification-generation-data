---
name: cbmc-followpath-depth-vacuity
description: "FollowPath verifies soundly but kill 0/52; reproduces every heavy hash/match/store callee, depth-200 wall"
metadata: 
  node_type: memory
  type: project
  originSessionId: ef522640-0a4a-4e82-b43d-aa5ae8d52aa1
---

`FollowPath` in `/app/Syzygy_Zopfli/c_code/zopfli.c` verifies (exit 0) with a
memory-safety + assigns contract but kill **0/52** — purely depth-vacuous.

**Why:** the body calls ZopfliResetHash, ZopfliWarmupHash, ZopfliUpdateHash,
ZopfliFindLongestMatch, ZopfliVerifyLenDist, ZopfliStoreLitLenDist. Discharging
their replaced contracts forces FollowPath's own `requires` to allocate the full
hash `h` (two 65536-int head arrays + seven 32768-entry window arrays), the block
state `s` + LMC arrays, the 8-byte input, and the full 7-array store. These
`is_fresh` objects exhaust CBMC's `--depth 200` object budget before the body is
explored, so every postcondition holds trivially and no mutant is killed.

**How to apply:** pins used to satisfy callee preconditions — `inend == 8`
(ZopfliVerifyLenDist datasize), `store->size == 3` + `pathsize == 1` (single
in-place ZopfliStoreLitLenDist append), `instart - s->blockstart < ZMCS_MAXPOS`
(lone ZopfliFindLongestMatch LMC index), plus the head/head2 chain-validity
forall for ZopfliUpdateHash. Needs `-I /app/Syzygy_Zopfli/c_code -I
/app/Syzygy_Zopfli/stubs` (FILE.h stub). Same wall as
[[cbmc-zopflifindlongestmatch-depth-vacuity]], [[cbmc-zopfliupdatehash-depth-vacuity]],
[[cbmc-storelitlendist-depth-vacuity]], [[cbmc-verifylendist-forall-pin]],
[[cbmc-depth-200-object-limit]]. Kill score cannot be raised — the object set is
irreducible.
