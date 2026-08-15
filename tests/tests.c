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

int main(void)
{
    static const double lo2[2] = { -5, -5 }, hi2[2] = { 5, 5 };

    /* rng: reproducible, in range, and 0-seed remapped (not stuck at zero) */
    {
        Rng g, h, z;
        double u;
        rng_seed(&g, 42);
        rng_seed(&h, 42);
        CHECK(rng_u32(&g) == rng_u32(&h), "rng: same seed gives the same sequence");
        u = rng_uniform(&g, -1.0, 1.0);
        CHECK(u >= -1.0 && u < 1.0, "rng: uniform stays in [lo, hi)");
        rng_seed(&z, 0);
        CHECK(rng_u32(&z) != 0, "rng: seed 0 is remapped, not degenerate");
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
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w };
        cjitter_budget b = { 100, 1, 0.1, 0 };
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL };
        cjitter_problem bad;
        cjitter_budget bb;

        CHECK(cjitter_run(NULL, &p, &b, &r) == -1, "run refuses a NULL method");
        CHECK(cjitter_run("climb", NULL, &b, &r) == -1, "run refuses a NULL problem");
        CHECK(cjitter_run("climb", &p, NULL, &r) == -1, "run refuses a NULL budget");
        CHECK(cjitter_run("climb", &p, &b, NULL) == -1, "run refuses a NULL result");
        CHECK(cjitter_run("simplex", &p, &b, &r) == -1, "run refuses a method it does not have");
        bad = p; bad.fitness = NULL;
        CHECK(cjitter_run("climb", &bad, &b, &r) == -1, "run refuses a NULL fitness");
        bad = p; bad.n = 0;
        CHECK(cjitter_run("climb", &bad, &b, &r) == -1, "run refuses n < 1");
        bb = b; bb.evals = 0;
        CHECK(cjitter_run("climb", &p, &bb, &r) == -1, "run refuses a budget of no evaluations");
        CHECK(w.calls == 0, "and no refusal called the fitness even once");
    }

    /* Each method, at budgets including 1 and an odd number: the budget is spent exactly, the
     * result is the minimum of what was scored, every point obeyed the box, and the same seed
     * reproduces the same answer bit for bit. This is the library's whole promise. */
    {
        static const long budgets[3] = { 1, 37, 300 };
        long m, bi;
        for (m = 0; cjitter_methods[m]; m++) {
            const char *name = cjitter_methods[m];
            int exact = 1, isbest = 1, inbox = 1, again = 1;
            char msg[96];
            for (bi = 0; bi < 3; bi++) {
                Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
                cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w };
                cjitter_budget b = { 0, 7, 0.1, 0 };
                double x[2], y[2];
                cjitter_result r = { 0, x, 0, 0, NULL }, r2 = { 0, y, 0, 0, NULL };
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
            sprintf(msg, "%s: spends the budget exactly, at 1, 37 and 300", name);
            CHECK(exact, msg);
            sprintf(msg, "%s: best is the minimum scored, and re-scoring it agrees", name);
            CHECK(isbest, msg);
            sprintf(msg, "%s: never scores a point outside the box", name);
            CHECK(inbox, msg);
            sprintf(msg, "%s: the same seed reproduces best and point bit for bit", name);
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
            cjitter_problem p = { 2, lo2, hi2, watched_sphere, floor_first, &w };
            cjitter_budget b = { 400, 3, 0.2, 0 };
            double x[2];
            cjitter_result r = { 0, x, 0, 0, NULL };
            if (cjitter_run(cjitter_methods[m], &p, &b, &r) != 0 || w.unrepaired) held = 0;
            if (x[0] < 0.5) returned = 0;
        }
        CHECK(held, "repair: every scored point satisfied the constraint, all four methods");
        CHECK(returned, "repair: every returned best satisfies it too");
    }

    /* Seed 0 is remapped inside run as it is in rng, so it works and reproduces. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w };
        cjitter_budget b = { 50, 0, 0.1, 0 };
        double x[2], y[2];
        cjitter_result r = { 0, x, 0, 0, NULL }, r2 = { 0, y, 0, 0, NULL };
        CHECK(cjitter_run("random", &p, &b, &r) == 0 &&
              cjitter_run("random", &p, &b, &r2) == 0 && r.best == r2.best,
              "seed 0 runs and reproduces, not a degenerate stream");
    }

    /* The GA's population defaults when 0 and is clamped to the budget when larger than it,
     * so a small budget still spends exactly and never indexes past what was scored. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w };
        cjitter_budget b = { 10, 1, 0.1, 1000 };
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL };
        CHECK(cjitter_run("ga", &p, &b, &r) == 0 && w.calls == 10,
              "ga: a population larger than the budget is clamped, budget still exact");
    }

    /* Climb on a 2-d sphere: with 5000 evaluations it should be at the optimum for any
     * practical purpose, and deterministically so. A regression pin on the whole climb path:
     * jitter, acceptance, the shrinking scale and the restart. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w };
        cjitter_budget b = { 5000, 1, 0.1, 0 };
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL };
        CHECK(cjitter_run("climb", &p, &b, &r) == 0 && r.best < 1e-6,
              "climb: reaches the sphere optimum in 5000 evaluations");
        CHECK(r.restarts > 0 && r.evals == 5000,
              "climb: the restart path was exercised and reported");
    }

    /* The regression that motivated the exactness checks: a restart on the last evaluation,
     * the one path that scored twice in an iteration and spent budget+1. Found by probing 2400
     * (method, seed, budget) triples against a build with the guard removed; seed 200 is the
     * witness under the libm-free gauss (seed 157 was, under Box-Muller). The restart count is
     * pinned so the witness cannot go vacuous: any drift in the climb trajectory -- jitter,
     * patience, acceptance -- changes it, and a changed count means the witness must be
     * re-derived by probing again, not re-pinned to the new number. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w };
        cjitter_budget b = { 5000, 200, 0.1, 0 };
        double x[2];
        cjitter_result r = { 0, x, 0, 0, NULL };
        CHECK(cjitter_run("climb", &p, &b, &r) == 0 && w.calls == 5000 && r.evals == 5000,
              "climb: a restart on the last evaluation does not spend 5001");
        CHECK(r.restarts == 6,
              "climb: seed 200's trajectory still reaches the witness (re-derive if this moves)");
    }

    /* cjitter_compare: refusals first, then the table itself, written to a tmpfile and read
     * back: the header, one row per method, the control named, and the same call twice giving
     * the same bytes. */
    {
        Watch w = { lo2, hi2, 2, 0, 0, 0, 0, HUGE_VAL };
        cjitter_problem p = { 2, lo2, hi2, watched_sphere, NULL, &w };
        cjitter_budget b = { 200, 1, 0.1, 0 };
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

    printf("\n%d checks, %d failed\n", t_run, t_fail);
    return t_fail ? 1 : 0;
}
