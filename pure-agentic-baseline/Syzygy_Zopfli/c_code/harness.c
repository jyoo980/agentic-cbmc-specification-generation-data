/* CBMC verification harnesses. Includes the source so that static functions
   are visible. The DFCC entry point must be a real `main`, so we build main()
   to invoke a single harness selected at compile time via -DHARNESS=...

   The original main() is renamed so that the stdin-reading driver code (which
   uses getchar, unsupported by DFCC) becomes unused. */
#define main zopfli_unused_main
#include "zopfli.c"
#undef main

/* ---- Per-function proof harnesses ----
   Each calls its target exactly once. Pointer inputs are expressed through the
   target's own __CPROVER_requires (e.g. __CPROVER_is_fresh), so most harnesses
   only need to declare nondet locals. */

void harness_AbsDiff(void)
{
    size_t x, y;
    AbsDiff(x, y);
}

void harness_ZopfliGetDistSymbol(void)
{
    int dist;
    ZopfliGetDistSymbol(dist);
}

void harness_ZopfliGetLengthSymbol(void)
{
    int l;
    ZopfliGetLengthSymbol(l);
}

void harness_ZopfliGetDistSymbolExtraBits(void)
{
    int s;
    ZopfliGetDistSymbolExtraBits(s);
}

void harness_ZopfliGetLengthSymbolExtraBits(void)
{
    int s;
    ZopfliGetLengthSymbolExtraBits(s);
}

void harness_ZopfliGetLengthExtraBits(void)
{
    int l;
    ZopfliGetLengthExtraBits(l);
}

void harness_ZopfliGetLengthExtraBitsValue(void)
{
    int l;
    ZopfliGetLengthExtraBitsValue(l);
}

void harness_ZopfliGetDistExtraBits(void)
{
    int dist;
    ZopfliGetDistExtraBits(dist);
}

void harness_ZopfliGetDistExtraBitsValue(void)
{
    int dist;
    ZopfliGetDistExtraBitsValue(dist);
}

void harness_GetLengthScore(void)
{
    int length, distance;
    GetLengthScore(length, distance);
}

void harness_zopfli_min(void)
{
    size_t a, b;
    zopfli_min(a, b);
}

void harness_CeilDiv(void)
{
    size_t a, b;
    CeilDiv(a, b);
}

void harness_GetCostFixed(void)
{
    unsigned litlen, dist;
    GetCostFixed(litlen, dist, 0);
}

void harness_PatchDistanceCodesForBuggyDecoders(void)
{
    unsigned *d_lengths;
    PatchDistanceCodesForBuggyDecoders(d_lengths);
}

void harness_InitNode(void)
{
    size_t weight;
    int count;
    Node *tail;
    Node *node;
    InitNode(weight, count, tail, node);
}

void harness_Ran(void)
{
    RanState *state;
    Ran(state);
}

void harness_InitRanState(void)
{
    RanState *state;
    InitRanState(state);
}

void harness_UpdateHashValue(void)
{
    ZopfliHash *h;
    unsigned char c;
    UpdateHashValue(h, c);
}

void harness_InitStats(void)
{
    SymbolStats *stats;
    InitStats(stats);
}

void harness_ClearStatFreqs(void)
{
    SymbolStats *stats;
    ClearStatFreqs(stats);
}

void harness_ZopfliLZ77GetByteRange(void)
{
    ZopfliLZ77Store *lz77;
    size_t lstart, lend;
    ZopfliLZ77GetByteRange(lz77, lstart, lend);
}

void harness_ZopfliMaxCachedSublen(void)
{
    ZopfliLongestMatchCache *lmc;
    size_t pos, length;
    ZopfliMaxCachedSublen(lmc, pos, length);
}

/* ---- Entry point selection ---- */
#ifndef HARNESS
#define HARNESS harness_AbsDiff
#endif
extern void HARNESS(void);
int main(void)
{
    HARNESS();
    return 0;
}
