/* station.c -- which aesthetic criteria hold a hand-drawn diagram layout?
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * A layout tool minimises a weighted sum of criteria. If people drew that way, a layout a
 * person accepted would be a local minimum of every criterion that carries weight. This
 * program asks, per criterion and per layout, whether it is.
 *
 * The directional test. For each node, try DIRS moves of length D (equally spaced angles),
 * every other node fixed. The node is HELD if none of the moves lowers the energy by more
 * than 1e-12 of its value; ties hold, since a move that changes nothing is not an
 * improvement, and the tolerance keeps a last-bit difference in summation from counting as
 * one. q is the fraction of nodes held: 1 means the layout is a local minimum of the energy
 * over single-node moves of length D, 0 means every node has somewhere better to be. It
 * needs no gradient, so it is defined for the crossing count and the overlap corners, and it
 * needs no seed, so it reproduces exactly.
 *
 *     station direct --corpus F --weights C,O,L,S,R,A,N,F [--d 0.02] [--dirs 16]
 *                    [--align a1|a2|a3|grid] [--L fit|median|rsqrt] [--s 0.02] [--tol 0.005]
 *                    [--nodes | --diffs]
 *     station terms  --corpus F [--align ...] [--L ...] [--s ...] [--tol ...]
 *     station check  --corpus F --against G
 *     station descend --corpus F --weights ... [--d 0.02] [--budget 4000] [--seeds 15]
 *                    [--jitter 0.25] [--patience 200] [--converge 0] [--seed 1]
 *
 * direct prints one CSV row per graph: id, n, m, E (the energy at the layout), q, dec (the
 * mean over nodes of the best decrease found, over E; 0 when held), and the eight term
 * values at the layout. With --nodes it prints one row per node instead: id, node, the best
 * direction's index (-1 if held) and its decrease. With --diffs it prints one row per node
 * and direction: id, node, dir, and the change of each of the eight terms under that move,
 * which is what the weight fit reads: node i is held under weights w exactly when
 * sum_k w_k diff_k >= 0 in every direction.
 *
 * terms prints id, n, m, L (the length term's reference), Ls (the stress term's), ux, uy
 * (the flow term's reading direction) and the eight term values at the layout, which is how
 * the tests pin the formulas and how the paper's term-share line is made.
 *
 * check reads two corpora and exits 0 when they are the same graphs in the same order with
 * the same edges, directions and raw box sizes, which is what a tool control must be, and 2
 * naming the first difference otherwise. Waypoints are not compared: a control has none.
 *
 * descend is the secondary estimand, through the library. Per graph, hill climbing starts
 * from the layout, one node per proposal (block 2), every node kept within the disc of radius
 * d about where it started (the repair), first step jitter x 2d per coordinate, 200
 * rejections before the step halves (--patience; at the default N of the library the
 * uncapped climb below stops short, q 0.88 where 200 gives 1.00), for BUDGET evaluations
 * and SEEDS seeds;
 * uniform sampling in the same cap at the same budget on the same seeds is the control. Per
 * graph it prints id, n, m, E, q, then the mean over seeds of the energy the climber ends at
 * and the control's, the fraction of the cap used (mean over seeds and nodes of the distance
 * moved over d), the fraction of energy removed, the number of seeds on which the climber
 * ends lower than the control, and the exact sign p of that count. With --converge B an
 * uncapped climb of B evaluations from the layout, one seed, adds the energy it ends at and
 * q there: the converged reference, which must be near 1 for the test to have power. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "energy.h"
#include "corpus.h"
#include "cjitter.h"

static void usage(void)
{
    fprintf(stderr,
        "usage: station direct --corpus FILE --weights C,O,L,S,R,A,N,F [--d 0.02] [--dirs 16]\n"
        "                      [--align a1|a2|a3|grid] [--L fit|median|rsqrt] [--s 0.02]\n"
        "                      [--tol 0.005] [--nodes | --diffs]\n"
        "       station terms  --corpus FILE [--align ...] [--L ...] [--s ...] [--tol ...]\n"
        "       station check  --corpus FILE --against FILE\n"
        "       station descend --corpus FILE --weights ... [--d 0.02] [--budget 4000] [--seeds 15]\n"
        "                       [--jitter 0.25] [--patience 200] [--converge 0] [--seed 1]\n");
    exit(2);
}

static double arg_double(const char *name, const char *v, double lo, double hi)
{
    char *end;
    double d = strtod(v, &end);
    if (*end || !(d >= lo) || !(d <= hi)) {
        fprintf(stderr, "station: %s must be a number in [%g, %g], not '%s'\n", name, lo, hi, v);
        exit(2);
    }
    return d;
}

static long arg_long(const char *name, const char *v, long lo, long hi)
{
    char *end;
    long l = strtol(v, &end, 10);
    if (*end || l < lo || l > hi) {
        fprintf(stderr, "station: %s must be an integer in [%ld, %ld], not '%s'\n", name, lo, hi, v);
        exit(2);
    }
    return l;
}

static void parse_weights(const char *v, double w[NTERMS])
{
    int k = 0;
    const char *p = v;
    for (;;) {
        char *end;
        double d = strtod(p, &end);
        if (end == p || !(d >= 0) || k >= NTERMS) break;
        w[k++] = d;
        if (*end == 0) { if (k == NTERMS) return; break; }
        if (*end != ',') break;
        p = end + 1;
    }
    fprintf(stderr, "station: --weights needs eight non-negative numbers C,O,L,S,R,A,N,F, not '%s'\n", v);
    exit(2);
}

/* Unit direction k of DIRS, by a Taylor series so the table is the same on every platform.
 * When DIRS is a multiple of four the angle is reduced to the first quadrant and rotated
 * back, so the four axis directions are exact and opposite directions are exact negatives;
 * a series evaluated at pi/2 gives a cosine of 1e-17, and a vertical move that also shifts
 * x by 1e-19 can lower a neighbour's alignment distance by that much. */
