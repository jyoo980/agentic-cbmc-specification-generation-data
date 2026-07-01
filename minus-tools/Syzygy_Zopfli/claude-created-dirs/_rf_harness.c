#include "zopfli.h"
#include <stdlib.h>

void RandomizeFreqs(RanState *state, size_t *freqs, int n);

void __rf_harness(void)
{
    int n;
    RanState *state = (RanState *)malloc(sizeof(RanState));
    size_t *freqs = (size_t *)malloc((size_t)n * sizeof(size_t));
    RandomizeFreqs(state, freqs, n);
}
