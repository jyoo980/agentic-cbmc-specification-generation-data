---
name: cbmc-calculateblocksize-dispatcher-depth-vacuity
description: ZopfliCalculateBlockSize verifies soundly but kill 0/6; depth-200 truncates even the cheap btype==0 byte-range path
metadata: 
  node_type: memory
  type: project
  originSessionId: 045c4885-595e-43ff-8454-80498c2e7958
---

`ZopfliCalculateBlockSize` (zopfli.c) is a btype dispatcher: btype==0 →
ZopfliLZ77GetByteRange + ceil-block arithmetic; btype==1 → GetFixedTree +
CalculateBlockSymbolSize; else → GetDynamicLengths. I wrote a full all-btype
contract (guarded preconditions, since btype==0 needs lend<=2 while btype!=0 needs
lstart==0 && lend>=288*3 — guard each branch's requires with `btype==0 ==>` /
`btype!=0 ==>`, including the is_fresh clauses). Postconditions: exact functional
formula for btype==0, tight [3+CBSGC_RESULT_MIN, 3+CBSGC_RESULT_BOUND] for btype==1,
lower bound 3+26+316 for else. Verifies soundly, **kill 0/6**.

**Why kill 0:** the harness fixes `--depth 200`. Even the cheapest branch
(btype==0, only ZopfliLZ77GetByteRange's contract replaced) needs ~450-500 symbolic
steps before its ensures is checked — the is_fresh setup of the store + pos/dists/
litlens plus the replaced callee's own is_fresh requires-assertions and ensures-
assume exhaust depth 200 first. Confirmed empirically by building the `+`->`-`
return mutant by hand and running cbmc directly: VERIFICATION SUCCESSFUL at
--depth 200/400, **FAILED at --depth 500 and unbounded**. So the contract is
correct and strong and discharges fully once the depth wall is lifted; it is just
vacuous under the harness. Simplifying the formula (pin lend==1 so pos terms
cancel) did NOT help — the cost is the is_fresh / replaced-contract enforcement,
not the formula. See sibling [[cbmc-getbyterange-concrete-pin-kills]] (value
postcondition truncated ~212 there) and [[cbmc-getdynamiclengths-no-mutable-operators]].

Build note: needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs`
(stubs supplies the x86_64 FILE.h). Vacuity diagnostic that works: replace an
ensures RHS with an impossible constant (e.g. `== 999999`); if it still "verifies",
that path is depth-truncated/vacuous.
