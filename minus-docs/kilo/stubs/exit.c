#include <stdlib.h>

/* FUNCTION: exit */

/* A model of `exit` that terminates the current execution path.  Providing a
 * body for `exit` keeps `run-cbmc` from taking its "missing body" retry
 * path, which injects CBMC's bundled C-library models via
 * `goto-instrument --add-library`.  Those models give `free`/`malloc` bodies
 * that introduce a nondeterministic `should_malloc_fail` /
 * `__VERIFIER_nondet___CPROVER_bool` symbol, which crashes
 * `goto-instrument --enforce-contract`'s assigns-frame instrumentation ("no
 * definite size for lvalue target ... __CPROVER_deallocate").
 *
 * `exit` never returns, so `__CPROVER_assume(0)` makes any execution that
 * reaches it infeasible, which is the correct semantics: no code after the
 * `exit` call runs along that path. */
void exit(int status)
{
    (void)status;
    __CPROVER_assume(0);
    /* Unreachable. */
    while (1) {}
}
