#include "zopfli.h"
#include <stdlib.h>

void __zclz_harness(void)
{
    ZopfliLZ77Store *source = (ZopfliLZ77Store *)malloc(sizeof(ZopfliLZ77Store));
    ZopfliLZ77Store *dest = (ZopfliLZ77Store *)malloc(sizeof(ZopfliLZ77Store));
    ZopfliCopyLZ77Store(source, dest);
}
