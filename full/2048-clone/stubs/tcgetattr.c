/* FUNCTION: tcgetattr */

#include <termios.h>

/* Non-deterministic stub for the POSIX `tcgetattr`.
 *
 * `tcgetattr` queries the terminal referenced by `fd` and stores its current
 * attributes in `*termios_p`.  CBMC has no model of the terminal, so the most
 * faithful sound over-approximation is to leave the output structure holding a
 * fully non-deterministic value (CBMC already treats unconstrained memory as
 * non-deterministic) and to return a non-deterministic success/failure code.
 * The function's caller-visible effect is exactly the write to `*termios_p`. */
int tcgetattr(int fd, struct termios *termios_p)
{
	(void)fd;
	struct termios nondet;
	*termios_p = nondet;
	int result;
	return result;
}
