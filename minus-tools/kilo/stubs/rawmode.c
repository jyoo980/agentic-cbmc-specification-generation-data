/* CBMC models for enableRawMode's external callees: isatty(), tcgetattr(),
 * tcsetattr() and atexit().  Each terminal syscall returns a scripted value
 * from a global (rm_isatty / rm_tcget / rm_tcset) so enableRawMode's outcome
 * becomes a deterministic function of those globals, which the contract pins
 * exactly -- this is what kills the comparison-operator mutants on the two
 * error-check branches (tcgetattr() == -1 and tcsetattr() < 0).  atexit() is a
 * no-op.  This file is linked only when one of these symbols is an external
 * callee (i.e. enableRawMode/disableRawMode), so no other verification is
 * affected. */
#include <termios.h>

int rm_isatty; /* isatty()   return: nonzero = a tty */
int rm_tcget;  /* tcgetattr() return: -1 on error, 0 on success */
int rm_tcset;  /* tcsetattr() return: <0 on error */

/* Model errno as a single stable global so the fatal path's `errno = ENOTTY`
 * writes a named, assignable object the contract can list and constrain.
 * Without a body CBMC's __errno_location() returns a fresh nondet pointer at
 * every call, which neither the assigns clause nor an ensures could pin. */
int rm_errno;
/* FUNCTION: __errno_location */
int *__errno_location(void)
{
    return &rm_errno;
}

/* FUNCTION: isatty */
int isatty(int fd)
{
    (void)fd;
    return rm_isatty;
}

/* FUNCTION: tcgetattr */
int tcgetattr(int fd, struct termios *termios_p)
{
    (void)fd;
    (void)termios_p;
    return rm_tcget;
}

/* FUNCTION: tcsetattr */
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    return rm_tcset;
}

/* FUNCTION: atexit */
int atexit(void (*function)(void))
{
    (void)function;
    return 0;
}
