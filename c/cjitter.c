/* cjitter.c -- the four searches, and the comparison between them.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cjitter.h"
#include "rng.h"

const char *const cjitter_methods[] = { "random", "climb", "anneal", "ga", NULL };

/* The shipped constants, in one place; cjitter_tuning_default hands them to the caller.
 * ANNEAL_PROBE_MIN is not in the tuning because it only guards a degenerate budget. */
#define CLIMB_PATIENCE(n)  (40 + 10 * (n))   /* rejections before the move size shrinks */
#define CLIMB_SHRINK       0.5
#define CLIMB_RESTART_AT   (1.0 / 64.0)
#define ANNEAL_PROBES      20
#define ANNEAL_PROBE_MIN   10                /* probes never exceed budget/this */
#define ANNEAL_COOL_LN     -6.907755278982137 /* ln(1e-3) */
#define ANNEAL_MOVE_DECAY  0.9
#define GA_POP_DEFAULT     30
#define GA_MUTATE          0.3
#define GA_MUTATE_DECAY    0.9
#define COMPARE_SEED_STEP  7919u             /* stride between compare's panel seeds */

cjitter_tuning cjitter_tuning_default(long n)
{
    cjitter_tuning t;
    t.climb_patience    = CLIMB_PATIENCE(n > 0 ? n : 1);
    t.climb_shrink      = CLIMB_SHRINK;
    t.climb_restart_at  = CLIMB_RESTART_AT;
    t.anneal_probes     = ANNEAL_PROBES;
    t.anneal_cool_ln    = ANNEAL_COOL_LN;
    t.anneal_move_decay = ANNEAL_MOVE_DECAY;
    t.ga_mutate         = GA_MUTATE;
    t.ga_mutate_decay   = GA_MUTATE_DECAY;
    return t;
}

/* Every field literal, every field checked against the range the header states. */
static int tuning_ok(const cjitter_tuning *t)
{
    return t->climb_patience >= 1 &&
           t->climb_shrink > 0 && t->climb_shrink < 1 &&
           t->climb_restart_at >= 0 && t->climb_restart_at <= 1 &&
           t->anneal_probes >= 1 &&
           t->anneal_cool_ln <= 0 &&
           t->anneal_move_decay >= 0 && t->anneal_move_decay <= 1 &&
           t->ga_mutate >= 0 &&
           t->ga_mutate_decay >= 0 && t->ga_mutate_decay <= 1;
}

/* Scratch for one run. Allocated once here, so no search step allocates. */
typedef struct {
    const cjitter_problem *p;
    double  *x, *cand, *best;
    double  *pop, *fit;          /* ga */
    long     npop;
    Rng      rng;
    long     spent, budget;
    double   bestf;
    int      has_best;           /* best/bestf are defined; set by the first keep() */
    cjitter_tuning tun;
} Run;

static double uni(Rng *r) { return (double)cjitter_rng_u32(r) / 4294967296.0; }

/* Sum of 12 uniforms, mean 6, variance 1: an approximate normal in pure arithmetic. A normal
 * step is what makes the move size mean something across dimensions of different width.
 * cjitter.h states the allowed operations; Box-Muller's log and cos are not among them. */
static double gauss(Rng *r)
{
    double s = 0;
    int i;
    for (i = 0; i < 12; i++) s += uni(r);
    return s - 6.0;
}

/* exp(x) for x <= 0, pure arithmetic: e^x = (e^(x/64))^64, the inner factor by Taylor series
 * (|x/64| is small, so it converges in a few terms), the power by six squarings. Exists so
 * anneal's acceptance threshold does not depend on whose libm rounded exp. */
static double exp_neg(double x)
{
    double y, t = 1.0, s = 1.0;
    int i;
    if (x > 0) x = 0;
    if (x < -40) return 0;         /* below any acceptance probability a draw can meet */
    y = x / 64.0;
    for (i = 1; i <= 8; i++) { t *= y / (double)i; s += t; }
    for (i = 0; i < 6; i++) s *= s;
    return s;
}

/* Box first, then the caller's repair, then the box again: a repair that overshoots cannot
 * move a point outside the box even by accident. */
