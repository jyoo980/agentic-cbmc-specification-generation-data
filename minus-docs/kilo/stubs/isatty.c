#include <errno.h>
#include <unistd.h>

/* FUNCTION: isatty */

/* A nondeterministic model of POSIX `isatty`.  Providing a body keeps
 * `run-cbmc` from taking its "missing body" retry path, which injects
 * CBMC's bundled C-library models (the same motivation documented in
 * `read.c`/`write.c`).
 *
 * `isatty` tests whether `fd` refers to a terminal, returning 1 if it does and
 * 0 otherwise.  The model has no side effects and returns a nondeterministic
 * `result` constrained to {0, 1}. */
int isatty(int fd)
{
    (void)fd;

    int result;
    __CPROVER_assume(result == 0 || result == 1);
    return result;
}

/* FUNCTION: __errno_location */

/* A concrete model of glibc's `__errno_location`.  The system `<errno.h>`
 * defines `errno` as `(*__errno_location())`; without a body for
 * `__errno_location` CBMC models each call as returning a fresh nondeterministic
 * pointer, so a write through `errno` targets an unknown object that no
 * `__CPROVER_assigns` frame can name.  Modeling `__errno_location` to always
 * return the address of one concrete global makes `errno` a stable, nameable
 * lvalue, `__avocado_errno`, that a specification can list in its assigns clause
 * to permit (and constrain) writes to `errno`.
 *
 * This lives alongside `isatty` because `enableRawMode` — the function whose
 * spec needs to frame `errno` — is the lone writer of `errno` in this file, and
 * `isatty.c` is already among its compiled-in stubs; avocado resolves stub files
 * by the external callees in the call graph, and `__errno_location` never
 * appears there (it is introduced only by the `errno` macro after
 * preprocessing), so it cannot be supplied as a stub file of its own. */
int __avocado_errno;

int *__errno_location(void)
{
    return &__avocado_errno;
}
