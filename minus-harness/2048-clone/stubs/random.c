// Nondeterministic stubs for the C standard-library PRNG and clock, used by
// addRandom().  rand() is constrained to be non-negative so that `rand() % len`
// stays within [0, len), matching the behaviour the caller relies on.

#include <stdlib.h>
#include <time.h>

/* FUNCTION: rand */
int rand(void)
{
	int result;
	__CPROVER_assume(result >= 0);
	return result;
}

/* FUNCTION: srand */
void srand(unsigned int seed)
{
	(void)seed;
}

/* FUNCTION: time */
time_t time(time_t *t)
{
	time_t result;
	if (t != NULL)
	{
		*t = result;
	}
	return result;
}
