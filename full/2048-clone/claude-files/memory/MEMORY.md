# Memory Index

- [CBMC depth-200 vacuous loops](cbmc-depth-200-vacuous-loops.md) — fixed --depth 200 makes loop-heavy specs pass vacuously (0 kill score)
- [CBMC depth-200 starves later ensures](cbmc-depth200-starves-later-ensures.md) — depth-200 truncates 2nd+ ensures in init-heavy straight-line funcs; use w_ok, single requires, static-const table, important ensures first
- [CBMC depth-200 postcondition eval cost](cbmc-depth200-postcondition-eval-cost.md) — multi-cell predicate eval (~80 derefs) alone overruns depth 200 even with trivial body; strong specs go vacuous/0-kill, keep them anyway
- [Stubs dir for external callees](stubs-dir-for-external-callees.md) — stub unmodeled library calls via /app/stubs/*.c with /* FUNCTION: name */ marker
- [r_ok vs is_fresh depth vacuity](rok-vs-isfresh-depth-vacuity.md) — r_ok in requires can exhaust --depth 200 → vacuous 0-kill; use is_fresh instead
- [depth-200 loops over callee contracts](depth200-callee-contract-deep-paths.md) — loop over contract-replaced callees: only shallow mutants killable; lead with cheap snapshot-free postcondition (e.g. left-packed); too many __CPROVER_old kills shallow kills too
- [I/O func static-state unkillable](io-func-static-state-unkillable.md) — void funcs w/ only static-local + external-call effects: empty assigns is strongest; branch mutants survive inherently (0 kill)
- [CBMC old-snapshot callee-in-loop crash](cbmc-old-snapshot-callee-in-loop-crash.md) — create_car_expr Unreachable abort when a __CPROVER_old two-clause callee contract is replaced inside a caller's loop; tool defect, not spec
