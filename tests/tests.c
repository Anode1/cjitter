/* tests.c -- in-process unit tests for the four searches and the comparison.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * Style follows bpnn: linear, inline, one comment per check saying what it verifies. What a
 * unit suite can see here is the library's contract -- refusals, the exact budget, determinism
 * from a seed, the box and the repair invariant held on every point scored. What it cannot see
 * is an exit code or a usage line; those live in tests/cli.sh.
 *
 * The one check that is about search quality, climb on a sphere, is safe to assert exactly
 * because everything is deterministic given a seed: it is a regression pin, not a benchmark.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cjitter.h"
#include "rng.h"

static int t_run, t_fail;

#define CHECK(cond, msg)                              \
    do {                                              \
        t_run++;                                      \
        if (cond) {                                   \
            printf("ok   %s\n", msg);                 \
        } else {                                      \
            t_fail++;                                 \
            printf("FAIL %s\n", msg);                 \
        }                                             \
    } while (0)

/* Watches every point the searches score: counts calls, tracks the minimum, and records any
 * point outside the box or violating the repair invariant. The searches promise all three. */
typedef struct {
    const double *lo, *hi;
    long   n, calls;
    int    outside, unrepaired, want_repair;
    double min;
} Watch;

static double watched_sphere(const double *x, void *ctx)
{
    Watch *w = ctx;
    double f = 0;
    long j;
    w->calls++;
    for (j = 0; j < w->n; j++) {
        if (x[j] < w->lo[j] || x[j] > w->hi[j]) w->outside = 1;
        f += x[j] * x[j];
    }
    if (w->want_repair && x[0] < 0.5) w->unrepaired = 1;
    if (f < w->min) w->min = f;
    return f;
}

/* The repair used below: x[0] at least 0.5, a hard constraint the box does not express. */
static void floor_first(double *x, void *ctx)
{
    (void)ctx;
    if (x[0] < 0.5) x[0] = 0.5;
}

/* A fitness with no finite value anywhere, for the degenerate-return check. */
static double always_worst(const double *x, void *ctx)
{
    (void)x; (void)ctx;
    return HUGE_VAL;
}

/* A fitness that alternates a unit below and a unit above the sphere, so the smallest value
 * ever observed at a point is a unit better than that point is worth. Deterministic, so the
 * check below is exact rather than statistical: this is what a noisy objective does to the
 * reported best, without needing noise to do it. */
static double alternating(const double *x, void *ctx)
{
    Watch *w = ctx;
    double f = 0;
    long j;
    for (j = 0; j < w->n; j++) f += x[j] * x[j];
    return f + ((w->calls++ % 2) ? 1.0 : -1.0);
}

/* Records, per variable, whether it ever took a value other than the one it held at the first
 * point scored, and the largest number of variables any one proposal changed at once. Together
 * those are what a block claims: no proposal moves more than BLOCK of them, and the cursor
 * advances far enough that every one of them eventually moves. */
typedef struct {
    long   n, calls, widest;
    double first[8], prev[8];
    int    moved[8];
} Moves;

static double moves_probe(const double *x, void *ctx)
{
    Moves *m = ctx;
    double f = 0;
    long j, changed = 0;
    for (j = 0; j < m->n; j++) {
        if (m->calls == 0) m->first[j] = x[j];
        else {
            if (x[j] != m->first[j]) m->moved[j] = 1;
            if (x[j] != m->prev[j]) changed++;
        }
        m->prev[j] = x[j];
        f += x[j] * x[j];
    }
    if (m->calls > 0 && changed > m->widest) m->widest = changed;
    m->calls++;
    return f;
}

/* Records the first point a run scores, which is what cjitter_problem.start names. */
typedef struct {
    long   n, calls;
    double first[8];
} First;

static double first_probe(const double *x, void *ctx)
{
    First *w = ctx;
    double f = 0;
    long j;
    for (j = 0; j < w->n; j++) {
        if (w->calls == 0) w->first[j] = x[j];
        f += (x[j] - 0.25) * (x[j] - 0.25);
    }
    w->calls++;
    return f;
}

/* A repair that is a projection rather than a clamp: every point onto the disc of radius 1
 * about the origin, the shape the diagram experiment constrains a node to. The objective's
 * optimum is outside the disc, so the search presses against the constraint instead of
 * settling away from it, which is the case where a repair that overshoots would put a point
 * outside the box. The probe watches the box and the disc on every point it is handed. */
#define DISC_R 1.0

typedef struct {
    const double *lo, *hi;
    long n, calls;
    int  outside, offdisc;
} Disc;

static double disc_probe(const double *x, void *ctx)
{
    Disc *d = ctx;
    double f = 0, q = 0;
    long j;
    d->calls++;
    for (j = 0; j < d->n; j++) {
        if (x[j] < d->lo[j] || x[j] > d->hi[j]) d->outside = 1;
        q += x[j] * x[j];
        f += (x[j] - 3.0) * (x[j] - 3.0);
    }
    if (q > DISC_R * DISC_R * (1.0 + 1e-12)) d->offdisc = 1;
    return f;
}

static void disc_repair(double *x, void *ctx)
{
    Disc *d = ctx;
    double q = 0;
    long j;
    for (j = 0; j < d->n; j++) q += x[j] * x[j];
    if (q > DISC_R * DISC_R) {
        double k = DISC_R / sqrt(q);
        for (j = 0; j < d->n; j++) x[j] *= k;
    }
}

