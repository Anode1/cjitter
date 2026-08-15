/* rng.h -- a small deterministic pseudo-random generator.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * xorshift64* (Vigna 2016 after Marsaglia 2003): a 64-bit xorshift state with a
 * multiplicative scramble, returning the high 32 bits. Integer arithmetic only,
 * so the stream is identical on every machine. The period is 2^64-1, which is
 * what lets compare's seed panel space its streams without collision: the
 * predecessor's 32-bit generator had seed streams overlapping after 1.6e8
 * draws, a budget a plain command-line argument can reach. NOT cryptographic.
 * Pure: no allocation, caller owns the state.
 */
#ifndef CJITTER_RNG_H
#define CJITTER_RNG_H

#include <stdint.h>


/* The whole generator state: one 64-bit word. Copyable by value. */
typedef struct {
    uint64_t s;
} Rng;

/* Seed the generator. Any seed is accepted; 0 is remapped (xorshift cannot
 * leave the zero state), so a 0 seed is reproducible, not degenerate. */
void rng_seed(Rng *r, uint32_t seed);

/* Next 32-bit value, advancing the state. */
uint32_t rng_u32(Rng *r);

/* Uniform real in [lo, hi). */
double rng_uniform(Rng *r, double lo, double hi);

#endif /* CJITTER_RNG_H */
