/* reinsert.c -- the box-reinsertion benchmark: delete one box, put it back with an energy.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 *     reinsert --corpus F --weights C,O,L,S,R,A,N,F [--align a1|a2|a3|grid]
 *              [--L fit|median|rsqrt] [--budget 4000] [--seed 1] [--method climb]
 *
 * For every node of every graph: freeze every other node at its hand position, start the
 * deleted box at a uniform draw in the unit square, and let the library place it by
 * minimising the weighted energy at the given budget. One CSV row per node: id, node, the
 * true centre, the placed centre, and the distance between them in drawing widths. The
 * reference length and the flow direction are the corpus's, fixed as everywhere else. A
 * benchmark row says how far an energy puts a box from where the person put it; whether an
 * energy earns its keep is read by comparing rows across energies on the same nodes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "corpus.h"
#include "../../c/cjitter.h"

typedef struct {
    const graph *g;
    energy_spec *e;
    double      *x;   /* 2n working copy */
    long         i;   /* the node being placed */
} placing;

static double fitness(const double *v, void *ctx)
{
    placing *p = ctx;
    p->x[2 * p->i] = v[0];
    p->x[2 * p->i + 1] = v[1];
    return energy(p->g, p->x, p->e, NULL);
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
    fprintf(stderr, "reinsert: --weights needs %d comma-separated non-negative values\n", NTERMS);
    exit(2);
}

int main(int argc, char **argv)
{
    const char *corpus = NULL, *weights = NULL, *method = "climb";
    long budget = 4000, k, gi;
    uint32_t seed = 1;
    energy_spec e;
    graph *gs;
    char err[256];
    FILE *f;
    int a;

    memset(&e, 0, sizeof e);
    e.align = ALIGN_A1; e.lref = L_FIT; e.s = 0.02; e.tol = 0.005;
    for (a = 1; a + 1 < argc; a += 2) {
        const char *o = argv[a], *v = argv[a + 1];
        if (!strcmp(o, "--corpus")) corpus = v;
        else if (!strcmp(o, "--weights")) weights = v;
        else if (!strcmp(o, "--budget")) budget = atol(v);
        else if (!strcmp(o, "--seed")) seed = (uint32_t)atol(v);
        else if (!strcmp(o, "--method")) method = v;
        else if (!strcmp(o, "--align")) {
            if (!strcmp(v, "a1")) e.align = ALIGN_A1;
            else if (!strcmp(v, "a2")) e.align = ALIGN_A2;
            else if (!strcmp(v, "a3")) e.align = ALIGN_A3;
            else if (!strcmp(v, "grid")) e.align = ALIGN_GRID;
            else { fprintf(stderr, "reinsert: bad --align %s\n", v); return 2; }
        }
        else if (!strcmp(o, "--L")) {
            if (!strcmp(v, "fit")) e.lref = L_FIT;
            else if (!strcmp(v, "median")) e.lref = L_MEDIAN;
            else if (!strcmp(v, "rsqrt")) e.lref = L_RSQRT;
            else { fprintf(stderr, "reinsert: bad --L %s\n", v); return 2; }
        }
        else { fprintf(stderr, "reinsert: unknown option %s\n", o); return 2; }
    }
    if (!corpus || !weights || budget < 1) {
        fprintf(stderr, "usage: reinsert --corpus F --weights C,O,L,S,R,A,N,F [--align ...] "
                        "[--L ...] [--budget 4000] [--seed 1] [--method climb]\n");
        return 2;
    }
    parse_weights(weights, e.w);
    f = fopen(corpus, "r");
    if (!f) { fprintf(stderr, "reinsert: cannot open %s\n", corpus); return 2; }
    k = corpus_read(f, &gs, err, sizeof err);
    fclose(f);
    if (k < 0) { fprintf(stderr, "reinsert: %s\n", err); return 2; }

    printf("id,node,truex,truey,placedx,placedy,dist\n");
    for (gi = 0; gi < k; gi++) {
        const graph *g = &gs[gi];
        double *x = malloc(2 * (size_t)g->n * sizeof *x);
        long i;
        if (!x) return 2;
        for (i = 0; i < g->n; i++) {
            static const double lo[2] = { 0, 0 }, hi[2] = { 1, 1 };
            placing ctx = { g, &e, x, i };
            cjitter_problem p = { 2, lo, hi, fitness, NULL, &ctx, NULL };
            cjitter_budget  b = { 0, 0 };
            cjitter_result  r = { 0 };
            double best[2], tx = g->x[2 * i], ty = g->x[2 * i + 1], d;
            memcpy(x, g->x, 2 * (size_t)g->n * sizeof *x);
            b.evals = budget;
            b.seed = seed + (uint32_t)(1000 * gi + i);
            r.x = best;
            if (cjitter_run(method, &p, &b, &r) != 0) {
                fprintf(stderr, "reinsert: %s failed on %s node %ld\n", method, g->id, i);
                return 2;
            }
            d = sqrt((best[0] - tx) * (best[0] - tx) + (best[1] - ty) * (best[1] - ty));
            printf("%s,%ld,%.6f,%.6f,%.6f,%.6f,%.6f\n", g->id, i, tx, ty, best[0], best[1], d);
        }
        free(x);
    }
    corpus_free(gs, k);
    return 0;
}
