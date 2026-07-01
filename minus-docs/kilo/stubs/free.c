#include <stdlib.h>

/* FUNCTION: free */

/* A no-op model of `free` that avoids CBMC's built-in `free` model.  The
 * built-in model lowers to `__CPROVER_deallocate`, whose body assigns a
 * nondeterministic `__VERIFIER_nondet___CPROVER_bool` temporary.  That
 * assignment crashes `goto-instrument --enforce-contract`'s assigns-frame
 * instrumentation ("no definite size for lvalue target ...
 * __CPROVER_deallocate") whenever the function under verification calls `free`
 * in its body (as `editorOpen` does).  Replacing a callee that uses
 * `__CPROVER_is_fresh` forces `run-cbmc` to inject the bundled library
 * via `--add-library`, so we cannot simply avoid that path.
 *
 * Not actually deallocating is sound: every pointer stays valid, so no spurious
 * use-after-free or double-free can arise, and the callers in this code base do
 * not depend on memory being reclaimed.  (Memory-leak checking is not enabled by
 * the verification command.) */
void free(void *ptr)
{
    (void)ptr;
}
