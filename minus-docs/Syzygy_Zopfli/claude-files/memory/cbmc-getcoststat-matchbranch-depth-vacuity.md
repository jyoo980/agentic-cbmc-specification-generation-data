---
name: cbmc-getcoststat-matchbranch-depth-vacuity
description: "GetCostStat verifies but 0/4; only the dist!=0 match branch is depth-unreachable, where all 4 mutants live"
metadata: 
  node_type: memory
  type: project
  originSessionId: 239101ac-de10-466f-805d-7ab5dd200d3f
---

`GetCostStat` in zopfli.c verifies NON-vacuously but scores 0/4. Unusual sub-case of the depth-vacuity family: only ONE branch is depth-unreachable.

- The `dist==0` (literal) branch IS reachable — proven exactly: `return == ((SymbolStats*)context)->ll_symbols[litlen]`, bounded `[0,64]`. A false unconditional ensures FAILS there (confirms reachability).
- The `dist!=0` (match) branch is UNREACHABLE at --depth 200: a false ensures guarded by `(dist!=0) ==> return==999999` still "verifies successfully". Cause is the 4 contract-replaced callee calls in that branch (ZopfliGetLengthSymbol/LengthExtraBits/DistSymbol/DistExtraBits), NOT my preconditions — removing the two large `forall` (288+32 elem) bound requires did NOT make the branch reachable.
- All 4 survivors live in / depend on the match branch: the `if(dist==0)->!=` branch-swap (dist==0 input then hits ZopfliGetDistSymbol(0), whose replaced-contract requires dist>=1 is assumed → assume(false) kills the path) and 3 `+ -> -` sign flips on `lbits+dbits+ll_symbols[lsym]+d_symbols[dsym]`.
- The literal branch has no mutable operators (`return ll_symbols[litlen]`), so 0 kills is the ceiling regardless of spec strength.

Strong sound spec left in place (is_fresh(context,sizeof(SymbolStats)), litlen<=258, dist<=32768, [0,64] forall bounds on ll_symbols/d_symbols, exact dist==0 functional ensures, [0,146] total bound). Related: [[cbmc-getcostfixed-lsym-contract-havoc]] (same callee set, constant context, 8/13), [[cbmc-findlargestsplittableblock-vacuity]] (forall-driven depth vacuity — here foralls were NOT the cause).
