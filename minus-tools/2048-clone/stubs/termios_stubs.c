// Non-deterministic CBMC stubs for the POSIX terminal-attribute functions used
// by setBufferedInput in 2048.c. CBMC's bundled library does not model these,
// so without a stub every call raises a "no body for function" failure.
//
// Besides returning a non-deterministic status (their only caller-visible
// effect, since the real functions touch the terminal, not program memory),
// each stub bumps a spec-only call counter declared in 2048.c. That lets
// setBufferedInput's contract observe *which* terminal calls a given input
// triggers, which is the only way the &&/|| guard mutants are observable
// through the function's otherwise side-effect-free (void, no-pointer) interface.

#include <termios.h>

// Spec-only counters, defined in 2048.c. Bumped here so the contract can pin the
// exact number of tcgetattr/tcsetattr calls each setBufferedInput input makes.
extern unsigned SPEC_tcget_calls;
extern unsigned SPEC_tcset_calls;

/* FUNCTION: tcgetattr */
int tcgetattr(int fd, struct termios *termios_p)
{
	(void)fd;
	(void)termios_p;
	SPEC_tcget_calls++;
	int result;
	return result;
}

/* FUNCTION: tcsetattr */
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
	(void)fd;
	(void)optional_actions;
	(void)termios_p;
	SPEC_tcset_calls++;
	int result;
	return result;
}
