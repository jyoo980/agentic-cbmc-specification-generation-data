/* FUNCTION: tcsetattr */

#include <termios.h>

/* Non-deterministic stub for the POSIX `tcsetattr`.
 *
 * `tcsetattr` pushes the attributes in `*termios_p` onto the terminal referenced
 * by `fd`.  Its only effect is on the (unmodelled) terminal device; it does not
 * write through any caller-visible pointer.  The sound over-approximation is
 * therefore a body that touches no caller memory and returns a
 * non-deterministic success/failure code. */
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
	(void)fd;
	(void)optional_actions;
	(void)termios_p;
	int result;
	return result;
}
