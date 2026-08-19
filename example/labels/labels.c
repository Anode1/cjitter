/* labels.c -- the problem this library came from: place labels in a rectangle without overlap.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * Each label is a fixed-size rectangle with a free centre. The objective is the total overlap
 * area between labels, which is exact, cheap and deterministic: no gradient, no noise, and one
 * evaluation is microseconds.
 *
 * Staying inside the container is a HARD constraint, enforced in the repair callback by clamping
 * the centre; cjitter.h says why that is not a penalty term.
 *
 *     example/labels [labels] [evaluations] [seeds] [block]
 *
 * BLOCK is cjitter_tuning.block, the number of variables one proposal moves; omitted it stays
 * at the default, the whole vector, which is what the README's table reports. This objective
 * is a sum over labels that interact only where they touch, so 2, one label per proposal, is
 * the interesting setting and the one the 2001 system used.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../../c/cjitter.h"

/* The example's constants: the geometry, the shipped budget, and the defaults the three
 * arguments fall back to. The README's labels table is a function of these. */
#define LABEL_W     8.0
#define LABEL_H     3.0
#define AREA_W      100.0
#define AREA_H      60.0
#define JITTER      0.15    /* first move size, as a fraction of the container */
#define POP         40
#define DEF_LABELS  90
#define DEF_EVALS   20000
#define DEF_SEEDS   7

typedef struct { long n; double w, h, cw, ch; } Labels;

static double overlap(const double *x, void *ctx)
{
    Labels *L = ctx;
    double total = 0;
    long i, j;
    for (i = 0; i < L->n; i++)
        for (j = i + 1; j < L->n; j++) {
            double dx = fabs(x[2*i] - x[2*j]), dy = fabs(x[2*i+1] - x[2*j+1]);
            double ox = L->w - dx, oy = L->h - dy;
            if (ox > 0 && oy > 0) total += ox * oy;
        }
    return total;
}

/* The container. Clamping the centre keeps every label wholly inside it. */
static void inside(double *x, void *ctx)
{
    Labels *L = ctx;
    long i;
    for (i = 0; i < L->n; i++) {
        double *cx = &x[2*i], *cy = &x[2*i+1];
        if (*cx < L->w / 2)          *cx = L->w / 2;
        if (*cx > L->cw - L->w / 2)  *cx = L->cw - L->w / 2;
        if (*cy < L->h / 2)          *cy = L->h / 2;
        if (*cy > L->ch - L->h / 2)  *cy = L->ch - L->h / 2;
    }
}

int main(int argc, char **argv)
{
    Labels L;
    cjitter_problem p;
    cjitter_budget b;
    double *lo, *hi;
    cjitter_tuning t;
    long n = argc > 1 ? atol(argv[1]) : DEF_LABELS;
    long evals = argc > 2 ? atol(argv[2]) : DEF_EVALS;
    long seeds = argc > 3 ? atol(argv[3]) : DEF_SEEDS;
    long block = argc > 4 ? atol(argv[4]) : 0;   /* 0: leave the tuning default */
    long i;

    /* atol reads junk as 0, so each bound below is also the refusal of a non-numeric
     * argument: without it, "junk" evaluations meant a header, no table and exit 0. The
     * upper bound keeps 2*n inside a long before it reaches the mallocs. */
    if (n < 2) { fprintf(stderr, "labels: need at least two labels\n"); return 2; }
    if (n > 10000000) { fprintf(stderr, "labels: too many labels\n"); return 2; }
    if (evals < 1) { fprintf(stderr, "labels: need at least one evaluation\n"); return 2; }
    if (seeds < 1) { fprintf(stderr, "labels: need at least one seed\n"); return 2; }
    /* Given at all, a block must be a real one: atol reads junk as 0, and 0 here would pass
     * silently as "the default" rather than being caught the way a junk count is. */
    if (argc > 4 && block < 1) { fprintf(stderr, "labels: need at least one variable per block\n"); return 2; }
    L.n = n; L.w = LABEL_W; L.h = LABEL_H; L.cw = AREA_W; L.ch = AREA_H;
    lo = malloc((size_t)(2*n) * sizeof *lo);
    hi = malloc((size_t)(2*n) * sizeof *hi);
    if (!lo || !hi) { free(lo); free(hi); return 1; }
    for (i = 0; i < n; i++) {
        lo[2*i] = 0; hi[2*i] = L.cw;
        lo[2*i+1] = 0; hi[2*i+1] = L.ch;
    }
    p.n = 2 * n; p.lo = lo; p.hi = hi;
    p.fitness = overlap; p.repair = inside; p.ctx = &L;
    b.evals = evals; b.seed = 1; b.jitter = JITTER; b.pop = POP;

    t = cjitter_tuning_default(p.n);
    if (block > 0) t.block = block;

    printf("%ld labels of %gx%g in a %gx%g container: %g%% of the area is label.\n",
           n, L.w, L.h, L.cw, L.ch, 100.0 * (double)n * L.w * L.h / (L.cw * L.ch));
    printf("Objective is total overlap area; 0 is a clean layout.\n");
    printf("One proposal moves %ld of the %ld variables%s.\n\n", t.block, p.n,
           t.block >= p.n ? " (the whole vector)" : ", so a label at a time");
    if (cjitter_compare_tuned(&p, &b, &t, seeds, stdout) != 0) {
        fprintf(stderr, "labels: comparison failed\n");
        free(lo); free(hi);
        return 1;
    }
    free(lo); free(hi);
    return 0;
}
