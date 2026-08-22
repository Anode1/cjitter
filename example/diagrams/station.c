/* station.c -- which aesthetic criteria hold a hand-drawn diagram layout?
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * A layout tool minimises a weighted sum of criteria. If people drew that way, a layout a
 * person accepted would be a local minimum of every criterion that carries weight. This
 * program asks, per criterion and per layout, whether it is.
 *
 * The directional test. For each node, try DIRS moves of length D (equally spaced angles),
 * every other node fixed. The node is HELD if none of the moves lowers the energy; ties hold,
 * since a move that changes nothing is not an improvement. q is the fraction of nodes held:
 * 1 means the layout is a local minimum of the energy over single-node moves of length D,
 * 0 means every node has somewhere better to be. It needs no gradient, so it is defined for
 * the crossing count and the overlap corners, and it needs no seed, so it reproduces exactly.
 *
 *     station direct --corpus F --weights C,O,L,S,R,A,N [--d 0.02] [--dirs 16]
 *                    [--align a1|a2|a3|grid] [--s 0.02] [--tol 0.005] [--nodes]
 *     station terms  --corpus F [--align ...] [--s ...] [--tol ...]
 *
 * direct prints one CSV row per graph: id, n, m, E (the energy at the layout), q, dec (the
 * mean over nodes of the best decrease found, over E; 0 when held), and the seven term
 * values at the layout. With --nodes it prints one row per node instead: id, node, the best
 * direction's index (-1 if held) and its decrease.
 *
 * terms prints id, n, m, L and the seven term values at the layout, which is how the tests
 * pin the formulas and how the paper's term-share line is made. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "energy.h"
#include "corpus.h"

static void usage(void)
{
    fprintf(stderr,
        "usage: station direct --corpus FILE --weights C,O,L,S,R,A,N [--d 0.02] [--dirs 16]\n"
        "                      [--align a1|a2|a3|grid] [--s 0.02] [--tol 0.005] [--nodes]\n"
        "       station terms  --corpus FILE [--align a1|a2|a3|grid] [--s 0.02] [--tol 0.005]\n");
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
    fprintf(stderr, "station: --weights needs seven non-negative numbers C,O,L,S,R,A,N, not '%s'\n", v);
    exit(2);
}

/* Unit direction k of DIRS, by a Taylor series so the table is the same on every platform. */
static void direction(long k, long dirs, double *cx, double *cy)
{
    double a = 6.283185307179586 * (double)k / (double)dirs, c = 1, s = a, t = 1;
    int i;
    for (i = 1; i <= 12; i++) {
        t *= -a * a / (double)((2 * i - 1) * (2 * i));
        c += t;
        s += t * a / (double)(2 * i + 1);
    }
    *cx = c; *cy = s;
}

int main(int argc, char **argv)
{
    const char *mode, *corpus = NULL, *weights = NULL;
    energy_spec e;
    double d = 0.02, *dx = NULL, *dy = NULL;
    long dirs = 16, k, gi, nodes = 0, i;
    int a, have_weights = 0;
    graph *gs;
    FILE *f;
    char err[256];

    if (argc < 2) usage();
    mode = argv[1];
    if (strcmp(mode, "direct") && strcmp(mode, "terms")) usage();
    memset(&e, 0, sizeof e);
    e.align = ALIGN_A3; e.s = 0.02; e.tol = 0.005;
    for (a = 2; a < argc; a++) {
        const char *o = argv[a], *v = a + 1 < argc ? argv[a + 1] : NULL;
        if (!strcmp(o, "--nodes")) { nodes = 1; continue; }
        if (!v) usage();
        if (!strcmp(o, "--corpus")) corpus = v;
        else if (!strcmp(o, "--weights")) { weights = v; have_weights = 1; }
        else if (!strcmp(o, "--d")) d = arg_double("--d", v, 1e-9, 1);
        else if (!strcmp(o, "--dirs")) dirs = arg_long("--dirs", v, 1, 3600);
        else if (!strcmp(o, "--s")) e.s = arg_double("--s", v, 1e-9, 1);
        else if (!strcmp(o, "--tol")) e.tol = arg_double("--tol", v, 0, 1);
        else if (!strcmp(o, "--align")) {
            if (!strcmp(v, "a1")) e.align = ALIGN_A1;
            else if (!strcmp(v, "a2")) e.align = ALIGN_A2;
            else if (!strcmp(v, "a3")) e.align = ALIGN_A3;
            else if (!strcmp(v, "grid")) e.align = ALIGN_GRID;
            else { fprintf(stderr, "station: --align must be a1, a2, a3 or grid, not '%s'\n", v); return 2; }
        }
        else usage();
        a++;
    }
    if (!corpus) { fprintf(stderr, "station: --corpus is required\n"); return 2; }
    if (!strcmp(mode, "direct")) {
        if (!have_weights) { fprintf(stderr, "station: direct needs --weights\n"); return 2; }
        parse_weights(weights, e.w);
    }
    f = fopen(corpus, "r");
    if (!f) { fprintf(stderr, "station: cannot open %s\n", corpus); return 2; }
    k = corpus_read(f, &gs, err, sizeof err);
    fclose(f);
    if (k < 0) { fprintf(stderr, "station: %s: %s\n", corpus, err); return 2; }
    if (k == 0) { fprintf(stderr, "station: %s: no graphs\n", corpus); return 2; }

    if (!strcmp(mode, "terms")) {
        printf("id,n,m,L");
        for (i = 0; i < NTERMS; i++) printf(",%s", term_name[i]);
        printf("\n");
        for (gi = 0; gi < k; gi++) {
            const graph *g = &gs[gi];
            printf("%s,%ld,%ld,%.6f", g->id, g->n, g->m, g->L);
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

    if (nodes) printf("id,node,best_dir,decrease\n");
    else {
        printf("id,n,m,E,q,dec");
        for (i = 0; i < NTERMS; i++) printf(",%s", term_name[i]);
        printf("\n");
    }
    for (gi = 0; gi < k; gi++) {
        const graph *g = &gs[gi];
        double t[NTERMS], e0 = energy(g, g->x, &e, t), dec = 0;
        double *y = malloc((size_t)(2 * g->n) * sizeof *y);
        long held = 0, node;
        if (!y) { fprintf(stderr, "station: out of memory\n"); return 2; }
        memcpy(y, g->x, (size_t)(2 * g->n) * sizeof *y);
        for (node = 0; node < g->n; node++) {
            double best = 0;
            long bdir = -1, dir;
            for (dir = 0; dir < dirs; dir++) {
                double e1;
                y[2 * node] = g->x[2 * node] + d * dx[dir];
                y[2 * node + 1] = g->x[2 * node + 1] + d * dy[dir];
                e1 = energy(g, y, &e, NULL);
                if (e0 - e1 > best) { best = e0 - e1; bdir = dir; }
            }
            y[2 * node] = g->x[2 * node];
            y[2 * node + 1] = g->x[2 * node + 1];
            if (bdir < 0) held++; else dec += best;
            if (nodes) printf("%s,%ld,%ld,%.9f\n", g->id, node, bdir, best);
        }
        if (!nodes) {
            printf("%s,%ld,%ld,%.6f,%.6f,%.6f", g->id, g->n, g->m, e0,
                   (double)held / (double)g->n, e0 > 0 ? dec / (double)g->n / e0 : 0);
            for (i = 0; i < NTERMS; i++) printf(",%.6f", t[i]);
            printf("\n");
        }
        free(y);
    }
    free(dx); free(dy);
    corpus_free(gs, k);
    return 0;
}
