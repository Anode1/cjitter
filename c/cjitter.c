/* cjitter.c -- the four searches, and the comparison between them.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cjitter.h"
#include "rng.h"

const char *const cjitter_methods[] = { "random", "climb", "anneal", "ga", NULL };

/* Scratch for one run. Allocated once here, so no search step allocates. */
typedef struct {
    const cjitter_problem *p;
    double  *x, *cand, *best;
    double  *pop, *fit;          /* ga */
    long     npop;
    Rng      rng;
    long     spent, budget;
    double   bestf;
} Run;

static double uni(Rng *r) { return (double)rng_u32(r) / 4294967296.0; }

/* Box-Muller, one value; the second is discarded. A normal step is what makes the move size
 * mean something across dimensions of different width. */
static double gauss(Rng *r)
{
    double u = uni(r), v = uni(r);
    if (u < 1e-12) u = 1e-12;
    return sqrt(-2.0 * log(u)) * cos(6.283185307179586 * v);
}

static void clamp(const cjitter_problem *p, double *x)
{
    long j;
    for (j = 0; j < p->n; j++) {
        if (x[j] < p->lo[j]) x[j] = p->lo[j];
        if (x[j] > p->hi[j]) x[j] = p->hi[j];
    }
    if (p->repair) p->repair(x, p->ctx);
}

static void draw(Run *R, double *x)
{
    long j;
    for (j = 0; j < R->p->n; j++)
        x[j] = R->p->lo[j] + uni(&R->rng) * (R->p->hi[j] - R->p->lo[j]);
    clamp(R->p, x);
}

/* Every evaluation goes through here, so the budget is exact and no method can quietly spend
 * more than another. That is the only thing that makes the comparison fair. The methods are
 * responsible for not calling past the budget; the assert is what makes a missed guard an
 * immediate failure instead of a silently unfair comparison, which is how one overspend
 * (climb's restart on the last evaluation) shipped before it existed. */
static double score(Run *R, const double *x)
{
    assert(R->spent < R->budget);
    R->spent++;
    return R->p->fitness(x, R->p->ctx);
}

static void keep(Run *R, const double *x, double f)
{
    if (f < R->bestf) { R->bestf = f; memcpy(R->best, x, (size_t)R->p->n * sizeof *x); }
}

/* A jittered neighbour: each variable moved by a normal draw scaled to its own range. */
static void jitter(Run *R, const double *from, double *to, double scale)
{
    long j;
    for (j = 0; j < R->p->n; j++)
        to[j] = from[j] + gauss(&R->rng) * scale * (R->p->hi[j] - R->p->lo[j]);
    clamp(R->p, to);
}

static void run_random(Run *R)
{
    while (R->spent < R->budget) {
        draw(R, R->x);
        keep(R, R->x, score(R, R->x));
    }
}

/* Hill climbing with restarts. Stuck is a fixed number of rejected proposals, after which the
 * move size has already shrunk as far as it usefully goes. */
static void run_climb(Run *R, double jit, long *restarts)
{
    long stuck = 0, patience = 40 + 10 * R->p->n;
    double f, scale = jit;
    draw(R, R->x);
    f = score(R, R->x);
    keep(R, R->x, f);
    while (R->spent < R->budget) {
        double g;
        jitter(R, R->x, R->cand, scale);
        g = score(R, R->cand);
        if (g < f) {
            f = g;
            memcpy(R->x, R->cand, (size_t)R->p->n * sizeof *R->x);
            keep(R, R->x, f);
            stuck = 0;
        } else if (++stuck >= patience) {
            scale *= 0.5;
            stuck = 0;
            /* As local as it is going to get: restart -- but only if there is budget left to
             * score the new start. Without the check this was the one path that could score
             * twice in an iteration and spend budget+1, which the header says cannot happen. */
            if (scale < jit / 64.0 && R->spent < R->budget) {
                draw(R, R->x);
                f = score(R, R->x);
                keep(R, R->x, f);
                scale = jit;
                (*restarts)++;
            }
        }
    }
}

/* The same walk, accepting a worse neighbour with probability exp(-d/T). The temperature falls
 * geometrically over the budget, and the move size falls with it: at low temperature a large
 * move is rejected anyway, so proposing one wastes an evaluation. */
static void run_anneal(Run *R, double jit)
{
    double f, t0 = 1.0, t;
    long k = 0;
    draw(R, R->x);
    f = score(R, R->x);
    keep(R, R->x, f);
    /* Scale the starting temperature to the problem: the mean uphill step of a few probes. */
    {
        double s = 0;
        long i, m = 20 < R->budget / 10 ? 20 : 1;
        for (i = 0; i < m && R->spent < R->budget; i++) {
            jitter(R, R->x, R->cand, jit);
            s += fabs(score(R, R->cand) - f);
        }
        if (s > 0) t0 = s / (double)m;
    }
    while (R->spent < R->budget) {
        double g, frac = (double)R->spent / (double)R->budget;
        t = t0 * pow(1e-3, frac);
        jitter(R, R->x, R->cand, jit * (1.0 - 0.9 * frac));
        g = score(R, R->cand);
        if (g < f || (t > 0 && uni(&R->rng) < exp((f - g) / t))) {
            f = g;
            memcpy(R->x, R->cand, (size_t)R->p->n * sizeof *R->x);
            keep(R, R->x, f);
        }
        k++;
    }
}

