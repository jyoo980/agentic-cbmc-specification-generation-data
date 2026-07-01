#include <sys/ioctl.h>

/* FUNCTION: ioctl */

/* A nondeterministic model of POSIX `ioctl`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models; that path additionally pulls in the
 * variadic-aware library handling that crashes
 * `goto-instrument --enforce-contract` (the same motivation documented in
 * `read.c`/`write.c`).
 *
 * `ioctl` performs a device-dependent control operation on the file descriptor
 * `fd`.  Callers in this file use it only to query the terminal window size
 * (`TIOCGWINSZ`) and inspect the `winsize` structure the kernel would fill in.
 * The model leaves that structure untouched -- the caller declares it as an
 * uninitialized local, so CBMC already treats its fields as nondeterministic --
 * and simply returns a nondeterministic `result` in [-1, 0] so that both the
 * success (0) and failure (-1) branches of every caller are explored. */
int ioctl(int fd, unsigned long request, ...)
{
    (void)fd;
    (void)request;

    int result;
    __CPROVER_assume(result == 0 || result == -1);
    return result;
}
