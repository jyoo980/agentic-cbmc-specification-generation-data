#include "zopfli.h"
#include <stdlib.h>

void __zvld_harness(void)
{
    const unsigned char *data;
    size_t datasize;
    size_t pos;
    unsigned short dist;
    unsigned short length;
    ZopfliVerifyLenDist(data, datasize, pos, dist, length);
}
