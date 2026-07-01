---
name: cbmc-copystats-no-mutable-operators
description: "CopyStats in zopfli.c verifies non-vacuously but mutation testing reports \"no mutable operators\" (memcpy-only body)"
metadata: 
  node_type: memory
  type: project
  originSessionId: a64027b0-92e6-4800-9b58-28e8c23e7810
---

CopyStats (zopfli.c) verifies successfully and NON-vacuously: only 2 is_fresh
objects (source, dest), no loops, so the body and all 4 forall ensures clauses
are reached well within --depth 200. Spec: is_fresh source+dest, assigns all 4
dest arrays (litlens/dists/ll_symbols/d_symbols), forall element-equality
ensures for each.

Mutation testing prints "no mutable operators" — the body is four straight
memcpy() calls, so there is nothing for the mutation engine to mutate. Kill
score is N/A (not low/vacuous). This is the strongest possible outcome for such
a function. Distinct from the vacuity cases (e.g. [[cbmc-calculatestatistics-inline-array-isfresh]]);
unlike the double-array sibling [[cbmc-calculateentropy]], the ll_symbols/d_symbols
double copies verify with plain == element equality.
