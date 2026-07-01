#include "zopfli.h"
#include <stdlib.h>

void __zibs_harness(void)
{
    const ZopfliOptions *options;
    size_t blockstart;
    size_t blockend;
    int add_lmc;
    ZopfliBlockState *s = (ZopfliBlockState *)malloc(sizeof(ZopfliBlockState));
    ZopfliInitBlockState(options, blockstart, blockend, add_lmc, s);
}
