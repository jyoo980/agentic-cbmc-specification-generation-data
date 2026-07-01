#include <string.h>

/* FUNCTION: strlen */

/* A nondeterministic model of C `strlen`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models (the same motivation documented in
 * `read.c`/`write.c`).
 *
 * `strlen` returns the number of bytes in the NUL-terminated string `s`,
 * excluding the terminating NUL.  Callers in this file use the result only as a
 * byte count handed to `write`, which does not dereference its buffer, so the
 * model neither reads `s` nor relates the result to its contents; it simply
 * returns a nondeterministic length `result`. */
size_t strlen(const char *s)
{
    (void)s;

    size_t result;
    return result;
}