int main(void)
{
    static const double lo2[2] = { -5, -5 }, hi2[2] = { 5, 5 };

    /* rng: reproducible, in range, and 0-seed remapped (not stuck at zero) */
    {
        Rng g, h, z;
        double u;
        cjitter_rng_seed(&g, 42);
        cjitter_rng_seed(&h, 42);
        CHECK(cjitter_rng_u32(&g) == cjitter_rng_u32(&h), "rng: same seed gives the same sequence");
        u = cjitter_rng_uniform(&g, -1.0, 1.0);
        CHECK(u >= -1.0 && u < 1.0, "rng: uniform stays in [lo, hi)");
        cjitter_rng_seed(&z, 0);
        CHECK(cjitter_rng_u32(&z) != 0, "rng: seed 0 is remapped, not degenerate");
    }

    /* The method list is the interface's own order, NULL-terminated after the four. */
    CHECK(cjitter_methods[0] && !strcmp(cjitter_methods[0], "random") &&
          cjitter_methods[1] && !strcmp(cjitter_methods[1], "climb")  &&
          cjitter_methods[2] && !strcmp(cjitter_methods[2], "anneal") &&
          cjitter_methods[3] && !strcmp(cjitter_methods[3], "ga"),
          "methods: random, climb, anneal, ga, in that order");
    CHECK(cjitter_methods[4] == NULL, "methods: NULL-terminated after the four");

    /* Every bad argument is a -1, not a crash and not a silent default. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 100, 1 };
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
        cjitter_problem bad;
        cjitter_budget bb;
        cjitter_tuning tb;

        CHECK(cjitter_run("", &p, &b, &r) == -1, "run refuses an empty method name");
        CHECK(cjitter_run("climb", NULL, &b, &r) == -1, "run refuses a NULL problem");
        CHECK(cjitter_run("climb", &p, NULL, &r) == -1, "run refuses a NULL budget");
        CHECK(cjitter_run("climb", &p, &b, NULL) == -1, "run refuses a NULL result");
        CHECK(cjitter_run("simplex", &p, &b, &r) == -1, "run refuses a method it does not have");
        bad = p; bad.fitness = NULL;
        CHECK(cjitter_run("climb", &bad, &b, &r) == -1, "run refuses a NULL fitness");
        bad = p; bad.n = 0;
        CHECK(cjitter_run("climb", &bad, &b, &r) == -1, "run refuses n < 1");
        bad = p; bad.lo = NULL;
        CHECK(cjitter_run("climb", &bad, &b, &r) == -1, "run refuses a NULL lower bound");
        bad = p; bad.hi = NULL;
        CHECK(cjitter_run("climb", &bad, &b, &r) == -1, "run refuses a NULL upper bound");
        bad = p; bad.lo = hi2; bad.hi = lo2;
        CHECK(cjitter_run("climb", &bad, &b, &r) == -1, "run refuses an inverted box");
        bb = b; bb.evals = 0;
        CHECK(cjitter_run("climb", &p, &bb, &r) == -1, "run refuses a budget of no evaluations");
        tb = cjitter_tuning_default(2); tb.jitter = -0.1;
        CHECK(cjitter_run_tuned("climb", &p, &b, &tb, &r) == -1,
              "tuning refuses a negative jitter");
        tb = cjitter_tuning_default(2); tb.pop = -1;
        CHECK(cjitter_run_tuned("ga", &p, &b, &tb, &r) == -1,
              "tuning refuses a negative population");
        CHECK(w.calls == 0, "and no refusal called the fitness even once");
    }

    /* A fitness that never returns a finite value: the returned point must still be one that
     * was scored, inside the box, never uninitialized memory handed back with rc 0. */
    {
        cjitter_problem p = { 2, lo2, hi2, always_worst, NULL, NULL, NULL };
        cjitter_budget b = { 50, 1 };
        double x[2] = { 12345.0, 67890.0 };
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
        CHECK(cjitter_run("random", &p, &b, &r) == 0 &&
              x[0] >= lo2[0] && x[0] <= hi2[0] && x[1] >= lo2[1] && x[1] <= hi2[1],
              "a fitness stuck at HUGE_VAL still returns a scored in-box point");
    }

    /* Each method, at budgets including 1 and an odd number: the budget is spent exactly, the
     * result is the minimum of what was scored, every point obeyed the box, and the same seed
     * reproduces the same answer bit for bit. */
    {
        static const long budgets[3] = { 1, 37, 300 };
        long m, bi;
        for (m = 0; cjitter_methods[m]; m++) {
            const char *name = cjitter_methods[m];
            int exact = 1, isbest = 1, inbox = 1, again = 1;
            char msg[96];
            for (bi = 0; bi < 3; bi++) {
                Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
                cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
                cjitter_budget b = { 0, 7 };
                double x[2], y[2];
                cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 }, r2 = { 0, y, 0, 0, NULL, 0, 0, 0 };
                b.evals = budgets[bi];
                if (cjitter_run(name, &p, &b, &r) != 0) { exact = 0; break; }
                if (w.calls != budgets[bi] || r.evals != budgets[bi]) exact = 0;
                /* min of everything scored, and re-scoring the returned point agrees */
                if (r.best != w.min) isbest = 0;
                w.calls = 0;
                if (watched_sphere(x, &w) != r.best) isbest = 0;
                if (w.outside) inbox = 0;
                /* the same seed again: same best, same point */
                w.calls = 0; w.min = HUGE_VAL;
                if (cjitter_run(name, &p, &b, &r2) != 0 ||
                    r2.best != r.best || memcmp(x, y, sizeof x) != 0) again = 0;
            }
            snprintf(msg, sizeof msg, "%s: spends the budget exactly, at 1, 37 and 300", name);
            CHECK(exact, msg);
            snprintf(msg, sizeof msg, "%s: best is the minimum scored, and re-scoring it agrees", name);
            CHECK(isbest, msg);
            snprintf(msg, sizeof msg, "%s: never scores a point outside the box", name);
            CHECK(inbox, msg);
            snprintf(msg, sizeof msg, "%s: the same seed reproduces best and point bit for bit", name);
            CHECK(again, msg);
        }
    }

    /* The repair callback is applied to every proposal, not just the returned one: with x[0]
     * floored at 0.5, no scored point and no returned point may ever be below it. The GA's
     * crossover is the path that would break this, blending parents into an unrepaired child. */
    {
        long m;
        int held = 1, returned = 1;
        for (m = 0; cjitter_methods[m]; m++) {
            Watch w = { lo2, hi2, 2, 0, 0, 0, 1, HUGE_VAL };
            cjitter_problem p = { 2, lo2, hi2, watched_sphere, floor_first, &w, NULL };
            cjitter_budget b = { 400, 3 };
            cjitter_tuning tw = cjitter_tuning_default(2);
            double x[2];
            cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
            tw.jitter = 0.2;   /* the width this check was calibrated at, kept through the move */
            if (cjitter_run_tuned(cjitter_methods[m], &p, &b, &tw, &r) != 0 || w.unrepaired)
                held = 0;
            if (x[0] < 0.5) returned = 0;
        }
        CHECK(held, "repair: every scored point satisfied the constraint, all four methods");
        CHECK(returned, "repair: every returned best satisfies it too");
    }

    /* The tuning: NULL and cjitter_tuning_default(n) are the same run bit for bit; every
     * field is literal, so a zeroed struct is refused rather than silently defaulted, a
     * field with the wrong sign is refused (anneal_cool_ln is only valid NEGATIVE, the case
     * an earlier blanket "negatives are refused" sentence got backwards), and ga_mutate 0
     * is a real mutation ablation that runs. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 300, 9 };
        cjitter_tuning dflt = cjitter_tuning_default(2);
        cjitter_tuning zero = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        cjitter_tuning bad;
        double x[2], y[2];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 }, r2 = { 0, y, 0, 0, NULL, 0, 0, 0 };
        CHECK(dflt.climb_patience == 60, "tuning_default: patience is 40 + 10n");
        CHECK(cjitter_run("climb", &p, &b, &r) == 0 &&
              cjitter_run_tuned("climb", &p, &b, &dflt, &r2) == 0 &&
              r.best == r2.best && memcmp(x, y, sizeof x) == 0,
              "tuning: NULL and the returned defaults are the same run bit for bit");
        w.calls = 0;
        CHECK(cjitter_run_tuned("climb", &p, &b, &zero, &r) == -1 && w.calls == 0,
              "tuning: a zeroed struct is refused, before any evaluation");
        bad = dflt; bad.anneal_cool_ln = 1.0;
        CHECK(cjitter_run_tuned("anneal", &p, &b, &bad, &r) == -1,
              "tuning: a positive anneal_cool_ln is refused");
        bad = dflt; bad.ga_mutate = -0.1;
        CHECK(cjitter_run_tuned("ga", &p, &b, &bad, &r) == -1,
              "tuning: a negative ga_mutate is refused");
        bad = dflt; bad.ga_mutate = 0;
        CHECK(cjitter_run_tuned("ga", &p, &b, &bad, &r) == 0,
              "tuning: ga_mutate 0 is an ablation that runs, not a default");
        bad = dflt; bad.ga_crossover = 1.5;
        CHECK(cjitter_run_tuned("ga", &p, &b, &bad, &r) == -1,
              "tuning: a crossover chance past 1 is refused");
        /* The crossover ablation: at 0 every child is a mutated tournament winner. It must
         * run, spend exactly, and be a different trajectory from the shipped GA; that
         * difference is what makes ga_crossover = 0 a real ablation rather than a synonym. */
        w.calls = 0;
        bad = dflt; bad.ga_crossover = 0;
        CHECK(cjitter_run_tuned("ga", &p, &b, &bad, &r) == 0 && w.calls == b.evals &&
              cjitter_run_tuned("ga", &p, &b, &dflt, &r2) == 0 &&
              (r.best != r2.best || memcmp(x, y, sizeof x) != 0),
              "tuning: ga_crossover 0 runs, spends exactly, and is not the shipped GA");
        bad = dflt; bad.climb_shrink = 1.0;
        {
            FILE *tf = tmpfile();
            long wrote = -1;
            if (tf) {
                int crc = cjitter_compare_tuned(&p, &b, &bad, 2, tf);
                fseek(tf, 0, SEEK_END);
                wrote = crc == -1 ? ftell(tf) : -1;
                fclose(tf);
            }
            CHECK(wrote == 0,
                  "tuning: compare refuses a bad one before printing anything");
        }
    }

    /* The block: how many variables one proposal moves. The default is n, the whole vector,
     * because a default that moved a trajectory would change what every published seed
     * reproduces; anything at or above n has to mean the same thing. A narrower block tiles the
     * vector and cycles, so the checks are that it is refused when out of range, that it still
     * spends the budget exactly and holds the box and the repair, and above all that the cursor
     * advances: a cursor stuck at zero would pin the tail of the vector and still look from the
     * outside like a working search. The short last block, when n is not a multiple, is the
     * only way the last variables are ever reached, so it gets its own check. */
    {
        static const double lo8[8] = { -5, -5, -5, -5, -5, -5, -5, -5 };
        static const double hi8[8] = {  5,  5,  5,  5,  5,  5,  5,  5 };
        Watch w = { lo8, hi8, 8, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 8, lo8, hi8, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 400, 3 };
        cjitter_tuning dflt = cjitter_tuning_default(8);
        cjitter_tuning t;
        double x[8], y[8];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 }, r2 = { 0, y, 0, 0, NULL, 0, 0, 0 };
        long m;

        CHECK(dflt.block == 8, "block: the default is n, the whole vector");
        t = dflt; t.block = 0;
        w.calls = 0;
        CHECK(cjitter_run_tuned("climb", &p, &b, &t, &r) == -1 && w.calls == 0,
              "block: 0 is refused, before any evaluation");
        t = dflt; t.block = 108;
        CHECK(cjitter_run_tuned("climb", &p, &b, &dflt, &r) == 0 &&
              cjitter_run_tuned("climb", &p, &b, &t, &r2) == 0 &&
              r.best == r2.best && memcmp(x, y, sizeof x) == 0,
              "block: at or above n is the whole vector, bit for bit");
        t = dflt; t.block = 2;
        CHECK(cjitter_run_tuned("climb", &p, &b, &t, &r2) == 0 && r2.best != r.best,
              "block: a narrower block is a different trajectory");

        /* Every method that jitters, at the narrowest block: the budget stays exact and no
         * point escapes the box or the repair. A blocked proposal copies the variables it does
         * not move, and copying the wrong ones is exactly how that invariant would break. */
        for (m = 0; cjitter_methods[m]; m++) {
            Watch wr = { lo8, hi8, 8, 0, 0, 0, 1, HUGE_VAL };
            cjitter_problem pr = { 8, lo8, hi8, watched_sphere, floor_first, &wr, NULL };
            cjitter_result rr = { 0, x, 0, 0, NULL, 0, 0, 0 };
            t = dflt; t.block = 1;
            if (cjitter_run_tuned(cjitter_methods[m], &pr, &b, &t, &rr) != 0 ||
                rr.evals != b.evals || wr.calls != b.evals || wr.outside || wr.unrepaired)
                break;
        }
        CHECK(cjitter_methods[m] == NULL,
              "block: at block 1 every method spends the budget exactly and holds box "
              "and repair");

        /* The cursor advances. Restarts are off, so after the first draw the only thing that
         * can move a variable is a blocked proposal: if all eight move, the blocks tiled the
         * whole vector, and no proposal changed more than two at once (a rejected proposal and
         * the next one differ in their own block and the one before it, never in more). */
        {
            Moves mv;
            cjitter_problem pm = { 8, lo8, hi8, moves_probe, NULL, &mv, NULL };
            cjitter_result rm = { 0, x, 0, 0, NULL, 0, 0, 0 };
            long j, all = 1;
            memset(&mv, 0, sizeof mv);
            mv.n = 8;
            t = dflt; t.block = 1; t.climb_restart_at = 0;
            CHECK(cjitter_run_tuned("climb", &pm, &b, &t, &rm) == 0, "block: the cycling run ran");
            for (j = 0; j < 8; j++) if (!mv.moved[j]) all = 0;
            CHECK(all, "block: the cursor cycles, so every variable is eventually moved");
            CHECK(mv.widest <= 2, "block: at block 1 no proposal moved more than one variable");
        }

        /* n = 5 with block 2 tiles as 2, 2, 1: the last variable is reachable only through the
         * short tail block, so its moving is the check that the tail is not dropped. */
        {
            static const double lo5[5] = { -5, -5, -5, -5, -5 };
            static const double hi5[5] = {  5,  5,  5,  5,  5 };
            Moves mv;
            cjitter_problem pm = { 5, lo5, hi5, moves_probe, NULL, &mv, NULL };
            cjitter_result rm = { 0, x, 0, 0, NULL, 0, 0, 0 };
            cjitter_tuning t5 = cjitter_tuning_default(5);
            memset(&mv, 0, sizeof mv);
            mv.n = 5;
            t5.block = 2; t5.climb_restart_at = 0;
            CHECK(cjitter_run_tuned("climb", &pm, &b, &t5, &rm) == 0 && rm.evals == b.evals &&
                  mv.moved[4] && mv.widest <= 4,
                  "block: a short last block is reached, so no variable is left behind");
        }
    }

    /* verify: the value the run delivered. The smallest value a run OBSERVES is the luckiest draw it
     * took, and how much luck that carries differs by method, so on a noisy objective a panel
     * of reported bests is not a fair comparison. verify re-evaluates the RETURNED point and
     * reports the mean. The checks: it is off by default and refused when negative; it costs
     * evaluations that do NOT come out of the search budget, so switching it on cannot shorten
     * a search; on a deterministic fitness it is exactly inert, which is what makes it safe to
     * leave on; and on a fitness whose observations differ from the point's worth it recovers
     * the difference, which is the whole point. */
    {
        static const double lo3[3] = { -2, -2, -2 }, hi3[3] = { 2, 2, 2 };
        Watch w = { lo3, hi3, 3, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 3, lo3, hi3, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 300, 5 };
        cjitter_tuning dflt = cjitter_tuning_default(3);
        cjitter_tuning t;
        double x[3];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };

        CHECK(dflt.verify == 0, "verify: off by default, so no existing result moves");
        t = dflt; t.verify = -1;
        w.calls = 0;
        CHECK(cjitter_run_tuned("climb", &p, &b, &t, &r) == -1 && w.calls == 0,
              "verify: a negative count is refused, before any evaluation");

        w.calls = 0;
        CHECK(cjitter_run_tuned("climb", &p, &b, &dflt, &r) == 0 &&
              r.verified == r.best && r.inflation == 0 && r.verify_evals == 0 &&
              w.calls == b.evals,
              "verify: at 0 the verified fields mirror best and nothing extra is spent");

        t = dflt; t.verify = 25;
        w.calls = 0;
        CHECK(cjitter_run_tuned("climb", &p, &b, &t, &r) == 0 &&
              r.verified == r.best && r.inflation == 0 && r.verify_evals == 25,
              "verify: on a deterministic fitness it is exactly inert");
        CHECK(r.evals == b.evals && w.calls == b.evals + 25,
              "verify: its evaluations are extra, so the search budget is untouched");

        t = dflt; t.verify = 1;
        CHECK(cjitter_run_tuned("climb", &p, &b, &t, &r) == 0 &&
              r.verified == r.best && r.inflation == 0 && r.verify_evals == 1,
              "verify: at one evaluation the inflation is exactly 0, not a rounding of it");

        /* The case it exists for: observations that differ from what the point is worth. */
        {
            Watch wa = { lo3, hi3, 3, 0, 0, 0, 0, HUGE_VAL };
            cjitter_problem pa = { 3, lo3, hi3, alternating, NULL, &wa, NULL };
            cjitter_result ra = { 0, x, 0, 0, NULL, 0, 0, 0 };
            t = dflt; t.verify = 40;
            CHECK(cjitter_run_tuned("climb", &pa, &b, &t, &ra) == 0 &&
                  ra.verify_evals == 40 && ra.inflation > 0.5 &&
                  ra.verified > ra.best,
                  "verify: it recovers the gap between the luckiest draw and the point's worth");
        }

        /* compare judges on the verified value when the caller paid for it, and prints that. */
        {
            FILE *tf = tmpfile();
            long saw_col = 0, saw_note = 0;
            if (tf) {
                char line[512];
                t = dflt; t.verify = 8;
                if (cjitter_compare_tuned(&p, &b, &t, 3, tf) == 0) {
                    rewind(tf);
                    while (fgets(line, sizeof line, tf))
                        if (strstr(line, "inflation")) saw_col++;
                    rewind(tf);
                    while (fgets(line, sizeof line, tf))
                        if (strstr(line, "fresh evaluations")) saw_note++;
                }
                fclose(tf);
            }
            CHECK(saw_col >= 1 && saw_note == 1,
                  "verify: compare reports the inflation column and what it judged on");
        }
    }

    /* NULL and "auto" are the default method, bit for bit the current climb; the alias is
     * an interface, so it gets its own checks. And the refusals that moved into the tuning
     * with jitter and pop in 0.11.0: a seed panel past 1000 (the exact tests' arithmetic
     * stops being exact), NaN jitter (it silently became the 0.1 default once), and a
     * population under two (one only re-scores a point, zero is no population at all). A bad
     * pop now refuses every method, the way a bad ga_mutate always did: a tuning is either
     * valid or it is not, whoever reads which field. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 200, 11 };
        cjitter_tuning tb;
        double x[2], y[2], z2[2];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 }, r2 = { 0, y, 0, 0, NULL, 0, 0, 0 };
        cjitter_result r3 = { 0, z2, 0, 0, NULL, 0, 0, 0 };
        CHECK(cjitter_run("climb", &p, &b, &r) == 0 &&
              cjitter_run("auto", &p, &b, &r2) == 0 &&
              cjitter_run(NULL, &p, &b, &r3) == 0 &&
              r.best == r2.best && r.best == r3.best &&
              memcmp(x, y, sizeof x) == 0 && memcmp(x, z2, sizeof x) == 0 &&
              strcmp(r2.method, "climb") == 0,
              "auto and NULL are the default method, bit for bit the current climb");
        CHECK(cjitter_compare(&p, &b, 1001, NULL) == -1,
              "compare refuses a seed panel past 1000");
        tb = cjitter_tuning_default(2); tb.jitter = 0.0 / 0.0;
        CHECK(cjitter_run_tuned("climb", &p, &b, &tb, &r) == -1,
              "tuning refuses a NaN jitter");
        tb = cjitter_tuning_default(2); tb.pop = 1;
        CHECK(cjitter_run_tuned("ga", &p, &b, &tb, &r) == -1,
              "tuning refuses a population of one");
        CHECK(cjitter_run_tuned("climb", &p, &b, &tb, &r) == -1,
              "and for climb too: a tuning is valid or it is not, whoever reads the field");
        tb = cjitter_tuning_default(2); tb.jitter = 0;
        CHECK(cjitter_run_tuned("climb", &p, &b, &tb, &r) == 0,
              "jitter 0 is a real ablation that runs: every proposal pinned to its parent");
    }

    /* Seed 0 is remapped inside run as it is in rng, so it works and reproduces. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 50, 0 };
        double x[2], y[2];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 }, r2 = { 0, y, 0, 0, NULL, 0, 0, 0 };
        CHECK(cjitter_run("random", &p, &b, &r) == 0 &&
              cjitter_run("random", &p, &b, &r2) == 0 && r.best == r2.best,
              "seed 0 runs and reproduces, not a degenerate stream");
    }

    /* The GA's population is clamped to the budget when larger than it, so a small budget
     * still spends exactly and never indexes past what was scored. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 10, 1 };
        cjitter_tuning tp = cjitter_tuning_default(2);
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
        tp.pop = 1000;
        CHECK(cjitter_run_tuned("ga", &p, &b, &tp, &r) == 0 && w.calls == 10,
              "ga: a population larger than the budget is clamped, budget still exact");
    }

    /* Climb on a 2-d sphere: with 5000 evaluations it should be at the optimum for any
     * practical purpose, and deterministically so. A regression pin on the whole climb path:
     * jitter, acceptance, the shrinking scale and the restart. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 5000, 1 };
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
        CHECK(cjitter_run("climb", &p, &b, &r) == 0 && r.best < 1e-6,
              "climb: reaches the sphere optimum in 5000 evaluations");
        CHECK(r.restarts > 0 && r.evals == 5000,
              "climb: the restart path was exercised and reported");
    }

    /* The regression that motivated the exactness checks: a restart on the last evaluation,
     * the one path that scored twice in an iteration and spent budget+1. Found by probing 2400
     * (method, seed, budget) triples against a build with the guard removed; seed 160 is the
     * witness under the 64-bit generator (200 was under the 32-bit one). The restart count is
     * pinned so the witness cannot go vacuous: any drift in the climb trajectory -- jitter,
     * patience, acceptance -- changes it, and a changed count means the witness must be
     * re-derived by probing again, not re-pinned to the new number. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 5000, 160 };
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
        CHECK(cjitter_run("climb", &p, &b, &r) == 0 && w.calls == 5000 && r.evals == 5000,
              "climb: a restart on the last evaluation does not spend 5001");
        CHECK(r.restarts == 7,
              "climb: seed 160's trajectory still reaches the witness (re-derive if this moves)");
    }

    /* cjitter_compare: refusals first, then the table itself, written to a tmpfile and read
     * back: the header, one row per method, the control named, and the same call twice giving
     * the same bytes. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 200, 1 };
        char buf[4096], buf2[4096];
        size_t got, got2;
        FILE *f;

        CHECK(cjitter_compare(NULL, &b, 3, NULL) == -1, "compare refuses a NULL problem");
        CHECK(cjitter_compare(&p, NULL, 3, NULL) == -1, "compare refuses a NULL budget");
        CHECK(cjitter_compare(&p, &b, 0, NULL) == -1, "compare refuses zero seeds");

        f = tmpfile();
        if (!f) { CHECK(0, "compare: tmpfile available"); return 1; }
        CHECK(cjitter_compare(&p, &b, 3, f) == 0, "compare runs to completion");
        rewind(f);
        got = fread(buf, 1, sizeof buf - 1, f);
        buf[got] = 0;
        fclose(f);
        CHECK(strstr(buf, "vs random") != NULL, "compare: prints the verdict column");
        CHECK(strstr(buf, "the control") != NULL, "compare: names random as the control");
        CHECK(strstr(buf, "\nclimb") && strstr(buf, "\nanneal") && strstr(buf, "\nga"),
              "compare: one row per method");
        CHECK(strstr(buf, "3 seeds at 200 evaluations") != NULL,
              "compare: states the seeds and the budget it used");

        f = tmpfile();
        if (!f) { CHECK(0, "compare: tmpfile available"); return 1; }
        cjitter_compare(&p, &b, 3, f);
        rewind(f);
        got2 = fread(buf2, 1, sizeof buf2 - 1, f);
        buf2[got2] = 0;
        fclose(f);
        CHECK(got == got2 && memcmp(buf, buf2, got) == 0,
              "compare: the same call gives the same bytes");
    }

    /* start: where a run begins when the caller has a point in mind. NULL is every release
     * before 0.13.0, a uniform draw. Otherwise climb and anneal score it first and the ga
     * carries it as member 0, bit for bit, and random ignores it, because a control handed
     * the answer is not a control. */
    {
        static const double lo3[3] = { -5, -5, -5 }, hi3[3] = { 5, 5, 5 };
        static const double s0[3] = { 1.5, -2.25, 0.75 };
        cjitter_budget b = { 100, 4 };
        double x[3];
        long m;
        int exact = 1, ignored = 0, atone = 1;

        for (m = 0; cjitter_methods[m]; m++) {
            First w;
            cjitter_problem p = { 3, lo3, hi3, first_probe, NULL, &w, s0 };
            cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
            memset(&w, 0, sizeof w);
            w.n = 3;
            if (cjitter_run(cjitter_methods[m], &p, &b, &r) != 0) exact = 0;
            if (m == 0) ignored = memcmp(w.first, s0, sizeof s0) != 0;
            else if (memcmp(w.first, s0, sizeof s0) != 0) exact = 0;
        }
        CHECK(exact, "start: climb, anneal and the ga score it first, bit for bit");
        CHECK(ignored, "start: random ignores it and draws uniformly, as a control must");

        /* At a budget of one there is nothing but the start, so best is its fitness exactly
         * and the point returned is the point handed in. */
        for (m = 1; cjitter_methods[m]; m++) {
            First w, w2;
            cjitter_problem p = { 3, lo3, hi3, first_probe, NULL, &w, s0 };
            cjitter_budget b1 = { 1, 4 };
            cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
            double want;
            memset(&w, 0, sizeof w); memset(&w2, 0, sizeof w2);
            w.n = 3; w2.n = 3;
            want = first_probe(s0, &w2);
            if (cjitter_run(cjitter_methods[m], &p, &b1, &r) != 0 ||
                r.best != want || memcmp(x, s0, sizeof s0) != 0) atone = 0;
        }
        CHECK(atone, "start: at a budget of one, best is its fitness and x is the start point");

        /* It is not exempt from the box: a start outside is brought in like any other point. */
        {
            static const double out3[3] = { 99, -99, 0.5 };
            First w;
            cjitter_problem p = { 3, lo3, hi3, first_probe, NULL, &w, out3 };
            cjitter_budget b1 = { 1, 4 };
            cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
            memset(&w, 0, sizeof w);
            w.n = 3;
            CHECK(cjitter_run("climb", &p, &b1, &r) == 0 &&
                  w.first[0] == hi3[0] && w.first[1] == lo3[1] && w.first[2] == 0.5,
                  "start: one outside the box is brought in, like every other point");
        }
    }

    /* The box under a repair that projects rather than clamps. Every point the search scores
     * and the point it returns must satisfy both the box and the disc; a repair that
     * overshoots is exactly how a scored point leaves the box, and nothing else here checks
     * the box while a repair is moving points. */
    {
        static const double lo3[3] = { -2, -2, -2 }, hi3[3] = { 2, 2, 2 };
        cjitter_budget b = { 500, 6 };
        double x[3];
        int held = 1;
        long m;
        for (m = 0; m < 2; m++) {
            Disc d;
            cjitter_problem p = { 3, lo3, hi3, disc_probe, disc_repair, &d, NULL };
            cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
            memset(&d, 0, sizeof d);
            d.lo = lo3; d.hi = hi3; d.n = 3;
            if (cjitter_run(m == 0 ? "random" : "climb", &p, &b, &r) != 0) held = 0;
            disc_probe(x, &d);      /* the returned point, watched like the rest */
            if (d.calls != b.evals + 1 || d.outside || d.offdisc) held = 0;
        }
        CHECK(held, "box: under a disc repair every scored point and the returned point hold "
                    "the box and the disc, for random and climb");
    }

    /* jitter 0: the first move size is zero, so every proposal is its parent, nothing ever
     * moves and the run reports the fitness of its first point. The ablation check above sees
     * only that such a run is accepted; this one looks at the trajectory. */
    {
        static const double lo8[8] = { -5, -5, -5, -5, -5, -5, -5, -5 };
        static const double hi8[8] = {  5,  5,  5,  5,  5,  5,  5,  5 };
        cjitter_budget b = { 200, 12 };
        cjitter_tuning t = cjitter_tuning_default(8);
        double x[8];
        int pinned = 1;
        long m, j;
        t.jitter = 0;
        for (m = 1; m <= 2; m++) {          /* climb and anneal: the two that jitter a parent */
            Moves mv;
            cjitter_problem p = { 8, lo8, hi8, moves_probe, NULL, &mv, NULL };
            cjitter_result r = { 0, x, 0, 0, NULL, 0, 0, 0 };
            double f0 = 0;
            memset(&mv, 0, sizeof mv);
            mv.n = 8;
            if (cjitter_run_tuned(cjitter_methods[m], &p, &b, &t, &r) != 0) pinned = 0;
            for (j = 0; j < 8; j++) {
                if (mv.moved[j]) pinned = 0;
                f0 += mv.first[j] * mv.first[j];
            }
            if (mv.widest != 0 || mv.calls != b.evals || r.restarts != 0) pinned = 0;
            if (r.best != f0 || memcmp(x, mv.first, sizeof x) != 0) pinned = 0;
        }
        CHECK(pinned, "jitter 0: every proposal is the parent, so nothing moves and best is "
                      "the first point's fitness");
    }

    /* The two statistics the verdict column is made of, against values computable by hand:
     * seven wins in seven is one coin sequence in 128, no wins at all is certain, and Holm
     * multiplies the smallest of three probabilities by three and the next by two, holding
     * each at the largest so far. */
    {
        static const double pin[3] = { 0.01, 0.02, 0.04 };
        double adj[3];
        CHECK(cjitter_sign_p(7, 7) == 1.0 / 128.0, "sign_p: seven wins in seven is 1/128");
        CHECK(cjitter_sign_p(0, 5) == 1.0, "sign_p: no wins in five is certain");
        CHECK(cjitter_holm(pin, 3, adj) == 0 &&
              adj[0] == 0.03 && adj[1] == 0.04 && adj[2] == 0.04,
              "holm: (0.01, 0.02, 0.04) corrects to (0.03, 0.04, 0.04)");
        CHECK(cjitter_holm(pin, 0, adj) == -1 && cjitter_holm(NULL, 3, adj) == -1,
              "holm: an empty family and a NULL vector are refused");
    }

    /* sign_p on pooled panels, where the plain binomial sum cannot go: C(n, k) overflows a
     * double past n = 1028 and 2^-n underflows past n = 1074. References are the exact
     * rational sums, computed outside and rounded to the nearest double; the scaled sum
     * rounds once per recurrence step, so the pin allows a relative 1e-12. */
    {
        double p650 = cjitter_sign_p(650, 1200);
        double p551 = cjitter_sign_p(551, 1200);
        double lo = 1.0, ref;
        int i;
        CHECK(fabs(p650 - 2.12282007733476412e-3) <= 1e-12 * 2.12282007733476412e-3,
              "sign_p: 650 of 1200 matches the exact rational sum");
        CHECK(fabs(cjitter_sign_p(700, 1200) - 4.26768304051716716e-9) <= 1e-12 * 4.3e-9,
              "sign_p: 700 of 1200 matches the exact rational sum");
        CHECK(fabs(p650 + p551 - 1.0) <= 1e-12,
              "sign_p: at least 650 heads and at most 649 are complementary");
        CHECK(cjitter_sign_p(651, 1200) < p650,
              "sign_p: one more required win can only lower the tail");
        for (ref = 1.0, i = 0; i < 1029; i++) ref *= 0.5;
        CHECK(cjitter_sign_p(1029, 1029) == ref,
              "sign_p: a clean sweep of 1029 is exactly 2^-1029, in the subnormal range");
        CHECK(cjitter_sign_p(2000, 2000) == 0.0,
              "sign_p: a tail below the smallest double is 0, not NaN");
        for (i = 0; i < 1200; i++) lo *= 0.5;
        CHECK(cjitter_sign_p(1200, 1200) == 0.0 && lo == 0.0,
              "sign_p: 2^-1200 itself is below the smallest double, so 0 is the rounding");
    }

    /* compare_raw: the panel's numbers without the table. Every row must be the run it stands
     * for, seed for seed and point for point, or a study computing its own estimand from
     * these is not looking at the runs compare judged. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 60, 5 };
        double sc[4 * 3], xs[4 * 3 * 2], infl[4 * 3];
        long m, s;
        int paired = 1;

        for (m = 0; m < 12; m++) { sc[m] = -1.0; infl[m] = -1.0; }
        CHECK(cjitter_compare_raw(&p, &b, NULL, 3, 0u, sc, xs, infl) == 0 &&
              w.calls == 4 * 3 * 60,
              "compare_raw: runs the panel, one budget per method per seed");
        for (m = 0; m < 4; m++)
            for (s = 0; s < 3; s++) {
                cjitter_budget bb = b;
                double y[2];
                cjitter_result r = { 0, y, 0, 0, NULL, 0, 0, 0 };
                bb.seed = 5u + 7919u * (uint32_t)s;   /* compare's own panel stride */
                if (cjitter_run(cjitter_methods[m], &p, &bb, &r) != 0 ||
                    sc[m * 3 + s] != r.best || infl[m * 3 + s] != 0 ||
                    memcmp(xs + (m * 3 + s) * 2, y, sizeof y) != 0) paired = 0;
            }
        CHECK(paired, "compare_raw: every row is the run it stands for, point and all");

        for (m = 0; m < 12; m++) sc[m] = -1.0;
        w.calls = 0;
        CHECK(cjitter_compare_raw(&p, &b, NULL, 3, CJITTER_M_RANDOM | CJITTER_M_CLIMB,
                                  sc, NULL, NULL) == 0 && w.calls == 2 * 3 * 60 &&
              sc[0] != -1.0 && sc[3] != -1.0 && sc[6] == -1.0 && sc[9] == -1.0,
              "compare_raw: a mask runs and writes only the methods it names");
        CHECK(cjitter_compare_raw(&p, &b, NULL, 3, 16u, sc, NULL, NULL) == -1 &&
              cjitter_compare_raw(&p, &b, NULL, 3, 0u, NULL, NULL, NULL) == -1,
              "compare_raw: a flag it does not have, and a NULL score, are refused");
    }

    /* The mask on the printed table. The full mask must be the bytes the unmasked call gives,
     * or every number pinned anywhere moved; a partial one prints only what it ran and
     * corrects over that family alone; and a panel without the control is refused, every
     * verdict here being against it. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w, NULL };
        cjitter_budget b = { 200, 1 };
        char buf[4096], buf2[4096];
        size_t got = 0, got2 = 0;
        FILE *f = tmpfile(), *g = tmpfile();

        if (!f || !g) { CHECK(0, "compare_masked: tmpfile available"); return 1; }
        cjitter_compare_tuned(&p, &b, NULL, 3, f);
        cjitter_compare_masked(&p, &b, NULL, 3, CJITTER_M_ALL, g);
        rewind(f); rewind(g);
        got = fread(buf, 1, sizeof buf - 1, f);
        got2 = fread(buf2, 1, sizeof buf2 - 1, g);
        buf[got] = 0; buf2[got2] = 0;
        fclose(f); fclose(g);
        CHECK(got == got2 && memcmp(buf, buf2, got) == 0,
              "compare_masked: all four is the table cjitter_compare_tuned prints, byte for byte");

        CHECK(cjitter_compare_masked(&p, &b, NULL, 3, CJITTER_M_CLIMB, NULL) == -1 &&
              cjitter_compare_masked(&p, &b, NULL, 3, 16u, NULL) == -1,
              "compare_masked: a panel without the control, and an unknown flag, are refused");

        f = tmpfile();
        if (!f) { CHECK(0, "compare_masked: tmpfile available"); return 1; }
        w.calls = 0;
        CHECK(cjitter_compare_masked(&p, &b, NULL, 3,
                                     CJITTER_M_RANDOM | CJITTER_M_CLIMB, f) == 0 &&
              w.calls == 2 * 3 * 200,
              "compare_masked: two methods cost two budgets a seed, not four");
        rewind(f);
        got = fread(buf, 1, sizeof buf - 1, f);
        buf[got] = 0;
        fclose(f);
        CHECK(strstr(buf, "\nclimb") && !strstr(buf, "\nanneal") && !strstr(buf, "\nga"),
              "compare_masked: it prints the rows it ran and no others");
        {   /* One comparison in the family, so Holm multiplies by one and the two columns
             * agree; at four methods they do not. */
            char line[512];
            double med = 0, range = 0, sp = -1, hp = -2;
            long wins = 0, n = 0;
            int read = 0;
            char *at = strstr(buf, "\nclimb");
            if (at) {
                size_t len = strcspn(at + 1, "\n");
                if (len < sizeof line) {
                    memcpy(line, at + 1, len);
                    line[len] = 0;
                    read = sscanf(line + 5, "%lf %lf %ld/%ld %lf %lf",
                                  &med, &range, &wins, &n, &sp, &hp);
                }
            }
            CHECK(read == 6 && sp == hp,
                  "compare_masked: a family of one is corrected by one, holm equal to sign-p");
        }
    }

    printf("\n%d checks, %d failed\n", t_run, t_fail);
    return t_fail ? 1 : 0;
}
