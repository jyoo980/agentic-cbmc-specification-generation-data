#include "zopfli.h"
#include <stdlib.h>

/* dfcc harness for FollowPath: the function's own requires clause (assumed under
   --enforce-contract) constrains/allocates the inputs it actually reads, so the
   arguments here only need to be symbolic handles. */
void __fp_harness(void)
{
    ZopfliBlockState *s = (ZopfliBlockState *)malloc(sizeof(ZopfliBlockState));
    ZopfliHash *h = (ZopfliHash *)malloc(sizeof(ZopfliHash));
    ZopfliLZ77Store *store = (ZopfliLZ77Store *)malloc(sizeof(ZopfliLZ77Store));
    const unsigned char *in;
    unsigned short *path;
    size_t instart, inend, pathsize;
    FollowPath(s, in, instart, inend, path, pathsize, store, h);
}
