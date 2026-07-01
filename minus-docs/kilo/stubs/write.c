#include <sys/types.h>
#include <unistd.h>

/* FUNCTION: write */

/* A nondeterministic model of POSIX `write`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models.  The bundled `<builtin-library-write>` model
 * maintains a global `__CPROVER_pipes` array with internal fields (e.g. a
 * `widowed` flag) whose size CBMC cannot determine while building an assigns
 * frame, crashing `goto-instrument --enforce-contract` with "no definite size
 * for lvalue target" (the same class of failure documented in `read.c` and
 * `exit.c`).
 *
 * `write` attempts to write up to `count` bytes from `buf` to `fd`, returning
 * the number of bytes actually written, or -1 on error.  The model only reads
 * from `buf` (it does not modify it) and returns a nondeterministic `result` in
 * [-1, count].  `buf` must point to at least `count` readable bytes, which every
 * caller guarantees. */
ssize_t write(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;

    ssize_t result;
    __CPROVER_assume(result >= -1 && result <= (ssize_t)count);
    return result;
}
