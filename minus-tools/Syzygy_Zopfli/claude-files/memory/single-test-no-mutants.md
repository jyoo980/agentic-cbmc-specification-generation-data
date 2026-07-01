---
name: single-test-no-mutants
description: "single_test test-driver — avocado generates 0 mutants, verifies as-is, kill score inherently 0, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: 8e15ee9f-5352-4ddb-bbac-785339dd997f
---

`single_test` (zopfli.c ~line 6149): void test-driver. Sets up a `ZopfliOptions`,
zeroes `out/outsize/bp` locals, `insize=strlen(in)`, then calls `ZopfliDeflate`.

avocado generates **0 mutants** ("no mutable operators") → kill score inherently 0,
cannot be increased. **It already verifies** (PASS rc=0) with NO contract.

Official flow (from zopfli-cbmc-runs.jsonl): `--partial-loops --unwind 5`, then
`--replace-call-with-contract ZopfliDeflate --enforce-contract single_test`,
`cbmc --depth 200`, with `strlen` nondet-stubbed.

Why no spec can help / don't add one:
- 0 mutants → 0 kills regardless of spec strength.
- C code hardcodes `bp=0, outsize=0, out=NULL` and passes `btype` straight through;
  these structurally violate ZopfliDeflate's replaced-contract requires
  (`*bp∈1..7`, `*outsize==1`, fresh non-null `*out`, `btype==0`). Any non-vacuous
  regime contract is unverifiable. It only passes because those precondition
  assertions sit past the --depth 200 frontier (same depth-200 vacuity family).
- Outputs are local + leaked; no observable postcondition to assert.

Left C unchanged. Runner: `/app/_run_st.py <fn>` (run from /app; the c_code dir has a
`bisect.py` that shadows stdlib, so don't `cd` there for python). Related: [[zdef-depth200-vacuity]].
