// Nondeterministic stubs for the terminal-attribute calls used by
// setBufferedInput().  They model the I/O effects as nondeterministic and
// otherwise have no observable behaviour relevant to verification.

#include <stddef.h>
#include <termios.h>

/* FUNCTION: tcgetattr */
int tcgetattr(int fd, struct termios *termios_p)
{
	(void)fd;
	if (termios_p != NULL)
	{
		struct termios nondet;
		*termios_p = nondet;
	}
	int result;
	return result;
}

/* FUNCTION: tcsetattr */
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
	(void)fd;
	(void)optional_actions;
	(void)termios_p;
	int result;
	return result;
}
