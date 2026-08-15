/* labels.c -- the problem this library came from: place labels in a rectangle without overlap.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * Each label is a fixed-size rectangle with a free centre. The objective is the total overlap
 * area between labels, which is exact, cheap and deterministic: no gradient, no noise, and one
 * evaluation is microseconds. That combination is what these searches are for.
 *
 * Staying inside the container is a HARD constraint, enforced in the repair callback by clamping
 * the centre. It is not a penalty term, so it cannot trade itself off against overlap and no
 * infeasible layout can be returned as the best.
 *
 *     example/labels [labels] [evaluations] [seeds]
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../c/cjitter.h"

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
    long n = argc > 1 ? atol(argv[1]) : 90;
    long evals = argc > 2 ? atol(argv[2]) : 20000;
    long seeds = argc > 3 ? atol(argv[3]) : 7;
    long i;

    /* atol reads junk as 0, so each bound below is also the refusal of a non-numeric
     * argument: without it, "junk" evaluations meant a header, no table and exit 0. */
    if (n < 2) { fprintf(stderr, "labels: need at least two labels\n"); return 2; }
    if (evals < 1) { fprintf(stderr, "labels: need at least one evaluation\n"); return 2; }
    if (seeds < 1) { fprintf(stderr, "labels: need at least one seed\n"); return 2; }
    L.n = n; L.w = 8; L.h = 3; L.cw = 100; L.ch = 60;
    lo = malloc((size_t)(2*n) * sizeof *lo);
    hi = malloc((size_t)(2*n) * sizeof *hi);
    if (!lo || !hi) { free(lo); free(hi); return 1; }
    for (i = 0; i < n; i++) {
        lo[2*i] = 0; hi[2*i] = L.cw;
        lo[2*i+1] = 0; hi[2*i+1] = L.ch;
    }
    p.n = 2 * n; p.lo = lo; p.hi = hi;
    p.fitness = overlap; p.repair = inside; p.ctx = &L;
    b.evals = evals; b.seed = 1; b.jitter = 0.15; b.pop = 40;

    printf("%ld labels of %gx%g in a %gx%g container: %g%% of the area is label.\n",
           n, L.w, L.h, L.cw, L.ch, 100.0 * (double)n * L.w * L.h / (L.cw * L.ch));
    printf("Objective is total overlap area; 0 is a clean layout.\n\n");
    if (cjitter_compare(&p, &b, seeds, stdout) != 0) {
        fprintf(stderr, "labels: comparison failed\n");
        free(lo); free(hi);
        return 1;
    }
    free(lo); free(hi);
    return 0;
}
