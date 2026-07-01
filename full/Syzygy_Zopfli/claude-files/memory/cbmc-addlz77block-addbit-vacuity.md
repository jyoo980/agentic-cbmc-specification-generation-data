---
name: cbmc-addlz77block-addbit-vacuity
description: AddLZ77Block verifies soundly but kill 0/14; AddBit 1==0 poison + depth-200 wall
metadata: 
  node_type: memory
  type: project
  originSessionId: 46299223-76e9-40b3-b440-d4c9101e0a8a
---

`AddLZ77Block` (Syzygy_Zopfli/c_code/zopfli.c) verifies but kills 0/14 mutants — structural, not fixable under harness flags.

**Why:** Contract restricts btype to {1,2} (its documented domain). On both paths the first effect call is `AddBit(final, bp, out, outsize)`, and AddBit's replaced contract carries `__CPROVER_ensures(1 == 0)` (its own poison, see [[cbmc-addnoncompressedblock-addbit-vacuity]]). So `assume(1==0)` is injected after the first AddBit, making every later statement (remaining AddBits, GetFixedTree/GetDynamicLengths, AddDynamicTree, ZopfliLengthsToSymbols, AddLZ77Data, AddHuffmanBits, the postconditions) vacuous. Mutants 1–9,11–14 all live past that point.

The one potentially-killable mutant is `if (btype == 0)` → `!=` (flips onto the byte-range/AddNonCompressedBlock branch whose preconditions the contract intentionally doesn't supply). It survives too: the depth-200 budget is exhausted during is_fresh setup before that branch's precondition assertion is reached, so the mutant also verifies vacuously.

**How to apply:** Don't chase kills here — AddBit's 1==0 poison cannot be removed (forbidden to edit C; it's intentional for AddBit's own vacuous verification). Same depth-200 wall as the whole output-writing family. Build needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` (FILE.h stub). Contract requires: options/lz77/bp/outsize/out/*out is_fresh, *bp<=7, *outsize==3, final 0/1, btype 1||2; assigns(*bp,*outsize,object_whole(*out)); ensures *bp<=7 && *outsize>=old.
