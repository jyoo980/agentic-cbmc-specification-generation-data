#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

/* FUNCTION: getline */

/* A nondeterministic model of `getline`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models and crashes
 * `goto-instrument --enforce-contract` (see `exit.c`).
 *
 * `getline` reads a whole line, (re)allocating `*lineptr` as needed, storing the
 * buffer capacity in `*n`, and returning the number of characters read (not
 * counting the terminating NUL) or -1 on EOF/error.  The model returns either:
 *   - -1, leaving `*lineptr`/`*n` unchanged (EOF/error), or
 *   - a nondeterministic length `len` in [0, INT32_MAX], with `*lineptr` pointed
 *     at a freshly-allocated, NUL-terminated buffer of `len + 1` bytes and `*n`
 *     set to that capacity.
 *
 * The buffer is allocated with `__CPROVER_allocate` (not `malloc`) to avoid
 * CBMC's built-in malloc model.  The length is bounded by INT32_MAX so callers
 * that pass it on as a row length stay within the `int`/`size_t` ranges their
 * own contracts require.  The previously-allocated buffer (if any) is left
 * untouched, which is sound for callers that only read the newly-returned
 * contents. */
ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    (void)stream;

    ssize_t result;
    __CPROVER_assume(result >= -1 && result <= INT32_MAX);
    if (result == -1)
        return -1;

    size_t len = (size_t)result;
    char *buf = __CPROVER_allocate(len + 1, 0);
    buf[len] = '\0';
    *lineptr = buf;
    *n = len + 1;
    return result;
}
