---
name: cbmc-depth-vacuous-and-recursive-callees
description: "Two CBMC gotchas in the kilo repo — --depth silently makes postconditions vacuous, and recursive callees crash the contract inliner (fix with a no-op stub body)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 73f630cf-61d6-46df-869f-3d6224ca55ca
---

While verifying `editorUpdateRow` in /app/kilo/kilo.c, two non-obvious CBMC issues bit me:

**1. `--depth N` makes postconditions vacuously pass.** With `cbmc --depth 400`, every
`__CPROVER_ensures` reported SUCCESS even when deliberately set to `row->rsize == 999`.
The depth bound truncates the symbolic path *before* reaching the postcondition check, so it
is never evaluated → false "VERIFICATION SUCCESSFUL" and a near-zero mutation kill score
(only mutants failing an *early* bounds check were killed). Fix: drop `--depth` entirely when
loops are already bounded by `--unwind` + `--unwinding-assertions`. Always sanity-check that a
contract is live by temporarily falsifying an ensures and confirming `postcondition.* : FAILURE`.

**2. Recursive callees crash the contract inliner.** `editorUpdateSyntax` self-calls on the
`E.syntax!=NULL` path; enforcing a *caller's* contract while inlining it gives
"Recursive call ... during inlining / Numeric exception : 0". `goto-instrument
--remove-function-body F` does NOT fix it cleanly: a call to the now-bodyless F raises
`<caller>.no-body.F: FAILURE`. Working fix: remove kilo's body, then link a separate no-op
stub providing F's body so the call returns normally:
`goto-cc -o stub.goto stub_F.c` then `goto-cc -o x.goto x.goto stub.goto --function <caller>`.
The stub uses an incomplete `typedef struct erow erow;` and never derefs the pointer. This is
sound here only because the callee runs *after* all mutated logic. See
[[cbmc-isfresh-mutation-kills]].

**3. Killing spurious-`exit()` mutants with an exit stub.** Mutants that corrupt the
`allocsize > UINT32_MAX` overflow guard into firing (`<`, `<=`, `!=`) call `exit(1)`. CBMC
models `exit` as no-return (`assume(false)`), which *prunes* that path, so the postcondition
is never checked → the mutant SURVIVES. Under `editorUpdateRow`'s precondition (`row->size==1`)
allocsize is a tiny constant, so the guard is provably false and `exit` is unreachable. Linking
a stub that models `exit` as `__CPROVER_assert(0); __CPROVER_assume(0);` turns "reaching exit"
into a failure: original still verifies (exit unreachable), and the 3 guard-firing mutants are
now KILLED. Stub file `stub_exit.c` (defines `void exit(int)`), linked like the editorUpdateSyntax
stub. This raised editorUpdateRow's kill score 16/26 → 19/26.

**4. The canonical avocado harness (tools/run_cbmc.py) CANNOT score this function.** It runs
`--partial-loops --unwind 5 ... --depth 200` with `--replace-call-with-contract editorUpdateSyntax`
(NOT the local script's `--unwind 10`, no-depth, remove-body stub, explicit `--*-check` flags).
At `--depth 200` everything is vacuous (issue #1) → 0/26. At full depth, `--partial-loops` lets
loops fall through over-iterated, so even the ORIGINAL code fails (render[idx] OOB, assigns,
postcondition). So depth 200 is what makes canonical proofs "succeed" by truncation. Net: this
spec can only be meaningfully scored by the repo's LOCAL scripts, not the canonical harness.
The local scripts' `--depth`-free, no-`--partial-loops` run is the real kill-score measure.

Result: editorUpdateRow verifies, kill score 19/26 (was 16/26); the 7 survivors are all equivalent
mutants (`j != size` ≡ `j < size`; `nonprint` is dead/always 0) or the `allocsize > UINT32_MAX`
overflow guard (needs a ~4GB row to exercise — infeasible in BMC).
Helper scripts: verify_editorUpdateRow.sh, run_mutants_editorUpdateRow.sh, stub_editorUpdateSyntax.c,
stub_exit.c; plus verify_canonical_editorUpdateRow.sh + run_mutants_canonical_editorUpdateRow.sh
(demonstrate the canonical-harness 0/26).