static void clamp(const cjitter_problem *p, double *x)
{
    long j;
    for (j = 0; j < p->n; j++) {
        if (x[j] < p->lo[j]) x[j] = p->lo[j];
        if (x[j] > p->hi[j]) x[j] = p->hi[j];
    }
    if (p->repair) {
        p->repair(x, p->ctx);
        for (j = 0; j < p->n; j++) {
            if (x[j] < p->lo[j]) x[j] = p->lo[j];
            if (x[j] > p->hi[j]) x[j] = p->hi[j];
        }
    }
}

static void draw(Run *R, double *x)
{
    long j;
    for (j = 0; j < R->p->n; j++)
        x[j] = R->p->lo[j] + uni(&R->rng) * (R->p->hi[j] - R->p->lo[j]);
    clamp(R->p, x);
}

/* Every evaluation goes through here, so the budget is exact and no method can quietly spend
 * more than another. The methods are responsible for not calling past the budget; the guard
 * turns a missed one into an immediate failure instead of a silently unfair comparison,
 * which is how one overspend (climb's restart on the last evaluation) shipped before it
 * existed. Not an assert: -DNDEBUG must not remove it. */
static double score(Run *R, const double *x)
{
    if (R->spent >= R->budget) {
        fprintf(stderr, "cjitter: internal error: a method overspent its budget\n");
        abort();
    }
    R->spent++;
    return R->p->fitness(x, R->p->ctx);
}

/* The first scored point always becomes best, whatever its value: a fitness that only ever
 * returns HUGE_VAL or NaN must still leave OUT->x holding a point that was actually scored,
 * never uninitialized memory. */
static void keep(Run *R, const double *x, double f)
{
    if (!R->has_best || f < R->bestf) {
        R->has_best = 1;
        R->bestf = f;
        memcpy(R->best, x, (size_t)R->p->n * sizeof *x);
    }
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
    long stuck = 0, patience = R->tun.climb_patience;
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
            scale *= R->tun.climb_shrink;
            stuck = 0;
            /* As local as it is going to get: restart, if there is budget left to score the
             * new start. Without that check this was the one path that could score twice in
             * an iteration and spend budget+1, which score()'s comment says cannot happen. */
            if (scale < jit * R->tun.climb_restart_at && R->spent < R->budget) {
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
    draw(R, R->x);
    f = score(R, R->x);
    keep(R, R->x, f);
    /* Scale the starting temperature to the problem: the mean uphill step of a few probes.
     * The probes are paid for from the budget, so their scores count toward best. */
    {
        double s = 0;
        long i, m = R->tun.anneal_probes;
        if (m > R->budget / ANNEAL_PROBE_MIN) m = R->budget / ANNEAL_PROBE_MIN;
        if (m < 1) m = 1;
        for (i = 0; i < m && R->spent < R->budget; i++) {
            double g;
            jitter(R, R->x, R->cand, jit);
            g = score(R, R->cand);
            keep(R, R->cand, g);
            s += fabs(g - f);
        }
        if (s > 0) t0 = s / (double)m;
    }
    while (R->spent < R->budget) {
        double g, frac = (double)R->spent / (double)R->budget;
        t = t0 * exp_neg(frac * R->tun.anneal_cool_ln);
        jitter(R, R->x, R->cand, jit * (1.0 - R->tun.anneal_move_decay * frac));
        g = score(R, R->cand);
        if (g < f || (t > 0 && uni(&R->rng) < exp_neg((f - g) / t))) {
            f = g;
            memcpy(R->x, R->cand, (size_t)R->p->n * sizeof *R->x);
            keep(R, R->x, f);
        }
    }
}

/* Tournament selection, blend crossover, jittered mutation, one elite carried. The mutation
 * scale decays over the run like anneal's move size: held constant it re-scattered every
 * converged layout each generation, and the GA's floor was a noise floor. */
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
        double frac = (double)R->spent / (double)R->budget;
        long elite = 0;
        for (i = 1; i < np; i++) if (R->fit[i] < R->fit[elite]) elite = i;
        memcpy(next, R->pop + elite * n, (size_t)n * sizeof *next);
        for (i = 1; i < np; i++) {
            /* uni() < 1, so (long)(uni()*np) is at most np-1 for any np this library can
             * allocate: the product stays strictly below np in double. */
            long a = (long)(uni(&R->rng) * (double)np), b = (long)(uni(&R->rng) * (double)np);
            long c = (long)(uni(&R->rng) * (double)np), d = (long)(uni(&R->rng) * (double)np);
            const double *pa, *pb;
            double *kid = next + i * n;
            pa = R->pop + (R->fit[a] < R->fit[b] ? a : b) * n;
            pb = R->pop + (R->fit[c] < R->fit[d] ? c : d) * n;
            for (j = 0; j < n; j++) {
                double w = uni(&R->rng);
                kid[j] = w * pa[j] + (1.0 - w) * pb[j];
            }
            jitter(R, kid, kid,
                   jit * R->tun.ga_mutate * (1.0 - R->tun.ga_mutate_decay * frac));
        }
        memcpy(R->pop, next, (size_t)np * (size_t)n * sizeof *next);
        for (i = 0; i < np && R->spent < R->budget; i++) {
            R->fit[i] = score(R, R->pop + i * n);
            keep(R, R->pop + i * n, R->fit[i]);
        }
    }
}

