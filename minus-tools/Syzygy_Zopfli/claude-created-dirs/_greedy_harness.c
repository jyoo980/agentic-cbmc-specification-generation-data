#include "zopfli.h"
#include <stdlib.h>

void __greedy_harness(void)
{
    ZopfliBlockState *s;
    const unsigned char *in;
    size_t instart, inend;
    ZopfliLZ77Store *store;
    ZopfliHash *h;
    ZopfliLZ77Greedy(s, in, instart, inend, store, h);
}
