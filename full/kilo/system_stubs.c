/* Nondeterministic stubs for external library functions that CBMC does not
 * model itself. Each modeled symbol is tagged with a `/​* FUNCTION: name *​/`
 * marker so the avocado harness can pull in this file when needed.
 *
 * Reading an uninitialized local yields a nondeterministic value in CBMC,
 * which is the intended "could return anything" abstraction for these calls. */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>

/* FUNCTION: strstr */
char *strstr(const char *haystack, const char *needle) {
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    int found;
    if (found && nlen <= hlen) {
        size_t off;
        __CPROVER_assume(off + nlen <= hlen);
        return (char *)haystack + off;
    }
    return (char *)0;
}

/* FUNCTION: tcsetattr */
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    int result;
    return result;
}

/* FUNCTION: tcgetattr */
int tcgetattr(int fd, struct termios *termios_p) {
    int result;
    return result;
}

/* FUNCTION: isatty */
int isatty(int fd) {
    int result;
    return result;
}

/* FUNCTION: atexit */
int atexit(void (*function)(void)) {
    int result;
    return result;
}

/* FUNCTION: ioctl */
int ioctl(int fd, unsigned long request, ...) {
    int result;
    return result;
}

/* FUNCTION: signal */
void (*signal(int signum, void (*handler)(int)))(int) {
    return handler;
}

/* FUNCTION: perror */
void perror(const char *s) {
}

/* FUNCTION: time */
time_t time(time_t *tloc) {
    time_t result;
    if (tloc) *tloc = result;
    return result;
}
