/* rng.c -- xorshift64* PRNG. See rng.h. Pure, no allocation. */
#include "rng.h"

void cjitter_rng_seed(Rng *r, uint32_t seed)
{
    /* xorshift cannot escape the all-zero state, so map 0 to a fixed nonzero
     * constant: a 0 seed stays reproducible instead of producing all zeros.
     * The golden-ratio constant spreads small consecutive seeds across the
     * state space before the first draw. */
    uint64_t s = seed ? (uint64_t)seed : UINT64_C(0x9e3779b97f4a7c15);
    r->s = s * UINT64_C(0x9e3779b97f4a7c15) + 1u;
    if (!r->s) r->s = UINT64_C(0x9e3779b97f4a7c15);
}

uint32_t cjitter_rng_u32(Rng *r)
{
    uint64_t x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return (uint32_t)((x * UINT64_C(2685821657736338717)) >> 32);
}

double cjitter_rng_uniform(Rng *r, double lo, double hi)
{
    /* Map the 32-bit word to [0,1) with the full 2^32 divisor, then scale. */
    double u = (double)cjitter_rng_u32(r) / 4294967296.0;
    return lo + u * (hi - lo);
}
