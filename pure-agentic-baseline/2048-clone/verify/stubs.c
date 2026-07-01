// Nondeterministic stubs for C library functions that CBMC does not model and
// that have no body in the program under test.  Used only for verification.
#include <stdint.h>
#include <time.h>

// printf's textual output is irrelevant to the properties being verified, and
// modelling its format-string parsing is very expensive.  Replace it with a
// non-deterministic no-op so that callers (e.g. drawBoard) verify quickly.
int printf(const char *fmt, ...)
{
	(void)fmt;
	int r;
	return r;
}

// srand only seeds the PRNG; for verification it has no observable effect.
void srand(unsigned int seed)
{
	(void)seed;
}

// rand returns a non-negative int in [0, RAND_MAX], as mandated by the C
// standard.  Modelled as an arbitrary non-negative value.
int rand(void)
{
	int r;
	__CPROVER_assume(r >= 0);
	return r;
}

// time returns an arbitrary calendar time.
time_t time(time_t *t)
{
	time_t r;
	if (t != (time_t *)0)
	{
		*t = r;
	}
	return r;
}
