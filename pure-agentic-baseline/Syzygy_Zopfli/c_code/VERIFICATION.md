# CBMC Contract Verification of `zopfli.c`

This directory contains CBMC function contracts inserted into `zopfli.c` and the
scaffolding to verify them, one function at a time, with CBMC 6.9.0.

## How verification works

CBMC 6.9.0 checks function contracts with the **DFCC** (dynamic frame condition
checking) pipeline of `goto-instrument`, which requires a real `main` entry
point and a goto binary. The flow is:

```
goto-cc        -> compile zopfli.c (+ harness + stubs) to a goto binary
goto-instrument --remove-function-body ...   (drop the stdin driver / getchar)
goto-instrument --dfcc main --enforce-contract <FN>   (wrap FN in CHECK mode)
cbmc           -> discharge the contract + memory-safety/UB checks
```

`./verify.sh <FN>` runs this for a single function; `./run_all.sh` runs the
whole annotated set and prints PASS/FAIL.

### Scaffolding files (not part of the original program)

* `harness.c` — `#include`s `zopfli.c` (so static functions are visible),
  renames the original `main` (its `getchar` loop is unsupported by DFCC),
  defines one `harness_<FN>` per target, and a `main` that calls the harness
  chosen with `-DHARNESS=...`.
* `cbmc_stubs.c` — models for `malloc`/`free`/`__CPROVER_deallocate`. This CBMC
  build leaves these as symex intrinsics with no goto body, which makes the
  DFCC contracts library fail to instrument ("no body for function 'free'").
  `malloc` still returns a real fresh object via `__CPROVER_allocate`, so bounds
  and pointer checks stay sound.
* `cbmc_include/` — a shim so the hard-coded `#include <x86_64-linux-gnu/...>`
  resolves on this aarch64 host (it just forwards to the aarch64 header).
* `verify.sh`, `run_all.sh` — the drivers.

None of the program logic in `zopfli.c` was changed; only `__CPROVER_*`
contracts were inserted.

## Check sets

* **STRONG** — `--bounds-check --pointer-check --div-by-zero-check
  --pointer-overflow-check --conversion-check --signed-overflow-check
  --unsigned-overflow-check --float-overflow-check --nan-check`
* **SAFE** — STRONG minus the conversion / unsigned-overflow / float / nan
  checks. Used only for functions that *intentionally* rely on unsigned
  wraparound or narrowing (e.g. the `Ran` PRNG, position deltas), where those
  stricter checks are false positives on otherwise-correct code.

## Verified functions (22, all PASS)

Pure / table-lookup leaves (STRONG): `AbsDiff`, `ZopfliGetLengthSymbol`,
`ZopfliGetDistSymbol`, `ZopfliGetDistSymbolExtraBits`,
`ZopfliGetLengthSymbolExtraBits`, `ZopfliGetLengthExtraBits`,
`ZopfliGetLengthExtraBitsValue`, `ZopfliGetDistExtraBits`,
`ZopfliGetDistExtraBitsValue`, `GetLengthScore`, `zopfli_min`, `CeilDiv`,
`GetCostFixed`.

Struct / array helpers (STRONG): `PatchDistanceCodesForBuggyDecoders`,
`InitNode`, `InitRanState`, `UpdateHashValue`, `InitStats`, `ClearStatFreqs`.

Intentional-wraparound / position math (SAFE): `Ran`, `ZopfliMaxCachedSublen`,
`ZopfliLZ77GetByteRange`.

The contracts establish, per function: caller-side preconditions
(`__CPROVER_requires`, including `__CPROVER_is_fresh` for pointer arguments),
frame conditions (`__CPROVER_assigns`), and functional/range postconditions
(`__CPROVER_ensures`) — and CBMC additionally discharges array-bounds, pointer,
shift, division, and overflow safety inside each body.

## Notes / limitations

* A few postconditions were kept range-/shape-based rather than bit-exact where
  the exact form is a table the contract cannot restate, or where a bit-exact
  form forces intractable symbolic 64-bit division (see `CeilDiv`).
* The remaining functions in `zopfli.c` (the optimizer / deflate driver, the
  dynamic-array `ZOPFLI_APPEND_DATA` writers, the recursive package-merge, etc.)
  are large, allocation-heavy, and loop-unbounded; CBMC cannot verify all of
  them within bounded model checking, consistent with the task's expectations.
