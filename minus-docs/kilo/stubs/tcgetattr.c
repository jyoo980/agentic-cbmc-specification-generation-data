#include <termios.h>

/* FUNCTION: tcgetattr */

/* A nondeterministic model of POSIX `tcgetattr`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models (the same motivation documented in
 * `read.c`/`write.c`).
 *
 * `tcgetattr` retrieves the terminal attributes of `fd` into `*termios_p`,
 * returning 0 on success or -1 on error.  On success the kernel fills in the
 * structure, so the model havocs `*termios_p` (it must point to a writable
 * `struct termios`, which every caller guarantees) and returns 0; on failure it
 * leaves the structure untouched and returns -1.  The choice is
 * nondeterministic. */
int tcgetattr(int fd, struct termios *termios_p)
{
    (void)fd;

    int result;
    __CPROVER_assume(result == 0 || result == -1);
    if (result == 0)
        __CPROVER_havoc_object(termios_p);
    return result;
}
