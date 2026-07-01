---
name: zmin-equiv-mutant
description: "zopfli_min exact-value contract maxes at 4/5; the a<=b survivor is an equivalent mutant, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: f9b051ff-afe2-4755-8331-d98df788940a
---

`zopfli_min(a,b)` returns `a < b ? a : b`. Leaf, no loops, no depth-200 issue. Verifies at depth 200 with an exact-value contract: `__CPROVER_assigns()` + `ensures(__CPROVER_return_value == (a < b ? a : b))` (plus min-property ensures).

Kill score maxes at **4/5**. The surviving mutant `a <= b ? a : b` is an **equivalent mutant**: it differs from `a < b` only when `a==b`, where original returns `b` and mutant returns `a` — equal values, identical output for all inputs. No value-based spec can kill it. Don't chase. Same family as [[absdiff-equiv-mutant]], [[zgds-equivalent-mutant]].
