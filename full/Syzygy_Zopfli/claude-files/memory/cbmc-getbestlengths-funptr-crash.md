---
name: cbmc-getbestlengths-funptr-crash
description: GetBestLengths undischargeable; CostModelFun* param crashes enforce harness (costmodel$object not found)
metadata: 
  node_type: memory
  type: project
  originSessionId: 12694355-3398-457a-82ed-4677ba85b45c
---

`GetBestLengths` (zopfli.c ~line 4010) is UNDISCHARGEABLE, same root cause as
[[cbmc-getcostmodelmincost-funptr-param]] and [[cbmc-getmatch-param-aliasing-wall]].

It takes a `CostModelFun *costmodel` parameter, passes it to `GetCostModelMinCost`,
and calls it directly in the body (`costmodel(in[i],0,ctx)` etc). Under
enforce-contract the param is havoc'd; function-pointer removal then references an
internal `__CPROVER__start::costmodel$object` symbol the generated start harness
never declares → CBMC aborts: "Invariant check failed ... identifier
__CPROVER__start::costmodel$object was not found" (exit 134), before any property.

Things that DON'T help (tried):
- `__CPROVER_requires(costmodel == &GetCostFixed)` — same $object crash (the
  comparison itself needs costmodel$object).
- Removing the pin entirely — still crashes (body's direct funptr call / the
  GetCostModelMinCost call trigger it).
- `__CPROVER_obeys_contract(costmodel, CostModelContract)` — "not supported in
  this version" at goto-instrument (needs --dfcc which harness omits). Confirmed
  by running GetCostModelMinCost: exit 6 "obeys_contract is not supported".

Side note: GetCostModelMinCost's in-file contract used obeys_contract (broken). I
rewrote it to plain assigns()/ensures(>=0) so it's at least instrumentable as a
replaced callee, but GetCostModelMinCost itself stays undischargeable (own funptr
param).

Left a strong memory-safety contract on GetBestLengths (mirrors FollowPath:
is_fresh on s/lmc/h + all hash arrays, head/head2 forall, inend<=ZMCS_MAXPOS pin,
length_array/costs is_fresh, assigns hash+lmc+DP arrays, ensures return>=0) with a
NOTE documenting the crash. Needs `-I /app/Syzygy_Zopfli/stubs` for FILE.h.