int cjitter_run_tuned(const char *method, const cjitter_problem *p, const cjitter_budget *b,
                      const cjitter_tuning *t, cjitter_result *out)
{
    Run R;
    long restarts = 0, j, mi;
    double jit;
    int rc = -1;

    if (!p || !b || !out || !p->fitness || p->n < 1 || b->evals < 1) return -1;
    if (!p->lo || !p->hi) return -1;
    for (j = 0; j < p->n; j++)
        if (!(p->lo[j] <= p->hi[j])) return -1;   /* also refuses NaN bounds */
    if (!(b->jitter >= 0) || b->pop < 0) return -1;   /* also refuses NaN jitter */
    /* NULL and "auto" both take the default method: the one the shipped benchmarks rank
     * most budget-efficient, currently climb, tied to CJITTER_VERSION because changing it
     * changes what a seed reproduces. */
    if (!method || !strcmp(method, "auto")) method = "climb";
    for (mi = 0; cjitter_methods[mi] && strcmp(method, cjitter_methods[mi]); mi++)
        ;
    if (!cjitter_methods[mi]) return -1;
    if (mi == 3 && b->pop == 1) return -1;   /* a population of one only re-scores a point */

    memset(&R, 0, sizeof R);
    R.tun = t ? *t : cjitter_tuning_default(p->n);
    if (!tuning_ok(&R.tun)) return -1;
    R.p = p;
    R.budget = b->evals;
    R.npop = b->pop > 0 ? b->pop : GA_POP_DEFAULT;
    if (R.npop > b->evals) R.npop = b->evals;
    jit = b->jitter > 0 ? b->jitter : 0.1;
    cjitter_rng_seed(&R.rng, b->seed ? b->seed : 1u);

    R.x    = malloc((size_t)p->n * sizeof *R.x);
    R.cand = malloc((size_t)p->n * sizeof *R.cand);
    R.best = malloc((size_t)p->n * sizeof *R.best);
    if (!R.x || !R.cand || !R.best) goto done;
    if (mi == 3) {                       /* only the ga owns population scratch */
        R.fit = malloc((size_t)R.npop * sizeof *R.fit);
        R.pop = malloc((size_t)R.npop * (size_t)p->n * 2 * sizeof *R.pop);
        if (!R.fit || !R.pop) goto done;
    }

    switch (mi) {
    case 0: run_random(&R); break;
    case 1: run_climb(&R, jit, &restarts); break;
    case 2: run_anneal(&R, jit); break;
    default: run_ga(&R, jit); break;
    }

    out->best = R.bestf;
    out->evals = R.spent;
    out->restarts = restarts;
    out->method = cjitter_methods[mi];   /* never the caller's pointer, which may not outlive us */
    if (out->x) memcpy(out->x, R.best, (size_t)p->n * sizeof *out->x);
    rc = 0;
done:
    free(R.x); free(R.cand); free(R.best); free(R.fit); free(R.pop);
    return rc;
}