/* Tournament selection, blend crossover, jittered mutation, one elite carried. */
static void run_ga(Run *R, double jit)
{
    long i, j, np = R->npop, n = R->p->n;
    double *next = R->pop + np * n;
    for (i = 0; i < np && R->spent < R->budget; i++) {
        draw(R, R->pop + i * n);
        R->fit[i] = score(R, R->pop + i * n);
        keep(R, R->pop + i * n, R->fit[i]);
    }
    while (R->spent < R->budget) {
        long elite = 0;
        for (i = 1; i < np; i++) if (R->fit[i] < R->fit[elite]) elite = i;
        memcpy(next, R->pop + elite * n, (size_t)n * sizeof *next);
        for (i = 1; i < np; i++) {
            long a = (long)(uni(&R->rng) * (double)np), b = (long)(uni(&R->rng) * (double)np);
            long c = (long)(uni(&R->rng) * (double)np), d = (long)(uni(&R->rng) * (double)np);
            const double *pa, *pb;
            double *kid = next + i * n;
            if (a >= np) a = np - 1;
            if (b >= np) b = np - 1;
            if (c >= np) c = np - 1;
            if (d >= np) d = np - 1;
            pa = R->pop + (R->fit[a] < R->fit[b] ? a : b) * n;
            pb = R->pop + (R->fit[c] < R->fit[d] ? c : d) * n;
            for (j = 0; j < n; j++) {
                double w = uni(&R->rng);
                kid[j] = w * pa[j] + (1.0 - w) * pb[j];
            }
            jitter(R, kid, kid, jit * 0.3);
        }
        memcpy(R->pop, next, (size_t)np * (size_t)n * sizeof *next);
        for (i = 0; i < np && R->spent < R->budget; i++) {
            R->fit[i] = score(R, R->pop + i * n);
            keep(R, R->pop + i * n, R->fit[i]);
        }
    }
}

int cjitter_run(const char *method, const cjitter_problem *p, const cjitter_budget *b,
                cjitter_result *out)
{
    Run R;
    long restarts = 0;
    double jit;
    int rc = -1;

    if (!method || !p || !b || !out || !p->fitness || p->n < 1 || b->evals < 1) return -1;
    memset(&R, 0, sizeof R);
    R.p = p;
    R.budget = b->evals;
    R.bestf = HUGE_VAL;
    R.npop = b->pop > 1 ? b->pop : 30;
    if (R.npop > b->evals) R.npop = b->evals;
    jit = b->jitter > 0 ? b->jitter : 0.1;
    rng_seed(&R.rng, b->seed ? b->seed : 1u);

    R.x    = malloc((size_t)p->n * sizeof *R.x);
    R.cand = malloc((size_t)p->n * sizeof *R.cand);
    R.best = malloc((size_t)p->n * sizeof *R.best);
    R.fit  = malloc((size_t)R.npop * sizeof *R.fit);
    R.pop  = malloc((size_t)R.npop * (size_t)p->n * 2 * sizeof *R.pop);
    if (!R.x || !R.cand || !R.best || !R.fit || !R.pop) goto done;

    if      (!strcmp(method, "random")) run_random(&R);
    else if (!strcmp(method, "climb"))  run_climb(&R, jit, &restarts);
    else if (!strcmp(method, "anneal")) run_anneal(&R, jit);
    else if (!strcmp(method, "ga"))     run_ga(&R, jit);
    else goto done;

    out->best = R.bestf;
    out->evals = R.spent;
    out->restarts = restarts;
    out->method = method;
    if (out->x) memcpy(out->x, R.best, (size_t)p->n * sizeof *out->x);
    rc = 0;
done:
    free(R.x); free(R.cand); free(R.best); free(R.fit); free(R.pop);
    return rc;
}

static int cmpd(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

int cjitter_compare(const cjitter_problem *p, const cjitter_budget *b, long seeds, void *stream)
{
    FILE *f = stream ? (FILE *)stream : stdout;
    double *v = NULL, med[8], spread[8], best[8];
    long m, s, nm = 0;
    int rc = -1;

    if (!p || !b || seeds < 1) return -1;
    while (cjitter_methods[nm]) nm++;
    v = malloc((size_t)seeds * sizeof *v);
    if (!v) return -1;

    fprintf(f, "%-8s %12s %12s %10s\n", "method", "median", "spread", "vs random");
    for (m = 0; m < nm; m++) {
        for (s = 0; s < seeds; s++) {
            cjitter_budget bb = *b;
            cjitter_result r;
            memset(&r, 0, sizeof r);
            bb.seed = (uint32_t)(1u + 7919u * (unsigned)s);
            if (cjitter_run(cjitter_methods[m], p, &bb, &r) != 0) goto done;
            v[s] = r.best;
        }
        qsort(v, (size_t)seeds, sizeof *v, cmpd);
        med[m] = v[(seeds - 1) / 2];
        best[m] = v[0];
        spread[m] = seeds > 1 ? v[seeds - 1] - v[0] : 0.0;
    }
    for (m = 0; m < nm; m++) {
        const char *verdict;
        /* Method 0 is the control. Beating its LUCKIEST seed is the bar, not beating its median
         * by more than its own range: random search is erratic, so its range is wide, and a
         * method that lands on the optimum every time would otherwise be called noise. Between
         * the control's best and its median is where a margin cannot be told from luck. */
        if (m == 0)                  verdict = "the control";
        else if (med[m] < best[0])   verdict = "better";
        else if (med[m] < med[0])    verdict = "inside noise";
        else                         verdict = "no better";
        fprintf(f, "%-8s %12.6g %12.6g %10s\n", cjitter_methods[m], med[m], spread[m], verdict);
    }
    fprintf(f, "\n%ld seeds at %ld evaluations each. A method counts as better only if its\n"
               "median beats the control's luckiest seed. If none does, uniform sampling is the\n"
               "honest answer for this problem at this budget.\n", seeds, b->evals);
    rc = 0;
done:
    free(v);
    return rc;
}
