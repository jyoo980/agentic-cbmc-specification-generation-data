#include "zopfli.h"
#include <stdlib.h>

void __zalz_harness(void)
{
    const ZopfliLZ77Store *store;
    ZopfliLZ77Store *target;
    ZopfliAppendLZ77Store(store, target);
}
