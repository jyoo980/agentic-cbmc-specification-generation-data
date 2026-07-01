---
name: addlz77block-depth200-vacuity
description: "AddLZ77Block verifies (btype==0 regime) but 0/14 kills — depth-200 vacuity, callee is_fresh preconditions cost depth ~450-600"
metadata: 
  node_type: memory
  type: project
  originSessionId: 902e03f9-a7bf-48e9-8c3e-eac907fc9704
---

AddLZ77Block (zopfli.c) is a 3-way orchestrator whose branch callees have
mutually-incompatible regime contracts under `--replace-call-with-contract`:
stored branch (btype==0) needs AddNonCompressedBlock/AddBit with `*outsize==1`;
fixed/dynamic branches need AddLZ77Data (`*outsize==3`) and AddDynamicTree
(`out==0`). No single output shape satisfies >1 branch, so the ONLY verifiable
regime is **btype==0** (one non-empty literal item: lstart==0,lend==1,dists[0]==0;
data is_fresh of size pos[0]+1; bp/out/outsize fresh, *outsize==1, *bp in 1..7).
Use r_ok (not is_fresh) for lz77/pos/dists/litlens — they only feed
ZopfliLZ77GetByteRange — to keep the prologue lean. Verifies: 0 of 28677 failed.

**Kill score is 0/14 and it's inherent depth-200 vacuity, not a weak spec.**
Verified by re-running each mutant with NO --depth bound:
- `end = pos + length` -> `pos - length`: FAILS unbounded, but threshold is
  depth ~700 (re-measured 2026-06-27: SUCCESS@400/500, FAILED@700/1000 — even
  deeper than the earlier ~450 estimate). The discriminator is
  AddNonCompressedBlock's REPLACED precondition (instart<=inend), gated behind its
  is_fresh validators. That cost is in the CALLEE's contract — cannot be shrunk
  from the caller. Pinning lz77->pos[0] to a concrete value does NOT help (still
  SUCCESS@450). Vacuously SUCCESS at 200.
- `if (btype == 0)` -> `!= 0` and ALL fixed/dynamic fall-through mutants
  (loop-bound family, assert(btype==2), if(btype==1), verbose size arithmetic):
  diverge only after the stored branch is skipped; first divergent check (2nd
  AddBit precondition) is at depth ~600. Vacuous at 200.
- `pos = lstart==lend?0:..` -> `!=`: SUCCEEDS even unbounded — genuinely
  EQUIVALENT. is_fresh checks "at least size" bytes (per docs), so the
  shrunk-to-zero range still validates against the pos[0]+1-byte data object.

Same family as [[avocado-depth200-vacuity]], [[tohfr-depth200-vacuity]],
[[zlz77gh-depth200-vacuity]]. Don't chase these kills. Kill script:
/app/kill_addlz77block.sh.
