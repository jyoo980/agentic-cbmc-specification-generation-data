// Non-deterministic stubs for library functions without CBMC bodies,
// used only for contract verification of 2048.c.

// srand only seeds the PRNG; it has no observable effect for verification.
void srand(unsigned int seed)
{
	(void)seed;
}
