---
name: calculatetreesize-depth200-vacuity
description: "CalculateTreeSize maxes 3/12 kills under hardcoded depth 200; loop-body cost makes return vacuous, EncodeTree contract only gives >=26"
metadata: 
  node_type: memory
  type: project
  originSessionId: 16a930d0-92d3-4e05-813e-bb154e41d2dd
---

CalculateTreeSize (Syzygy_Zopfli/c_code/zopfli.c ~line 1373) loops 8x calling
EncodeTree in size-only mode and returns the min. Spec: forward EncodeTree's
requires (is_fresh ll_lengths[288]/d_lengths[32], all <=15), `__CPROVER_assigns()`,
`__CPROVER_ensures(__CPROVER_return_value >= 26)`. Verifies SUCCESSFUL.

Kill score under avocado's hardcoded `--depth 200`: **3/12** (loop-boundary
mutants i>8, i>=8, i==8 — these make the loop never run, so `return` is reached
on a SHORT path within depth 200 and result==0 fails `>=26`).

The other 9 survive under depth 200 due to [[avocado-depth200-vacuity]]: the
heavy per-iteration cost (EncodeTree's replaced-contract is_fresh/forall over 288
elems) pushes `return`/postcondition past depth 200, so all loop-dependent
assertions are vacuously SUCCESS. Proven: mutants `result==0 && size<result` and
`result!=0 || size<result` (both leave result==0) pass at depth 200 but FAIL
(killed) at `--depth 2000`.

Re-measured 2026-06-27: idx5 (`result!=0||size<result`) kill threshold is between
depth 300 (SUCCESS) and 350 (FAILED) — far above the harness's fixed `--depth 200`.
Stripping BOTH foralls from EncodeTree's requires does NOT lower it (still SUCCESS
at 200, even 100), so the foralls are NOT the depth hog — the cost is structural
(5 loop unwinds × EncodeTree contract-replacement is_fresh expansion/havoc), which
no allowed spec change can shave. Confirms 3/12 is the hard depth-200 ceiling.

Even at unbounded depth the ceiling is 5/12: the 4 size-comparison mutants
(`size <=/>/>=/==/!=`... specifically >, >=, ==, !=) and i<=8 can't be killed
because EncodeTree's size-only contract only guarantees return >= 26, not an
exact value, so min-vs-max-vs-whatever all satisfy >=26. `size <=` and `i != 8`
are equivalent mutants. Don't chase — spec is maximally strong.
