#include <stdio.h>

/* FUNCTION: fclose */

/* A nondeterministic model of `fclose`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models and crashes
 * `goto-instrument --enforce-contract` (see `exit.c`).  `fclose` returns 0 on
 * success or EOF on failure; the model returns an unconstrained value so callers
 * cannot rely on a particular result. */
int fclose(FILE *stream)
{
    (void)stream;
    int result;
    return result;
}
