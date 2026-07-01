#include <stdio.h>

/* FUNCTION: perror */

/* A no-op model of `perror`.  Providing a body keeps `run-cbmc` from
 * taking its "missing body" retry path, which injects CBMC's bundled C-library
 * models and crashes `goto-instrument --enforce-contract` (see `exit.c`).
 * `perror` only writes a diagnostic to stderr, which is irrelevant to
 * verification, so the model does nothing. */
void perror(const char *s)
{
    (void)s;
}
