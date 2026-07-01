#include <sys/types.h>
#include <unistd.h>

/* FUNCTION: read */

/* A nondeterministic model of POSIX `read`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models.  The bundled `<builtin-library-read>` model
 * declares an internal `error` symbol whose size CBMC cannot determine while
 * building a loop's assigns frame, crashing
 * `goto-instrument --enforce-contract` with "no definite size for lvalue
 * target ... read::1::1::error" (the same class of failure documented in
 * `exit.c`).
 *
 * `read` attempts to read up to `count` bytes from `fd` into `buf`, returning
 * the number of bytes actually read (0 at EOF), or -1 on error.  The model
 * returns a nondeterministic `result` in [-1, count]; on a non-negative result
 * it havocs the first `result` bytes of `buf` (the bytes the kernel would have
 * written), leaving the rest of the buffer untouched.  `buf` must point to at
 * least `count` writable bytes, which every caller guarantees. */
ssize_t read(int fd, void *buf, size_t count)
{
    (void)fd;

    ssize_t result;
    __CPROVER_assume(result >= -1 && result <= (ssize_t)count);
    if (result > 0)
        __CPROVER_havoc_slice(buf, (size_t)result);
    return result;
}
