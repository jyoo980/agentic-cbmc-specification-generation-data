---
name: absdiff-equiv-mutant
description: "AbsDiff exact-value contract maxes at 6/7; the x>=y survivor is an equivalent mutant, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: e376b3d9-0f11-4819-9a29-ec05aae44479
---

`AbsDiff(size_t x, size_t y)` in `/app/Syzygy_Zopfli/c_code/zopfli.c` (returns |x-y|).
An exact-value contract (`x>=y ==> ret==x-y`, `x<=y ==> ret==y-x`) verifies and
kills 6 of 7 mutants. The lone survivor `if (x > y)` → `if (x >= y)` is an
**equivalent mutant**: it differs only at x==y, where both then/else branches
return 0 (x-y == y-x == 0), so it is behaviorally identical. 6/7 is the max; don't chase it.

**Why:** Mirrors [[zgds-equivalent-mutant]] / [[zgdebv-equivalent-mutants]] — comparison-operator
mutants that collapse to equality of both branches are unkillable.

**How to apply:** Leaf func, no callees. Kill-test harness must pass `-I /app/Syzygy_Zopfli/c_code`
to goto-cc or `#include "zopfli.h"` fails on the /tmp copy and EVERY mutant falsely reads as
"killed" (empty cbmc output mis-scored). Verify build succeeds before trusting kill counts.
