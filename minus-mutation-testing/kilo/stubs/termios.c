/* CBMC stub models for the low-level terminal/process functions used by
 * kilo's enableRawMode/disableRawMode.
 *
 * goto-cc does not link bundled CPROVER models for isatty, atexit, tcgetattr,
 * or tcsetattr, so under an assigns contract their call sites fail with
 * "no body for callee". This file supplies sound non-deterministic models.
 *
 * It also models __errno_location. In glibc, `errno` expands to
 * `*__errno_location()`; without a body for __errno_location, a write to
 * `errno` targets an unmodeled nondet pointer that CBMC cannot relate to any
 * assigns target, so the assigns check fails. Pinning __errno_location to a
 * single global object (`__avocado_errno`) lets callers name that object in
 * their assigns clause and discharge the errno write.
 */

#include <termios.h>

/* FUNCTION: __errno_location */

/* Backing storage for errno; callers writing errno must list this in assigns. */
int __avocado_errno;

int *__errno_location(void) {
    return &__avocado_errno;
}

/* FUNCTION: isatty */

int isatty(int fd) {
    (void)fd;
    int result;
    /* isatty returns 1 if fd is a terminal, 0 otherwise. */
    __CPROVER_assume(result == 0 || result == 1);
    return result;
}

/* FUNCTION: atexit */

int atexit(void (*function)(void)) {
    (void)function;
    /* atexit returns 0 on success, nonzero on failure. */
    int result;
    return result;
}

/* FUNCTION: tcgetattr */

int tcgetattr(int fd, struct termios *termios_p) {
    (void)fd;

    /* The caller must provide a writable struct termios. */
    __CPROVER_precondition(__CPROVER_w_ok(termios_p, sizeof(*termios_p)),
                           "tcgetattr: termios_p must be writable");

    int result;
    __CPROVER_assume(result == 0 || result == -1);
    if (result == 0) {
        /* On success the structure is filled with the current settings;
           modelling that as a full havoc is a sound over-approximation. */
        struct termios nondet_termios;
        *termios_p = nondet_termios;
    }
    return result;
}

/* FUNCTION: tcsetattr */

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)fd;
    (void)optional_actions;

    /* The caller must provide a readable struct termios. */
    __CPROVER_precondition(__CPROVER_r_ok(termios_p, sizeof(*termios_p)),
                           "tcsetattr: termios_p must be readable");

    int result;
    __CPROVER_assume(result == 0 || result == -1);
    return result;
}
