/* CBMC stub file for kilo.c verification.
 *
 * Supplies bodies for the external functions that the verified contracts reach
 * but whose implementations CBMC cannot see.  Each is the "nondeterministic
 * specification in a stub file" sanctioned for missing callee bodies.
 */

/* glibc internal ctype table accessor, referenced by the isspace()/isdigit()/
 * isprint() macros expanded from <ctype.h>.  Without a body the ctype checks
 * become unreachable assertions.  The table contents are left
 * nondeterministic (a sound over-approximation of any classification result),
 * and the pointer is (re)assigned in the body on every call so that the
 * nondeterministic-static-initialization done by contract enforcement cannot
 * make it dangle.  glibc's table is addressable on the index range [-128, 255],
 * which is exactly the precondition the ctype-using contracts establish. */
const unsigned short int **__ctype_b_loc(void) {
    static unsigned short int table[384];
    static const unsigned short int *ptr;
    ptr = &table[128];
    return (const unsigned short int **)&ptr;
}

/* Terminal control call used by disableRawMode().  Returns a nondeterministic
 * status and writes through none of its arguments, modelling a syscall that
 * leaves caller memory untouched.  termios.h supplies struct termios. */
#include <termios.h>

int tcsetattr(int fd, int actions, const struct termios *tp) {
    (void)fd; (void)actions; (void)tp;
    int r; return r;
}
