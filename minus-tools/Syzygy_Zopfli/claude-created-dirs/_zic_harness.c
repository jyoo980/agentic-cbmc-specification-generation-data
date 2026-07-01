#include "zopfli.h"
#include <stdlib.h>

void __zic_harness(void)
{
    size_t blocksize;
    ZopfliLongestMatchCache *lmc =
        (ZopfliLongestMatchCache *)malloc(sizeof(ZopfliLongestMatchCache));
    ZopfliInitCache(blocksize, lmc);
}
