#include <termios.h>

/* FUNCTION: tcsetattr */

/* A nondeterministic model of POSIX `tcsetattr`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models (the same motivation documented in
 * `read.c`/`write.c`).
 *
 * `tcsetattr` sets the terminal attributes of `fd` from `*termios_p` according
 * to `optional_actions`, returning 0 on success or -1 on error.  The model only
 * reads from `*termios_p` (it does not modify it) and returns a
 * nondeterministic `result` in {0, -1}.  `termios_p` must point to a readable
 * `struct termios`, which every caller guarantees. */
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    __CPROVER_assume(termios_p != (const struct termios *)0);

    int result;
    __CPROVER_assume(result == 0 || result == -1);
    return result;
}
