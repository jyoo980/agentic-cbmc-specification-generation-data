#include <stdlib.h>

/* FUNCTION: atexit */

/* A nondeterministic model of C `atexit`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models (the same motivation documented in
 * `read.c`/`write.c`).
 *
 * `atexit` registers `func` to be called at normal program termination,
 * returning 0 on success or a nonzero value if the registration fails.  CBMC
 * does not model exit-time callbacks, so the registration has no observable
 * side effect here; the model simply returns a nondeterministic `result`
 * (0 for success, nonzero for failure). */
int atexit(void (*func)(void))
{
    (void)func;

    int result;
    return result;
}
