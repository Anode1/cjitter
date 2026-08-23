/* sixty.c -- the sixty-draws question, piloted.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * The rule of 59: n uniform draws land in the top 5% of the sampling distribution
 * with probability 1 - 0.95^n, which crosses 95% at n = 59. The arithmetic is not in
 * question. The claim the rule makes quietly is that the top 5% of the SAMPLING
 * distribution is a good outcome, and that is a claim about landscapes. This pilot
 * measures both halves on two landscapes: what best-of-n random actually delivers
 * (median over a seed panel, and the fraction of panels inside the top 5% against
 * the arithmetic's prediction), and the budget at which each search method matches
 * random's n = 59 outcome, which is the method-wise counterpart of the constant.
 *
 * Exploratory. The output is not pinned by any test and no verdict here is a
 * finding; confirmatory runs wait for a signed pre-registration. Phase two adds
 * the smbpann engine's training constants as a third landscape.
 */
#include <stdio.h>
#include <stdlib.h>

#include "../../c/cjitter.h"

#define PANELS   201        /* seeds per estimate; odd, so the median is one value */
#define REFDRAWS 50000      /* single draws estimating the sampling distribution  */
#define PANELSEED  1000u    /* panel i runs on PANELSEED + i                      */
#define REFSEED 2000000u    /* reference draw i runs on REFSEED + i               */

static const long grid[] = { 1, 2, 4, 8, 15, 30, 59, 120, 240, 480, 960 };
#define NGRID ((long)(sizeof grid / sizeof grid[0]))
#define NFIFTYNINE 6        /* grid[6] == 59, the folklore budget */

/* Landscape 1: the sphere, the textbook bowl. Optimum 0 at the origin. */
#define SPH_N 20

static double sphere(const double *x, void *ctx)
{
    double s = 0;
    long j;
    (void)ctx;
    for (j = 0; j < SPH_N; j++)
        s += x[j] * x[j];
    return s;
}

/* Landscape 2: rectangle placement, the labels example's objective in miniature.
 * K rectangles of fixed mixed sizes, variables are the centers, fitness is the
 * summed pairwise overlap area. The box holds each center far enough from the
 * container wall that the rectangle stays inside, so the hard constraint needs
 * no repair. About 44% of the container is covered; zero overlap exists but
 * uniform sampling almost never finds it. */
#define LAB_K 30
#define LAB_N (2 * LAB_K)
#define LAB_SIDE 100.0

static double lab_w(long i) { return 8.0 + 3.0 * (double)(i % 5); }
static double lab_h(long i) { return 6.0 + 3.0 * (double)(i % 4); }

static double overlap(const double *x, void *ctx)
{
    double s = 0;
    long i, j;
    (void)ctx;
    for (i = 0; i < LAB_K; i++) {
        for (j = i + 1; j < LAB_K; j++) {
            double dx = x[2 * i] - x[2 * j];
            double dy = x[2 * i + 1] - x[2 * j + 1];
            double ox, oy;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            ox = 0.5 * (lab_w(i) + lab_w(j)) - dx;
            oy = 0.5 * (lab_h(i) + lab_h(j)) - dy;
            if (ox > 0 && oy > 0)
                s += ox * oy;
        }
    }
    return s;
}

static int cmp_double(const void *a, const void *b)
{
    double d = *(const double *)a - *(const double *)b;
    return (d > 0) - (d < 0);
}

/* Best fitness from one run: METHOD at budget EVALS on SEED. -1 on failure. */
static int one_run(const char *method, const cjitter_problem *p, long evals,
                   uint32_t seed, double *xbuf, double *best)
{
    cjitter_budget b = { 0, 0 };
    cjitter_result r;
    b.evals = evals;
    b.seed = seed;
    r.x = xbuf;
    if (cjitter_run(method, p, &b, &r))
        return -1;
    *best = r.best;
    return 0;
}

