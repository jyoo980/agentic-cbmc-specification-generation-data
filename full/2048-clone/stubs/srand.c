/* FUNCTION: srand */

/* Non-deterministic stub for the C standard library `srand`.
 *
 * `srand` only seeds the pseudo-random number generator used by `rand`; it has
 * no observable effect on its caller's data. CBMC already models `rand` as a
 * nondeterministic source, so the seeding is irrelevant to verification. This
 * stub therefore has an empty body, which is a sound over-approximation: it
 * makes no assumptions and leaves all caller-visible state untouched. */
void srand(unsigned int seed)
{
	(void)seed;
}
