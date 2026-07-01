#include "zopfli.h"
#include <stdlib.h>

void __zrh_harness(void)
{
    size_t window_size;
    ZopfliHash *h = (ZopfliHash *)malloc(sizeof(ZopfliHash));
    ZopfliResetHash(window_size, h);
}