int cjitter_run(const char *method, const cjitter_problem *p, const cjitter_budget *b,
                cjitter_result *out)
{
    return cjitter_run_tuned(method, p, b, NULL, out);
}

static int cmpd(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* The chance of at least W wins in N tries of a fair coin: the exact one-sided sign test.
 * N is a seed count, so the binomial sum in doubles is exact to far more digits than the
 * three that get printed. */
static double sign_p(long w, long n)
{
    double c = 1.0, half = 1.0, sum = 0.0;
    long k;
    for (k = 0; k < n; k++) half *= 0.5;
    for (k = 0; k <= n; k++) {
        if (k >= w) sum += c * half;
        c = c * (double)(n - k) / (double)(k + 1);
    }
    return sum < 1 ? sum : 1;
}

int cjitter_compare_tuned(const cjitter_problem *p, const cjitter_budget *b,
                          const cjitter_tuning *t, long seeds, void *stream)
{
    FILE *f = stream ? (FILE *)stream : stdout;
    double *sc = NULL, *v = NULL;
    uint32_t base;
    long m, s, nm = 0;
    int rc = -1;

    /* The cap is two guards in one: past it the size arithmetic below can wrap, and past
     * about a thousand the exact tests' doubles stop being exact. A thousand seeds is
     * beyond any panel this comparison is for. */
    if (!p || !b || seeds < 1 || seeds > 1000) return -1;
    while (cjitter_methods[nm]) nm++;
    sc = malloc((size_t)nm * (size_t)seeds * sizeof *sc);
    v  = malloc((size_t)seeds * sizeof *v);
    if (!sc || !v) goto done;

    /* The same seed panel for every method, based on the caller's seed, so the per-seed
     * differences below are paired and a fresh panel is one seed away. */
    base = b->seed ? b->seed : 1u;
    for (m = 0; m < nm; m++)
        for (s = 0; s < seeds; s++) {
            cjitter_budget bb = *b;
            cjitter_result r;
            memset(&r, 0, sizeof r);
            bb.seed = base + COMPARE_SEED_STEP * (uint32_t)s;
            if (cjitter_run_tuned(cjitter_methods[m], p, &bb, t, &r) != 0) goto done;
            sc[m * seeds + s] = r.best;
        }

    fprintf(f, "%-8s %12s %12s %7s %9s %11s\n",
            "method", "median", "range", "wins", "sign-p", "vs random");
    for (m = 0; m < nm; m++) {
        double med, range;
        memcpy(v, sc + m * seeds, (size_t)seeds * sizeof *v);
        qsort(v, (size_t)seeds, sizeof *v, cmpd);
        med = v[(seeds - 1) / 2];
        range = v[seeds - 1] - v[0];
        if (m == 0) {
            fprintf(f, "%-8s %12.6g %12.6g %7s %9s %11s\n",
                    cjitter_methods[m], med, range, "-", "-", "the control");
        } else {
            /* Wins on the paired per-seed differences; a tie counts for neither side. */
            long wins = 0, n = 0;
            double pval;
            for (s = 0; s < seeds; s++) {
                if (sc[m * seeds + s] == sc[s]) continue;
                n++;
                if (sc[m * seeds + s] < sc[s]) wins++;
            }
            pval = n > 0 ? sign_p(wins, n) : 1.0;
            fprintf(f, "%-8s %12.6g %12.6g %4ld/%-2ld %9.3g %11s\n",
                    cjitter_methods[m], med, range, wins, n, pval,
                    pval <= 0.05 ? "better" : "not shown");
        }
    }
    fprintf(f, "\n%ld seeds at %ld evaluations each, every method on the same seeds. A method\n"
               "is better when it beats the control on enough of them that a fair coin explains\n"
               "it with probability at most 5%% (the sign-p column, an exact one-sided sign\n"
               "test on the paired per-seed differences). not shown is a failure to demonstrate\n"
               "improvement, and says nothing about equality.\n", seeds, b->evals);
    rc = 0;
done:
    free(sc);
    free(v);
    return rc;
}

int cjitter_compare(const cjitter_problem *p, const cjitter_budget *b, long seeds, void *stream)
{
    return cjitter_compare_tuned(p, b, NULL, seeds, stream);
}
