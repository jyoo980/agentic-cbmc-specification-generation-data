---
name: cbmc-findminimum-fnptr-param
description: FindMinimum in zopfli.c unverifiable — FindMinimumFun fn-ptr param crashes goto-instrument during Function Pointer Removal
metadata: 
  node_type: memory
  type: project
  originSessionId: d26ffbf3-dcea-4f7f-91b8-b41ebf9b0900
---

`FindMinimum` (zopfli.c ~line 4714) takes a `FindMinimumFun f` parameter, i.e. a
function pointer `double (size_t, void*)`. Under `--enforce-contract`,
goto-instrument's **Function Pointer Removal** pass replaces the `f(i, context)`
call with a switch over every same-signature function in the file (SplitCost →
EstimateCost → ... → BoundaryPM). BoundaryPM's recursion then triggers
"Numeric exception : 0" / "Recursive call to 'BoundaryPM' during inlining" and
the run dies before any solving. Unverifiable regardless of spec — same family as
[[cbmc-getcostmodelmincost-fnptr-param]] (fn-ptr param crashes the harness).

Strong sound spec left in place: requires(f!=NULL), requires(start<end),
is_fresh(smallest), assigns(*smallest), ensures(return in [start,end)).