static void direction(long k, long dirs, double *cx, double *cy)
{
    long quad = 0;
    double a, c = 1, s, t = 1;
    int i;
    if (dirs % 4 == 0) { quad = k / (dirs / 4); k %= dirs / 4; }
    a = 6.283185307179586 * (double)k / (double)dirs;
    s = a;
    for (i = 1; i <= 12; i++) {
        t *= -a * a / (double)((2 * i - 1) * (2 * i));
        c += t;
        s += t * a / (double)(2 * i + 1);
    }
    switch (quad) {
    case 1:  *cx = -s; *cy = c;  break;
    case 2:  *cx = -c; *cy = -s; break;
    case 3:  *cx = s;  *cy = -c; break;
    default: *cx = c;  *cy = s;  break;
    }
}

static graph *read_corpus(const char *path, long *k)
{
    graph *gs;
    char err[256];
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "station: cannot open %s\n", path); exit(2); }
    *k = corpus_read(f, &gs, err, sizeof err);
    fclose(f);
    if (*k < 0) { fprintf(stderr, "station: %s: %s\n", path, err); exit(2); }
    if (*k == 0) { fprintf(stderr, "station: %s: no graphs\n", path); exit(2); }
    return gs;
}

/* The first difference between two corpora, as a message, or NULL. Raw box sizes agree to
 * a part in 1e-6, the precision the corpus files carry. */
static const char *differ(const graph *a, const graph *b)
{
    static char msg[512];
    long i;
    if (strcmp(a->id, b->id)) { snprintf(msg, sizeof msg, "id %s against %s", a->id, b->id); return msg; }
    if (a->n != b->n || a->m != b->m)
        { snprintf(msg, sizeof msg, "%s: %ld nodes %ld edges against %ld and %ld", a->id, a->n, a->m, b->n, b->m); return msg; }
    for (i = 0; i < a->m; i++)
        if (a->ea[i] != b->ea[i] || a->eb[i] != b->eb[i] || a->dir[i] != b->dir[i])
            { snprintf(msg, sizeof msg, "%s: edge %ld is %ld-%ld (%d) against %ld-%ld (%d)", a->id, i,
                       a->ea[i], a->eb[i], a->dir[i], b->ea[i], b->eb[i], b->dir[i]); return msg; }
    for (i = 0; i < a->n; i++) {
        double aw = a->w[i] * a->scale, ah = a->h[i] * a->scale, bw = b->w[i] * b->scale, bh = b->h[i] * b->scale;
        if (fabs(aw - bw) > 1e-6 * (1 + fabs(aw)) || fabs(ah - bh) > 1e-6 * (1 + fabs(ah)))
            { snprintf(msg, sizeof msg, "%s: node %ld is %g x %g against %g x %g", a->id, i, aw, ah, bw, bh); return msg; }
    }
    return NULL;
}

/* The directional test on layout X: the fraction of nodes held, and the mean best decrease
 * over E(X) in *DEC. Y is scratch for 2n doubles. With DIFFS the per-move term changes are
 * printed, with NODES the per-node verdicts. */
