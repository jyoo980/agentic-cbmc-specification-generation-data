# CBMC contracts for kilo.c

This directory holds the harness, stubs and driver scripts used to verify the
CBMC function contracts inserted into `../kilo.c`.

## Running

```
./verify_all.sh              # verify every contracted function
./verify.sh <fn> [unwind]    # one function, old-style enforcement
./verify_dfcc.sh <fn> [unwind]  # one function, DFCC enforcement (frees clauses)
```

`verify_all.sh` reports `passed: 9   failed: 0`.

## Verified functions (9)

| function                 | flow | what the contract guarantees |
|--------------------------|------|------------------------------|
| `is_separator`           | old  | result is 0/1; safe for any unsigned-char/EOF argument |
| `editorSyntaxToColor`    | old  | result in 31..37; no side effects |
| `editorFileWasModified`  | old  | returns `E.dirty`; no side effects |
| `editorRowHasOpenComment`| old  | memory-safe last-cell inspection; result 0/1; no side effects |
| `abAppend`               | old  | memory-safe grow; writes only the buffer pointer/length |
| `disableRawMode`         | old  | only `E.rawmode` changes, ending cleared |
| `editorAtExit`           | old  | only `E.rawmode` changes, ending cleared |
| `abFree`                 | dfcc | frees only the (NULL-or-fresh) buffer |
| `editorFreeRow`          | dfcc | frees only the three (NULL-or-fresh) row buffers |

`old` = `goto-instrument --enforce-contract`; `dfcc` =
`goto-instrument --dfcc --enforce-contract` (needed for `frees` clauses).

## Files

- `stubs.c`   — nondeterministic models for `__ctype_b_loc` and `tcsetattr`.
- `harness.c` — DFCC entry points (`h_<fn>`); `#include`s `kilo.c` so the real
  types/globals are available and `kilo.c` carries only specifications.
- `verify.sh`, `verify_dfcc.sh`, `verify_all.sh` — drivers.

## Notes on what could not be strengthened, and why

The contracts above are restricted to functions that operate on a single
caller-supplied object (an `erow*`/`struct abuf*`) or on scalar global state.
Several CBMC-6.9 limitations blocked stronger/broader contracts; they are
recorded here so the boundary is explicit rather than accidental:

- **`__CPROVER_forall` over an `__CPROVER_is_fresh` array is not honoured** as a
  precondition assumption (verified in both enforcement modes). This makes it
  impossible to bound the per-element fields (`size`, `idx`, …) of the global
  `E.row[]` array, so every function that iterates that array and does
  arithmetic on those fields (`editorMoveCursor`, `editorDelRow`,
  `editorInsertRow`, `editorRowsToString`, …) trips the default signed-overflow
  checks with no way to constrain the inputs.
- **Old-style enforcement cannot instrument a function that contains a loop**
  (`Loops remain ... assigns clause checking instrumentation cannot be
  applied`), and **does not support `frees` clauses**.
- **DFCC** supports loops and `frees`, but instruments the bodies of library
  functions it inlines: string functions with internal loops (`strlen`,
  `strstr`, `strchr`, the `isspace` table walk) then fail their own
  frame/bounds checks, and DFCC additionally crashes (`Numeric exception`) on
  some functions that own a local stack array.
- The `editorRowInsertChar` / `…AppendString` / `…DelChar` mutators all funnel
  through `editorUpdateRow`, which in turn calls the **recursive** `strlen`-using
  `editorUpdateSyntax`; abstracting that chain with replacement contracts is
  possible in principle but the DFCC proof of `editorUpdateRow` did not converge
  within practical time even at a rendered length of 2.
- `errno` expands to `*__errno_location()`, a function call, which is rejected
  inside an `assigns` clause — so functions whose only extra effect is setting
  `errno` (e.g. `enableRawMode`) cannot get a precise frame.
