---
name: cbmc-function-pointer-param-crash
description: run-cbmc crashes verifying any function that takes a function-pointer parameter
metadata: 
  node_type: memory
  type: project
  originSessionId: 0aa8f19b-7957-4833-9ad2-0a597a1db614
---

In this repo, `run-cbmc` (CBMC 6.9.0, legacy `goto-instrument --enforce-contract`, no `--dfcc`) **crashes** when verifying any function whose parameter is a function pointer.

The `--enforce-contract` harness havocs the function-pointer parameter (nondet `code*` plus an `INPUT` statement), and `cbmc`'s mandatory function-pointer-removal pass then aborts with an invariant violation: `__CPROVER__start::<param>$object was not found` (exit 134). This happens regardless of contract content, even with `requires(fp == SomeFunc)` and even when no indirect calls remain in the body. Plain `cbmc` (without `--enforce-contract`) handles the same nondet function pointer fine, but the pipeline always enforces.

The proper fix — `__CPROVER_obeys_contract(fp, contract)` + pure contract symbol — fails earlier at goto-instrument: "`__CPROVER_obeys_contract` is not supported in this version" (needs DFCC, which the pipeline doesn't enable). `--restrict-function-pointer` would help but the fixed pipeline never passes it.

**Why:** It's a CBMC tool limitation, not a spec defect — `CLAUDE.md` allows that "CBMC cannot verify all correct C code."

**How to apply:** For `zopfli.c` functions taking `CostModelFun *costmodel` (`GetCostModelMinCost` — verified-with-contract-but-crashes; also `GetBestLengths`, `LZ77OptimalRun`, `ZopfliLZ77Optimal`), still write the strongest sound contract (e.g. `requires(costmodel == GetCostStat || costmodel == GetCostFixed)` + `is_fresh(costcontext, sizeof(SymbolStats))` + empty `assigns()`), but expect the run to abort at the cbmc step. Don't burn the 5-attempt budget re-trying — the crash is source-independent.

**Second crash variant (FindMinimum, takes `FindMinimumFun f`):** Here function-pointer removal *succeeds*, but the run then dies at the `goto-instrument` "Enforcing contracts" step with `Numeric exception : 0` (SIGFPE, exit 6), reported alongside "Recursive call to 'BoundaryPM' during inlining". This is a goto-instrument whole-program-inlining bug, still source-independent of the contract (the contract had already parsed/type-checked). Strongest sound contract used: `requires(f == SplitCost)` + `requires(start < end)` + `is_fresh(context, sizeof(SplitCostContext))` + `is_fresh(smallest, sizeof(double))` + `assigns(*smallest)` + `ensures return_value in [old(start), old(end))` + `ensures *smallest <= ZOPFLI_LARGE_FLOAT`. Don't re-try — neither crash mode is fixable from the source.