static double direct_q(const graph *g, const double *x, const energy_spec *e, double d,
                       long dirs, const double *dx, const double *dy, double *y,
                       double *dec, int diffs, int nodes)
{
    double t[NTERMS], e0 = energy(g, x, e, t), sum = 0;
    long held = 0, node, dir, i;
    memcpy(y, x, (size_t)(2 * g->n) * sizeof *y);
    for (node = 0; node < g->n; node++) {
        double best = 1e-12 * e0;
        long bdir = -1;
        for (dir = 0; dir < dirs; dir++) {
            double e1, t1[NTERMS];
            y[2 * node] = x[2 * node] + d * dx[dir];
            y[2 * node + 1] = x[2 * node + 1] + d * dy[dir];
            e1 = energy(g, y, e, diffs ? t1 : NULL);
            if (diffs) {
                printf("%s,%ld,%ld", g->id, node, dir);
                for (i = 0; i < NTERMS; i++) printf(",%.9g", t1[i] - t[i]);
                printf("\n");
            }
            if (e0 - e1 > best) { best = e0 - e1; bdir = dir; }
        }
        y[2 * node] = x[2 * node];
        y[2 * node + 1] = x[2 * node + 1];
        if (bdir < 0) held++; else sum += best;
        if (nodes) printf("%s,%ld,%ld,%.9f\n", g->id, node, bdir, bdir < 0 ? 0 : best);
    }
    *dec = e0 > 0 ? sum / (double)g->n / e0 : 0;
    return (double)held / (double)g->n;
}

/* The climber's problem: the layout as 2n variables, the energy as fitness, the cap as a
 * disc of radius d about the start, enforced by the repair. */
typedef struct { const graph *g; const energy_spec *e; const double *x0; double d; } cap_ctx;

static double cap_fitness(const double *x, void *ctx)
{
    const cap_ctx *c = ctx;
    return energy(c->g, x, c->e, NULL);
}

static void cap_repair(double *x, void *ctx)
{
    const cap_ctx *c = ctx;
    long i;
    for (i = 0; i < c->g->n; i++) {
        double ux = x[2 * i] - c->x0[2 * i], uy = x[2 * i + 1] - c->x0[2 * i + 1];
        double r = sqrt(ux * ux + uy * uy);
        if (r > c->d) { x[2 * i] = c->x0[2 * i] + ux * c->d / r; x[2 * i + 1] = c->x0[2 * i + 1] + uy * c->d / r; }
    }
}

/* Mean over nodes of the distance from X0 to X, over d. */
static double cap_used(const graph *g, const double *x0, const double *x, double d)
{
    long i;
    double s = 0;
    for (i = 0; i < g->n; i++) {
        double ux = x[2 * i] - x0[2 * i], uy = x[2 * i + 1] - x0[2 * i + 1];
        s += sqrt(ux * ux + uy * uy) / d;
    }
    return s / (double)g->n;
}

