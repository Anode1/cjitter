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
    t.ga_crossover      = 1.0;             /* the shipped GA: every child a blend */
    t.block             = n > 0 ? n : 1;   /* the whole vector: today's trajectories */
    t.jitter            = 0.1;
    t.pop               = GA_POP_DEFAULT;
    t.verify            = 0;               /* off: report the smallest observation */
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
           t->ga_mutate_decay >= 0 && t->ga_mutate_decay <= 1 &&
           t->ga_crossover >= 0 && t->ga_crossover <= 1 &&
           t->block >= 1 &&
           t->verify >= 0 &&
           t->jitter >= 0 &&      /* false for NaN too, which this refuses on purpose */
           t->pop >= 2;           /* one only re-scores a point; zero is no population */
}

/* Scratch for one run. Allocated once here, so no search step allocates. */
typedef struct {
    const cjitter_problem *p;
    double  *x, *cand, *best;
    double  *pop, *fit;          /* ga */
    long     npop;
    Rng      rng;
    long     cursor;             /* first variable of the next block; see jitter() */
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

/* A jittered neighbour: each variable in the proposal's block moved by a normal draw scaled to
 * its own range, the rest copied. The blocks tile the vector in order and cycle, one per call,
 * so successive proposals walk across the problem; cjitter.h's tuning comment says when a
 * block narrower than n is worth having. At block >= n this is the whole vector, drawing n
 * values in index order exactly as it always did: the default must not move a single
 * trajectory, so that path is written to be the same arithmetic in the same sequence. */
static void jitter(Run *R, const double *from, double *to, double scale)
{
    long j, first = 0, last = R->p->n;
    if (R->tun.block < R->p->n) {
        first = R->cursor;
        last = first + R->tun.block;
        if (last > R->p->n) last = R->p->n;   /* a short last block when n is not a multiple */
        R->cursor = last < R->p->n ? last : 0;
        for (j = 0; j < first; j++)      to[j] = from[j];
        for (j = last; j < R->p->n; j++) to[j] = from[j];
    }
    for (j = first; j < last; j++)
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
            const double *pa = R->pop + (R->fit[a] < R->fit[b] ? a : b) * n;
            double *kid = next + i * n;
            /* The crossover gate draws nothing at 1: that keeps the default the same
             * arithmetic in the same order as before the field existed, so no shipped
             * trajectory moves. Below 1 the gate costs one draw per child, blend or not. */
            if (R->tun.ga_crossover >= 1.0 || uni(&R->rng) < R->tun.ga_crossover) {
                long c = (long)(uni(&R->rng) * (double)np);
                long d = (long)(uni(&R->rng) * (double)np);
                const double *pb = R->pop + (R->fit[c] < R->fit[d] ? c : d) * n;
                for (j = 0; j < n; j++) {
                    double w = uni(&R->rng);
                    kid[j] = w * pa[j] + (1.0 - w) * pb[j];
                }
            } else {
                memcpy(kid, pa, (size_t)n * sizeof *kid);
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
    /* NULL and "auto" both take the default method, currently climb, tied to CJITTER_VERSION
     * because changing it changes what a seed reproduces. cjitter.h says which benchmark the
     * choice comes from and which one disagrees with it. */
    if (!method || !strcmp(method, "auto")) method = "climb";
    for (mi = 0; cjitter_methods[mi] && strcmp(method, cjitter_methods[mi]); mi++)
        ;
    if (!cjitter_methods[mi]) return -1;

    memset(&R, 0, sizeof R);
    R.tun = t ? *t : cjitter_tuning_default(p->n);
    if (!tuning_ok(&R.tun)) return -1;
    R.p = p;
    R.budget = b->evals;
    R.npop = R.tun.pop;
    if (R.npop > b->evals) R.npop = b->evals;
    jit = R.tun.jitter;
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

    /* What the search delivered, as opposed to the luckiest thing it saw. These evaluations are
     * made at the point being returned, after the search, through the caller's fitness directly
     * rather than through score(): they are deliberately outside the budget, so switching
     * verification on cannot shorten a search or move a trajectory. On a deterministic fitness
     * every draw equals bestf, so verified == best and inflation == 0 exactly. */
    out->verified = R.bestf;
    out->inflation = 0;
    out->verify_evals = 0;
    if (R.tun.verify > 0 && R.has_best) {
        /* A running mean, not a sum divided at the end: on a deterministic fitness every draw
         * is the same value and this returns it EXACTLY, where summing k copies and dividing
         * by k does not (0.1 added 25 times is not 2.5). That exactness is what lets the
         * header promise the check is inert where it is not needed, and it is the same
         * streaming form linearr's five-line least squares uses. */
        double m = 0;
        long v;
        for (v = 0; v < R.tun.verify; v++) {
            double f = p->fitness(R.best, p->ctx);
            m += (f - m) / (double)(v + 1);
        }
        out->verified = m;
        out->inflation = m - R.bestf;
        out->verify_evals = R.tun.verify;
    }
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
                          const cjitter_tuning *t, long seeds, FILE *stream)
{
    FILE *f = stream ? stream : stdout;
    double *sc = NULL, *v = NULL, *inf = NULL, *pv = NULL, *adj = NULL;
    long *wn = NULL, *nn = NULL;
    cjitter_tuning eff;
    long verify;
    uint32_t base;
    long m, s, nm = 0;
    int rc = -1;

    /* The cap is two guards in one: past it the size arithmetic below can wrap, and past
     * about a thousand the exact tests' doubles stop being exact. A thousand seeds is
     * beyond any panel this comparison is for. */
    if (!p || !b || seeds < 1 || seeds > 1000) return -1;
    eff = t ? *t : cjitter_tuning_default(p->n);
    verify = eff.verify;
    while (cjitter_methods[nm]) nm++;
    sc = malloc((size_t)nm * (size_t)seeds * sizeof *sc);
    v  = malloc((size_t)seeds * sizeof *v);
    inf = malloc((size_t)nm * (size_t)seeds * sizeof *inf);
    pv  = malloc((size_t)nm * sizeof *pv);
    adj = malloc((size_t)nm * sizeof *adj);
    wn  = malloc((size_t)nm * sizeof *wn);
    nn  = malloc((size_t)nm * sizeof *nn);
    if (!sc || !v || !inf || !pv || !adj || !wn || !nn) goto done;

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
            /* Judge on what the search delivered when the caller paid to find that out. The
             * smallest observation is the luckiest draw, and how much luck it carries differs
             * by method, so a panel of them is not a fair comparison on a noisy objective. */
            sc[m * seeds + s] = verify > 0 ? r.verified : r.best;
            inf[m * seeds + s] = r.inflation;
        }

    /* Every non-control method is tested against the same control, so the three tests are
     * one family: at 5% each the chance of calling at least one of three null methods better
     * is 1 - 0.95^3, about 14%. The verdict column is therefore read off the Holm-adjusted
     * value, and the raw sign-p is printed beside it so both are visible. */
    for (m = 1; m < nm; m++) {
        long wins = 0, n = 0;
        for (s = 0; s < seeds; s++) {
            if (sc[m * seeds + s] == sc[s]) continue;
            n++;
            if (sc[m * seeds + s] < sc[s]) wins++;
        }
        wn[m] = wins; nn[m] = n;
        pv[m] = n > 0 ? sign_p(wins, n) : 1.0;
    }
    {   /* Holm step-down over the nm-1 comparisons, monotone by construction. */
        long k, i, done_n = 0;
        double prev = 0.0;
        for (m = 1; m < nm; m++) adj[m] = -1.0;
        for (k = 1; k < nm; k++) {
            long besti = -1;
            for (i = 1; i < nm; i++)
                if (adj[i] < 0 && (besti < 0 || pv[i] < pv[besti])) besti = i;
            if (besti < 0) break;
            {
                double a = (double)(nm - 1 - done_n) * pv[besti];
                if (a > 1.0) a = 1.0;
                if (a < prev) a = prev;
                adj[besti] = a; prev = a; done_n++;
            }
        }
    }

    fprintf(f, "%-8s %12s %12s %7s %9s %9s %11s", "method",
            verify > 0 ? "median(ver)" : "median", "range", "wins", "sign-p", "holm",
            "vs random");
    if (verify > 0) fprintf(f, " %12s", "inflation");
    fprintf(f, "\n");
    for (m = 0; m < nm; m++) {
        double med, range;
        memcpy(v, sc + m * seeds, (size_t)seeds * sizeof *v);
        qsort(v, (size_t)seeds, sizeof *v, cmpd);
        med = v[(seeds - 1) / 2];
        range = v[seeds - 1] - v[0];
        if (m == 0) {
            fprintf(f, "%-8s %12.6g %12.6g %7s %9s %9s %11s",
                    cjitter_methods[m], med, range, "-", "-", "-", "the control");
        } else {
            /* Wins on the paired per-seed differences; a tie counts for neither side. */
            fprintf(f, "%-8s %12.6g %12.6g %4ld/%-2ld %9.3g %9.3g %11s",
                    cjitter_methods[m], med, range, wn[m], nn[m], pv[m], adj[m],
                    adj[m] <= 0.05 ? "better" : "not shown");
        }
        if (verify > 0) {
            memcpy(v, inf + m * seeds, (size_t)seeds * sizeof *v);
            qsort(v, (size_t)seeds, sizeof *v, cmpd);
            fprintf(f, " %12.6g", v[(seeds - 1) / 2]);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\n%ld seeds at %ld evaluations each, every method on the same seeds. A method\n"
               "is better when it beats the control on enough of them that a fair coin explains\n"
               "it with probability at most 5%% after correcting for the number of methods\n"
               "compared: the holm column. Beside it, sign-p is the uncorrected\n"
               "exact one-sided sign test on the paired per-seed differences. not shown is a\n"
               "failure to demonstrate improvement, and says nothing about equality.\n",
               seeds, b->evals);
    if (verify > 0)
        fprintf(f, "Judged on the mean of %ld fresh evaluations of each returned point, not on\n"
                   "the smallest value seen, which on a noisy objective is the luckiest draw the\n"
                   "search took and carries more luck for a method that resamples one place.\n"
                   "inflation is that luck: the median of verified minus reported, per method.\n"
                   "Those evaluations are extra and are not part of the %ld above.\n",
                verify, b->evals);
    rc = 0;
done:
    free(sc);
    free(v);
    free(inf);
    free(pv);
    free(adj);
    free(wn);
    free(nn);
    return rc;
}

int cjitter_compare(const cjitter_problem *p, const cjitter_budget *b, long seeds, FILE *stream)
{
    return cjitter_compare_tuned(p, b, NULL, seeds, stream);
}