/* Median over the panel of best-of-EVALS for METHOD. -1 on failure. */
static int panel_median(const char *method, const cjitter_problem *p, long evals,
                        double *xbuf, double *vals, double *median, const double *q05,
                        double *hitshare)
{
    long i, hits = 0;
    for (i = 0; i < PANELS; i++) {
        if (one_run(method, p, evals, PANELSEED + (uint32_t)i, xbuf, &vals[i]))
            return -1;
        if (vals[i] <= *q05)
            hits++;
    }
    qsort(vals, PANELS, sizeof vals[0], cmp_double);
    *median = vals[PANELS / 2];
    *hitshare = (double)hits / PANELS;
    return 0;
}

/* One landscape through the whole pilot. Returns 0, or -1 on a failed run. */
static int landscape(const char *name, const cjitter_problem *p)
{
    static const char *const searches[] = { "climb", "anneal", "ga" };
    double *ref = NULL, xbuf[LAB_N], vals[PANELS];
    double q05, target, median, hitshare, pred;
    long g, i, m;
    int rc = -1;

    ref = malloc(REFDRAWS * sizeof *ref);
    if (!ref)
        goto out;
    for (i = 0; i < REFDRAWS; i++)
        if (one_run("random", p, 1, REFSEED + (uint32_t)i, xbuf, &ref[i]))
            goto out;
    qsort(ref, REFDRAWS, sizeof ref[0], cmp_double);
    q05 = ref[REFDRAWS / 20 - 1];

    printf("%s, %ld variables\n", name, p->n);
    printf("  reference: %d single draws; top-5%% threshold %g; best single draw %g\n",
           REFDRAWS, q05, ref[0]);
    printf("  %6s  %14s  %10s  %10s\n", "n", "random median", "in top 5%", "1-0.95^n");
    target = 0;
    for (g = 0; g < NGRID; g++) {
        if (panel_median("random", p, grid[g], xbuf, vals, &median, &q05, &hitshare))
            goto out;
        pred = 1.0;
        for (i = 0; i < grid[g]; i++)
            pred *= 0.95;
        printf("  %6ld  %14g  %10.3f  %10.3f\n", grid[g], median, hitshare, 1.0 - pred);
        if (g == NFIFTYNINE)
            target = median;
    }
    printf("  random's n = 59 median: %g\n", target);
    for (m = 0; m < 3; m++) {
        double at59 = 0;
        long match = 0;
        for (g = 0; g < NGRID; g++) {
            if (panel_median(searches[m], p, grid[g], xbuf, vals, &median, &q05,
                             &hitshare))
                goto out;
            if (g == NFIFTYNINE)
                at59 = median;
            if (!match && median <= target)
                match = grid[g];
        }
        if (match)
            printf("  %-6s  matches it by n = %-4ld (median at 59: %g)\n",
                   searches[m], match, at59);
        else
            printf("  %-6s  never matches it in this grid (median at 59: %g)\n",
                   searches[m], at59);
    }
    printf("\n");
    rc = 0;
out:
    free(ref);
    return rc;
}

int main(void)
{
    static double lo[LAB_N], hi[LAB_N];
    cjitter_problem p;
    long j;

    printf("The sixty-draws pilot. Exploratory: nothing here is pinned or a finding.\n\n");

    for (j = 0; j < SPH_N; j++) {
        lo[j] = -5;
        hi[j] = 5;
    }
    p.n = SPH_N;
    p.lo = lo;
    p.hi = hi;
    p.fitness = sphere;
    p.repair = NULL;
    p.ctx = NULL;
    p.start = NULL;
    if (landscape("sphere", &p))
        return 1;

    for (j = 0; j < LAB_K; j++) {
        lo[2 * j] = 0.5 * lab_w(j);
        hi[2 * j] = LAB_SIDE - 0.5 * lab_w(j);
        lo[2 * j + 1] = 0.5 * lab_h(j);
        hi[2 * j + 1] = LAB_SIDE - 0.5 * lab_h(j);
    }
    p.n = LAB_N;
    p.fitness = overlap;
    if (landscape("rectangle overlap", &p))
        return 1;

    return 0;
}
