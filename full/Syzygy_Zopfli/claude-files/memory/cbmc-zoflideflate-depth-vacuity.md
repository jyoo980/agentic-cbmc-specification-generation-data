---
name: cbmc-zoflideflate-depth-vacuity
description: "ZopfliDeflate verifies soundly but kill 0/18; depth-200 wall + single-iteration callee, twin of ZopfliDeflatePart"
metadata: 
  node_type: memory
  type: project
  originSessionId: df4de112-2f2a-470f-9e80-a3c6ff22ab77
---

`ZopfliDeflate` (zopfli.c:6366) verifies SOUNDLY (exit 0) but kill 0/18 — depth-200 vacuity wall, the top sibling of [[cbmc-zoflideflatepart-depth-vacuity]].

Contract mirrors ZopfliDeflatePart's preconditions (is_fresh options/in/bp/out/*out, btype 0..2, final 0..1, *bp<=7, *outsize==3, is_fresh(*out,8)) plus pinned `insize==8`. assigns={*bp,*outsize,object_whole(*out)}, ensures *outsize>=old.

Body is a do/while chopping input into MASTER_BLOCK (1000000) chunks, one `ZopfliDeflatePart` call per trip, then a verbose fprintf. Forced single iteration: callee requires *outsize==3 on entry but ensures only >=old, so a 2nd trip can never re-establish the precondition — single call is the only feasible shape. Pinning insize==8 makes masterfinal true on trip 1 and keeps is_fresh(in,insize) well-formed.

**Why kill 0:** all 18 survivors are loop arithmetic (masterfinal, size=insize-i, i+size, final2 &&) behind the one ZopfliDeflatePart call whose replaced contract havocs *out/*outsize/*bp and exhausts the depth-200 budget via accumulated is_fresh ensures before any mutated operator evaluates; the rest are arithmetic inside the verbose fprintf args (reads only, no contract-observable effect). Hard wall, not a spec defect.

**How to apply:** needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h (fprintf/stderr). Don't chase the kill — same dead end as the whole driver family.
