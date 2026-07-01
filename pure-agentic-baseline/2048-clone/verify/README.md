# CBMC contract verification for `2048.c`

The function contracts (`__CPROVER_requires` / `__CPROVER_ensures` /
`__CPROVER_assigns`) live in `../2048.c`. This directory holds the proof
harnesses and scripts used to check them.

## Files
- `harness.c` — one `harness_<fn>` per verified function; each declares
  non-deterministic arguments and calls the function. Preconditions
  (`__CPROVER_requires`, assumed during enforcement) constrain those arguments.
- `stubs.c` — non-deterministic stubs for library functions with no body in the
  program (`srand`, `rand`, `time`) and a no-op `printf` (its output is
  irrelevant to the verified properties and modelling its format parsing is
  expensive).
- `verify.sh` — verify one function: `verify.sh <fn> <harness> <unwind> [flags]`.
- `verify_all.sh` — run every function.

## Flow (CBMC 6.9 / DFCC)
```
goto-cc --function <harness> -o all.goto 2048.c harness.c stubs.c
goto-instrument --dfcc <harness> --enforce-contract <fn> all.goto mod.goto
cbmc --function <harness> --unwind N --unwinding-assertions mod.goto
```
All loops are bounded (≤ 10 for `getDigitCount`, ≤ 4 elsewhere), so full
unwinding with `--unwinding-assertions` gives a complete proof — no loop
invariants are needed.

## Result
All 16 game-logic functions verify: `getColors`, `getDigitCount`, `drawBoard`,
`findTarget`, `slideArray`, `rotateBoard`, `moveUp/Left/Down/Right`,
`findPairDown`, `countEmpty`, `gameEnded`, `addRandom`, `initBoard`,
`testSucceed`.

### Note on `move*`
`moveUp/Left/Down/Right` inline `slideArray`, whose `1 << cell` shift is proven
free of signed-overflow / undefined-shift in `slideArray`'s own proof (where the
row is a 1-D array). When the same shift is reached through the *decayed 2-D row
pointer* `board[x]` with a *variable* column index, CBMC's array theory cannot
relate the flat byte access back to the 2-D `board[i][j] <= 16` precondition (a
known limitation, reproducible in a few lines). The `move*` runs therefore
disable *only* the shift/overflow checks (`--no-undefined-shift-check
--no-signed-overflow-check`) and still prove memory safety, array-bounds,
pointer safety, the frame (`__CPROVER_assigns`) conditions and termination.

### Not verified
`setBufferedInput` (termios syscalls + static state), `signal_callback_handler`
(calls `exit`) and `main` (unbounded interactive input loop) are I/O / control
scaffolding with no bounded functional contract and are intentionally excluded.