static int descend(const graph *gs, long k, const energy_spec *e, double d, long dirs,
                   const double *dx, const double *dy, long budget, long seeds, double jitter,
                   long patience, long converge, uint32_t seed)
{
    long gi, i, s;
    printf("id,n,m,E,q,climb,random,cap_climb,cap_random,rho_climb,rho_random,wins,p");
    if (converge) printf(",converged,q_converged");
    printf("\n");
    for (gi = 0; gi < k; gi++) {
        const graph *g = &gs[gi];
        long n2 = 2 * g->n;
        double *lo = malloc((size_t)n2 * sizeof *lo), *hi = malloc((size_t)n2 * sizeof *hi);
        double *y = malloc((size_t)n2 * sizeof *y);
        double *score = malloc((size_t)(4 * seeds) * sizeof *score);
        double *pts = malloc((size_t)(4 * seeds * n2) * sizeof *pts);
        double e0, q, dec, ec = 0, er = 0, cc = 0, cr = 0;
        long wins = 0;
        cap_ctx ctx;
        cjitter_problem p;
        cjitter_budget b;
        cjitter_tuning t = cjitter_tuning_default(n2);
        if (!lo || !hi || !y || !score || !pts) { fprintf(stderr, "station: out of memory\n"); return 2; }
        for (i = 0; i < n2; i++) { lo[i] = g->x[i] - d; hi[i] = g->x[i] + d; }
        ctx.g = g; ctx.e = e; ctx.x0 = g->x; ctx.d = d;
        memset(&p, 0, sizeof p);
        p.n = n2; p.lo = lo; p.hi = hi; p.fitness = cap_fitness; p.repair = cap_repair;
        p.ctx = &ctx; p.start = g->x;
        b.evals = budget; b.seed = seed + (uint32_t)gi * 1000003u;
        t.block = 2; t.jitter = jitter; t.climb_patience = patience;
        t.climb_restart_at = 0; t.verify = 0;
        if (cjitter_compare_raw(&p, &b, &t, seeds, CJITTER_M_CLIMB | CJITTER_M_RANDOM, score, pts, NULL))
            { fprintf(stderr, "station: the library refused the problem\n"); return 2; }
        e0 = energy(g, g->x, e, NULL);
        q = direct_q(g, g->x, e, d, dirs, dx, dy, y, &dec, 0, 0);
        for (s = 0; s < seeds; s++) {
            /* cjitter_methods order: random 0, climb 1 */
            double sr = score[0 * seeds + s], sc = score[1 * seeds + s];
            er += sr; ec += sc;
            cr += cap_used(g, g->x, pts + (0 * seeds + s) * n2, d);
            cc += cap_used(g, g->x, pts + (1 * seeds + s) * n2, d);
            if (sc < sr) wins++;
        }
        ec /= (double)seeds; er /= (double)seeds; cc /= (double)seeds; cr /= (double)seeds;
        printf("%s,%ld,%ld,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f,%.4f,%ld,%.4g", g->id, g->n, g->m, e0, q,
               ec, er, cc, cr, e0 > 0 ? (e0 - ec) / e0 : 0, e0 > 0 ? (e0 - er) / e0 : 0,
               wins, cjitter_sign_p(wins, seeds));
        if (converge) {
            cjitter_result r = { 0 };
            double qc, decc, *xc = malloc((size_t)n2 * sizeof *xc);
            if (!xc) { fprintf(stderr, "station: out of memory\n"); return 2; }
            for (i = 0; i < n2; i++) { lo[i] = -0.5; hi[i] = 1.5; }
            p.repair = NULL;
            b.evals = converge;
            t.jitter = 0.02;
            r.x = xc;
            if (cjitter_run_tuned("climb", &p, &b, &t, &r))
                { fprintf(stderr, "station: the library refused the problem\n"); return 2; }
            qc = direct_q(g, xc, e, d, dirs, dx, dy, y, &decc, 0, 0);
            printf(",%.6f,%.6f", r.best, qc);
            free(xc);
        }
        printf("\n");
        free(lo); free(hi); free(y); free(score); free(pts);
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode, *corpus = NULL, *weights = NULL, *against = NULL;
    energy_spec e;
    double d = 0.02, *dx = NULL, *dy = NULL;
    long dirs = 16, k, gi, nodes = 0, diffs = 0, i, budget = 4000, seeds = 15, patience = 200, converge = 0;
    double jitter = 0.25;
    uint32_t seed = 1;
    int a, have_weights = 0;
    graph *gs;

    if (argc < 2) usage();
    mode = argv[1];
    if (strcmp(mode, "direct") && strcmp(mode, "terms") && strcmp(mode, "check") && strcmp(mode, "descend")) usage();
    memset(&e, 0, sizeof e);
    e.align = ALIGN_A3; e.lref = L_FIT; e.s = 0.02; e.tol = 0.005;
    for (a = 2; a < argc; a++) {
        const char *o = argv[a], *v = a + 1 < argc ? argv[a + 1] : NULL;
        if (!strcmp(o, "--nodes")) { nodes = 1; continue; }
        if (!strcmp(o, "--diffs")) { diffs = 1; continue; }
        if (!v) usage();
        if (!strcmp(o, "--corpus")) corpus = v;
        else if (!strcmp(o, "--against")) against = v;
        else if (!strcmp(o, "--weights")) { weights = v; have_weights = 1; }
        else if (!strcmp(o, "--d")) d = arg_double("--d", v, 1e-9, 1);
        else if (!strcmp(o, "--dirs")) dirs = arg_long("--dirs", v, 1, 3600);
        else if (!strcmp(o, "--s")) e.s = arg_double("--s", v, 1e-9, 1);
        else if (!strcmp(o, "--tol")) e.tol = arg_double("--tol", v, 0, 1);
        else if (!strcmp(o, "--budget")) budget = arg_long("--budget", v, 1, 100000000L);
        else if (!strcmp(o, "--seeds")) seeds = arg_long("--seeds", v, 1, 1000);
        else if (!strcmp(o, "--jitter")) jitter = arg_double("--jitter", v, 0, 10);
        else if (!strcmp(o, "--patience")) patience = arg_long("--patience", v, 1, 1000000);
        else if (!strcmp(o, "--converge")) converge = arg_long("--converge", v, 0, 100000000L);
        else if (!strcmp(o, "--seed")) seed = (uint32_t)arg_long("--seed", v, 0, 4294967295L);
        else if (!strcmp(o, "--align")) {
            if (!strcmp(v, "a1")) e.align = ALIGN_A1;
            else if (!strcmp(v, "a2")) e.align = ALIGN_A2;
            else if (!strcmp(v, "a3")) e.align = ALIGN_A3;
            else if (!strcmp(v, "grid")) e.align = ALIGN_GRID;
            else { fprintf(stderr, "station: --align must be a1, a2, a3 or grid, not '%s'\n", v); return 2; }
        }
        else if (!strcmp(o, "--L")) {
            if (!strcmp(v, "fit")) e.lref = L_FIT;
            else if (!strcmp(v, "median")) e.lref = L_MEDIAN;
            else if (!strcmp(v, "rsqrt")) e.lref = L_RSQRT;
            else { fprintf(stderr, "station: --L must be fit, median or rsqrt, not '%s'\n", v); return 2; }
        }
        else usage();
        a++;
    }
    if (!corpus) { fprintf(stderr, "station: --corpus is required\n"); return 2; }
    if (!strcmp(mode, "direct") || !strcmp(mode, "descend")) {
        if (!have_weights) { fprintf(stderr, "station: %s needs --weights\n", mode); return 2; }
        parse_weights(weights, e.w);
    }
    if (!strcmp(mode, "check") && !against) { fprintf(stderr, "station: check needs --against\n"); return 2; }
    gs = read_corpus(corpus, &k);

    if (!strcmp(mode, "check")) {
        long k2;
        graph *gs2 = read_corpus(against, &k2);
        const char *msg = NULL;
        for (gi = 0; gi < k && gi < k2 && !msg; gi++) msg = differ(&gs[gi], &gs2[gi]);
        if (msg) fprintf(stderr, "station: graph %ld: %s\n", gi - 1, msg);
        else if (k != k2) fprintf(stderr, "station: %s has %ld graphs, %s has %ld\n", corpus, k, against, k2);
        else printf("same %ld graphs\n", k);
        corpus_free(gs, k); corpus_free(gs2, k2);
        return msg || k != k2 ? 2 : 0;
    }

    if (!strcmp(mode, "terms")) {
        printf("id,n,m,L,Ls,ux,uy");
        for (i = 0; i < NTERMS; i++) printf(",%s", term_name[i]);
        printf("\n");
        for (gi = 0; gi < k; gi++) {
            const graph *g = &gs[gi];
            printf("%s,%ld,%ld,%.6f,%.6f,%g,%g", g->id, g->n, g->m,
                   ref_length(TERM_L, g, &e), ref_length(TERM_S, g, &e), g->ux, g->uy);
            for (i = 0; i < NTERMS; i++) printf(",%.6f", term_value((int)i, g, g->x, &e));
            printf("\n");
        }
        corpus_free(gs, k);
        return 0;
    }

    dx = malloc((size_t)dirs * sizeof *dx);
    dy = malloc((size_t)dirs * sizeof *dy);
    if (!dx || !dy) { fprintf(stderr, "station: out of memory\n"); return 2; }
    for (i = 0; i < dirs; i++) direction(i, dirs, &dx[i], &dy[i]);

    if (!strcmp(mode, "descend")) {
        int rc = descend(gs, k, &e, d, dirs, dx, dy, budget, seeds, jitter, patience, converge, seed);
        free(dx); free(dy);
        corpus_free(gs, k);
        return rc;
    }
    if (diffs) {
        printf("id,node,dir");
        for (i = 0; i < NTERMS; i++) printf(",%s", term_name[i]);
        printf("\n");
    }
    else if (nodes) printf("id,node,best_dir,decrease\n");
    else {
        printf("id,n,m,E,q,dec");
        for (i = 0; i < NTERMS; i++) printf(",%s", term_name[i]);
        printf("\n");
    }
    for (gi = 0; gi < k; gi++) {
        const graph *g = &gs[gi];
        double t[NTERMS], e0 = energy(g, g->x, &e, t), dec, q;
        double *y = malloc((size_t)(2 * g->n) * sizeof *y);
        if (!y) { fprintf(stderr, "station: out of memory\n"); return 2; }
        q = direct_q(g, g->x, &e, d, dirs, dx, dy, y, &dec, (int)diffs, (int)nodes);
        if (!nodes && !diffs) {
            printf("%s,%ld,%ld,%.6f,%.6f,%.6f", g->id, g->n, g->m, e0, q, dec);
            for (i = 0; i < NTERMS; i++) printf(",%.6f", t[i]);
            printf("\n");
        }
        free(y);
    }
    free(dx); free(dy);
    corpus_free(gs, k);
    return 0;
}
