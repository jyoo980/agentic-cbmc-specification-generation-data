/* Standalone simulator of OptimizeHuffmanForRle to compute exact outputs for
   chosen input vectors used to build a strong CBMC contract. Not part of the
   verified code; helper only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t AbsDiff(size_t x, size_t y){ return x>y ? x-y : y-x; }

void OptimizeHuffmanForRle(int length, size_t *counts)
{
    int i, k, stride;
    size_t symbol, sum, limit;
    int *good_for_rle;
    for (; length >= 0; --length){
        if (length == 0) return;
        if (counts[length - 1] != 0) break;
    }
    good_for_rle = (int *)malloc((unsigned)length * sizeof(int));
    for (i = 0; i < length; ++i) good_for_rle[i] = 0;
    symbol = counts[0];
    stride = 0;
    for (i = 0; i < length + 1; ++i){
        if (i == length || counts[i] != symbol){
            if ((symbol == 0 && stride >= 5) || (symbol != 0 && stride >= 7)){
                for (k = 0; k < stride; ++k) good_for_rle[i - k - 1] = 1;
            }
            stride = 1;
            if (i != length) symbol = counts[i];
        } else { ++stride; }
    }
    stride = 0;
    limit = counts[0];
    sum = 0;
    for (i = 0; i < length + 1; ++i){
        if (i == length || good_for_rle[i] || AbsDiff(counts[i], limit) >= 4){
            if (stride >= 4 || (stride >= 3 && sum == 0)){
                int count = (sum + stride / 2) / stride;
                if (count < 1) count = 1;
                if (sum == 0) count = 0;
                for (k = 0; k < stride; ++k) counts[i - k - 1] = count;
            }
            stride = 0; sum = 0;
            if (i < length - 3)
                limit = (counts[i] + counts[i + 1] + counts[i + 2] + counts[i + 3] + 2) / 4;
            else if (i < length) limit = counts[i];
            else limit = 0;
        }
        ++stride;
        if (i != length) sum += counts[i];
    }
    free(good_for_rle);
}

static void run(int n, size_t *in){
    size_t c[64];
    memcpy(c, in, n*sizeof(size_t));
    OptimizeHuffmanForRle(n, c);
    printf("len=%d in =[", n);
    for(int i=0;i<n;i++) printf("%zu%s", in[i], i+1<n?",":"");
    printf("]  out=[");
    for(int i=0;i<n;i++) printf("%zu%s", c[i], i+1<n?",":"");
    printf("]\n");
}

int main(void){
    /* L=4 cases exercising loop 3 collapse paths */
    size_t a1[]={5,5,5,5};      run(4,a1);
    size_t a2[]={1,2,3,4};      run(4,a2);   /* spreads, AbsDiff triggers */
    size_t a3[]={10,10,10,1};   run(4,a3);
    size_t a4[]={0,0,0,7};      run(4,a4);
    size_t a5[]={9,9,9,9};      run(4,a5);
    size_t a6[]={100,1,1,1};    run(4,a6);
    size_t a7[]={2,2,2,9};      run(4,a7);
    size_t a8[]={6,6,6,6};      run(4,a8);
    /* larger L to exercise loop2 good_for_rle (zero run >=5, nonzero run >=7) */
    size_t b1[]={1,0,0,0,0,0,3};      run(7,b1);  /* 5 zeros middle */
    size_t b2[]={4,4,4,4,4,4,4,1};    run(8,b2);  /* 7 nonzero run */
    size_t b3[]={2,2,2,2,2,2,2,2};    run(8,b3);
    return 0;
}
