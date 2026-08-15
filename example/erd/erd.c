/* erd.c -- placing new tables on an existing entity-relationship diagram.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * The graph is a real anonymized production schema (data/PROVENANCE.md has its story and the
 * README the problem's). When a migration adds ten tables, only those ten need placing: the
 * rest of the diagram must NOT move, a reader who knows where a table sits should still find
 * it there. So the old coordinates are frozen and the search has 2k variables for k new
 * tables, not 2n. The human's own placement of the ten is scored as a reference beside the
 * centroid heuristic.
 *
 * OBJECTIVE, in tiers so that no weight has to be guessed:
 *   edges passing through a table   the length of the segment inside the rectangle, x100
 *   edges crossing each other       the count, x100
 *   edge length                     the total, x1, which breaks ties toward a tidy diagram
 * Node overlap and staying on canvas are HARD, enforced by the repair callback.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../c/cjitter.h"
#include "erd_data.h"

#define MAXN 64
#define MAXE 128

/* Regenerating erd_data.h from a bigger schema must fail here, at compile time, never as a
 * silent overflow of the fixed arrays below. */
typedef char erd_tables_fit[(ERD_NFIXED + ERD_NNEW <= MAXN) ? 1 : -1];
typedef char erd_edges_fit[(ERD_NEDGE <= MAXE) ? 1 : -1];
typedef char erd_vector_fits[(2 * ERD_NNEW <= 64) ? 1 : -1];

/* The example's constants, in one place. W_TIER separates the objective's tiers by two orders
 * of magnitude, so length only ever breaks ties; PUSH_PASSES bounds the repair's overlap
 * resolution; the rest are the shipped budget, and every pinned number in tests/cli.sh and
 * the README is a function of them. */
#define W_TIER      100.0   /* penetration and crossings, against length at 1 */
#define PUSH_PASSES 4       /* overlap push-out sweeps per repaired table */
#define EVALS       8000
#define SEEDS       5
#define JITTER      0.25    /* first move size, as a fraction of the canvas */
#define POP         30

/* sqrt is correctly rounded by IEEE requirement and hypot is not; cjitter.h states the
 * discipline this length obeys. */
static double seglen(double dx, double dy) { return sqrt(dx * dx + dy * dy); }

typedef struct {
    long   n, nfixed, ne;
    double x[MAXN], y[MAXN];       /* fixed tables keep these; free ones are read from the vector */
    double w[MAXN], h[MAXN];
    long   e[MAXE][2];
    int    enew[MAXE];             /* does edge a touch a new table? decided once */
    double konst;                  /* every term among frozen tables only, summed once */
    double cw, ch;
} Erd;

/* Where table i sits: the frozen coordinate, or the searched one. */
static void pos(const Erd *g, const double *v, long i, double *px, double *py)
{
    if (i < g->nfixed) { *px = g->x[i]; *py = g->y[i]; }
    else { *px = v[2 * (i - g->nfixed)]; *py = v[2 * (i - g->nfixed) + 1]; }
}

/* Length of segment AB lying inside rectangle i. Continuous in the table's position, which a
 * crossing COUNT is not; the README's objective section says why that matters. */
static double through(const Erd *g, const double *v, long i, double ax, double ay,
                      double bx, double by)
{
    double px, py, dx = bx - ax, dy = by - ay, t0 = 0, t1 = 1;
    double q[4], d[4];
    long s;
    pos(g, v, i, &px, &py);
    /* Bounding-box reject first: on 44 tables almost every rectangle is nowhere near the
     * segment, and four comparisons here are what keep an evaluation cheap. */
    if ((ax < px - g->w[i]/2 && bx < px - g->w[i]/2) ||
        (ax > px + g->w[i]/2 && bx > px + g->w[i]/2) ||
        (ay < py - g->h[i]/2 && by < py - g->h[i]/2) ||
        (ay > py + g->h[i]/2 && by > py + g->h[i]/2)) return 0;
    /* Liang-Barsky: clip the segment to the rectangle, exactly and in O(1). An earlier
     * version sampled 25 points along the segment; the exact length is cheaper and has no
     * sampling grain for the search to fall between. */
    d[0] = -dx; q[0] = ax - (px - g->w[i]/2);
    d[1] =  dx; q[1] = (px + g->w[i]/2) - ax;
    d[2] = -dy; q[2] = ay - (py - g->h[i]/2);
    d[3] =  dy; q[3] = (py + g->h[i]/2) - ay;
    for (s = 0; s < 4; s++) {
        if (d[s] == 0) {
            if (q[s] < 0) return 0;              /* parallel to this edge and outside it */
        } else {
            double t = q[s] / d[s];
            if (d[s] < 0) { if (t > t0) t0 = t; }
            else          { if (t < t1) t1 = t; }
        }
    }
    return t1 > t0 ? (t1 - t0) * seglen(dx, dy) : 0;
}

static int cross(double ax, double ay, double bx, double by,
                 double cx, double cy, double dx, double dy)
{
    double d1 = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
    double d2 = (bx-ax)*(dy-ay) - (by-ay)*(dx-ax);
    double d3 = (dx-cx)*(ay-cy) - (dy-cy)*(ax-cx);
    double d4 = (dx-cx)*(by-cy) - (dy-cy)*(bx-cx);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

/* The incremental decomposition that makes the objective cheap: every term among frozen
 * tables only -- frozen edge through frozen table, frozen-frozen crossing, frozen edge
 * length -- is the same for every candidate, summed once into g->konst by frozen_part().
 * score() then evaluates only what a candidate can change: terms touching a new table. */
static double frozen_part(Erd *g)
{
    double total = 0;
    long a, b, i;
    for (a = 0; a < g->ne; a++) {
        double ax, ay, bx, by;
        g->enew[a] = g->e[a][0] >= g->nfixed || g->e[a][1] >= g->nfixed;
        if (g->enew[a]) continue;
        pos(g, NULL, g->e[a][0], &ax, &ay);
        pos(g, NULL, g->e[a][1], &bx, &by);
        total += seglen(bx - ax, by - ay);
        for (i = 0; i < g->nfixed; i++) {
            if (i == g->e[a][0] || i == g->e[a][1]) continue;
            total += W_TIER * through(g, NULL, i, ax, ay, bx, by);
        }
        for (b = a + 1; b < g->ne; b++) {
            double cx, cy, dx, dy;
            if (g->e[b][0] >= g->nfixed || g->e[b][1] >= g->nfixed) continue;
            if (g->e[a][0] == g->e[b][0] || g->e[a][0] == g->e[b][1] ||
                g->e[a][1] == g->e[b][0] || g->e[a][1] == g->e[b][1]) continue;
            pos(g, NULL, g->e[b][0], &cx, &cy);
            pos(g, NULL, g->e[b][1], &dx, &dy);
            if (cross(ax, ay, bx, by, cx, cy, dx, dy)) total += W_TIER;
        }
    }
    return total;
}

static double score(const double *v, void *ctx)
{
    Erd *g = ctx;
    double total = g->konst, len = 0;
    long a, b, i;
    for (a = 0; a < g->ne; a++) {
        double ax, ay, bx, by;
        pos(g, v, g->e[a][0], &ax, &ay);
        pos(g, v, g->e[a][1], &bx, &by);
        if (g->enew[a]) {
            len += seglen(bx - ax, by - ay);
            for (i = 0; i < g->n; i++) {
                if (i == g->e[a][0] || i == g->e[a][1]) continue;
                total += W_TIER * through(g, v, i, ax, ay, bx, by);
            }
        } else {
            for (i = g->nfixed; i < g->n; i++)
                total += W_TIER * through(g, v, i, ax, ay, bx, by);
        }
        for (b = a + 1; b < g->ne; b++) {
            double cx, cy, dx, dy;
            if (!g->enew[a] && !g->enew[b]) continue;
            if (g->e[a][0] == g->e[b][0] || g->e[a][0] == g->e[b][1] ||
                g->e[a][1] == g->e[b][0] || g->e[a][1] == g->e[b][1]) continue;
            pos(g, v, g->e[b][0], &cx, &cy);
            pos(g, v, g->e[b][1], &dx, &dy);
            if (cross(ax, ay, bx, by, cx, cy, dx, dy)) total += W_TIER;
        }
    }
    return total + len;
}

/* Table k's centre clamped onto the canvas. Called after every move a repair makes, not once
 * before them: the overlap push-out below can shove a table outward, and a clamp that ran only
 * first left two thirds of repaired points off the canvas -- an infeasible layout returned as
 * best. */
static void oncanvas(const Erd *g, long k, double *px, double *py)
{
    if (*px < g->w[k]/2) *px = g->w[k]/2;
    if (*px > g->cw - g->w[k]/2) *px = g->cw - g->w[k]/2;
    if (*py < g->h[k]/2) *py = g->h[k]/2;
    if (*py > g->ch - g->h[k]/2) *py = g->ch - g->h[k]/2;
}

/* Hard: on the canvas, and not overlapping any table. Enforced by moving the proposal, so an
 * unreadable diagram is never a candidate at all. The canvas bound holds by construction at
 * every step; the push-out is best-effort within it, over a fixed number of passes. */
static void legal(double *v, void *ctx)
{
    Erd *g = ctx;
    long k, i, pass;
    for (k = g->nfixed; k < g->n; k++) {
        double *px = &v[2 * (k - g->nfixed)], *py = &v[2 * (k - g->nfixed) + 1];
        oncanvas(g, k, px, py);
        for (pass = 0; pass < PUSH_PASSES; pass++)
            for (i = 0; i < g->n; i++) {
                double qx, qy, ox, oy;
                if (i == k) continue;
                pos(g, v, i, &qx, &qy);
                ox = (g->w[k] + g->w[i]) / 2 - fabs(*px - qx);
                oy = (g->h[k] + g->h[i]) / 2 - fabs(*py - qy);
                if (ox > 0 && oy > 0) {          /* push out along the shallower axis */
                    if (ox < oy) *px += (*px < qx ? -ox : ox);
                    else         *py += (*py < qy ? -oy : oy);
                    oncanvas(g, k, px, py);
                }
            }
    }
}

/* The control that might simply win: each new table at the centroid of its neighbours. */
static void centroid_place(const Erd *g, double *x)
{
    long i, k;
    for (k = 0; k < g->n - g->nfixed; k++) {
        double cx = 0, cy = 0;
        long m = 0;
        for (i = 0; i < g->ne; i++) {
            long u = g->e[i][0], w = g->e[i][1];
            if (u == g->nfixed + k && w < g->nfixed) { cx += g->x[w]; cy += g->y[w]; m++; }
            if (w == g->nfixed + k && u < g->nfixed) { cx += g->x[u]; cy += g->y[u]; m++; }
        }
        x[2*k] = m ? cx / (double)m : g->cw / 2;
        x[2*k+1] = m ? cy / (double)m : g->ch / 2;
    }
}

/* One panel of the picture: the canvas, the edges under the tables (the new tables' edges
 * darker, since they are the ones being judged), then every table with its name. */
static void svg_panel(const Erd *g, const double *v, double ox)
{
    long a, i;
    printf("  <rect x='%g' y='0' width='%g' height='%g' fill='#fafafa' stroke='#ccc'/>\n",
           ox, g->cw, g->ch);
    for (a = 0; a < g->ne; a++) {
        double ax, ay, bx, by;
        int nu = g->e[a][0] >= g->nfixed || g->e[a][1] >= g->nfixed;
        pos(g, v, g->e[a][0], &ax, &ay);
        pos(g, v, g->e[a][1], &bx, &by);
        printf("  <line x1='%g' y1='%g' x2='%g' y2='%g' stroke='%s' stroke-width='2'/>\n",
               ox + ax, ay, ox + bx, by, nu ? "#c60" : "#999");
    }
    for (i = 0; i < g->n; i++) {
        double px, py;
        int nu = i >= g->nfixed;
        pos(g, v, i, &px, &py);
        printf("  <rect x='%g' y='%g' width='%g' height='%g' rx='6' fill='%s' "
               "stroke='%s' stroke-width='2'/>\n",
               ox + px - g->w[i]/2, py - g->h[i]/2, g->w[i], g->h[i],
               nu ? "#fc3" : "#e8e8e8", nu ? "#963" : "#555");
        printf("  <text x='%g' y='%g' text-anchor='middle' font-size='%d' fill='%s'>"
               "%s</text>\n", ox + px, py + 8, nu ? 26 : 24,
               nu ? "#630" : "#333", erd_name[i]);
    }
}

/* Initial state, final state, and the reference: the centroid heuristic, the search's answer,
 * and the layout the human actually accepted, stacked vertically, the frozen tables identical
 * in all three. What the scores say, made visible: the heuristic drops each new table onto the
 * edges running between its neighbours. */
static void svg_out(const Erd *g, const double *xc, double sc,
                    const double *xb, const char *method, double sb,
                    const double *xh, double sh)
{
    double W = g->cw + 10, band = 90, H = 3 * (g->ch + band) + 10;
    const double *v[3];
    const char *title[3];
    char t0[96], t1[96], t2[96];
    long j;
    v[0] = xc; v[1] = xb; v[2] = xh;
    snprintf(t0, sizeof t0, "initial: neighbours&#8217; centroid, score %.6g", sc);
    snprintf(t1, sizeof t1, "final: %s at seed 1, score %.6g", method, sb);
    snprintf(t2, sizeof t2, "reference: the human&#8217;s accepted layout, score %.6g", sh);
    title[0] = t0; title[1] = t1; title[2] = t2;
    printf("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g' "
           "font-family='sans-serif'>\n", W, H);
    printf("<rect width='%g' height='%g' fill='white'/>\n", W, H);
    for (j = 0; j < 3; j++) {
        double oy = (double)j * (g->ch + band);
        printf("<text x='%g' y='%g' text-anchor='middle' font-size='44' fill='#111'>"
               "%s</text>\n", W / 2, oy + 60, title[j]);
        printf("<g transform='translate(0,%g)'>\n", oy + band);
        svg_panel(g, v[j], 5);
        printf("</g>\n");
    }
    printf("</svg>\n");
}

int main(int argc, char **argv)
{
    static Erd g;
    cjitter_problem p;
    cjitter_budget b;
    cjitter_result r;
    double lo[64], hi[64], x[64], xh[64];
    long i, k, nnew, nv;
    int want_svg = argc == 2 && !strcmp(argv[1], "--svg");

    if (argc > 1 && !want_svg) {
        fprintf(stderr, "erd: --svg is the only option\n");
        return 2;
    }

    /* The anonymized production schema from erd_data.h: the frozen tables keep the human's
     * coordinates, the added ones get theirs from the search. The human's own answer for them
     * stays in erd_cx/erd_cy past nfixed, scored below as a reference. */
    g.cw = ERD_CW; g.ch = ERD_CH;
    g.nfixed = ERD_NFIXED;
    nnew = ERD_NNEW;
    g.n = g.nfixed + nnew;
    for (i = 0; i < g.n; i++) {
        g.x[i] = erd_cx[i]; g.y[i] = erd_cy[i];
        g.w[i] = erd_w[i];  g.h[i] = erd_h[i];
    }
    g.ne = ERD_NEDGE;
    for (i = 0; i < g.ne; i++) { g.e[i][0] = erd_edge[i][0]; g.e[i][1] = erd_edge[i][1]; }
    g.konst = frozen_part(&g);

    nv = 2 * nnew;
    for (i = 0; i < nv; i += 2) {
        lo[i] = 0; hi[i] = g.cw;
        lo[i+1] = 0; hi[i+1] = g.ch;
    }
    p.n = nv; p.lo = lo; p.hi = hi; p.fitness = score; p.repair = legal; p.ctx = &g;
    b.evals = EVALS; b.seed = 1; b.jitter = JITTER; b.pop = POP;

    /* The human's own answer: where the migration's tables sit in the accepted diagram. */
    for (k = 0; k < nnew; k++) {
        xh[2*k] = erd_cx[g.nfixed + k];
        xh[2*k+1] = erd_cy[g.nfixed + k];
    }

    /* The picture instead of the report: centroid, search, and the human's layout, one SVG to
     * stdout, computed exactly as below so the two never disagree. */
    if (want_svg) {
        double xc[64], xb[64], sc, sh;
        centroid_place(&g, xc);
        legal(xc, &g);
        sc = score(xc, &g);
        sh = score(xh, &g);
        r.x = xb;
        if (cjitter_run("climb", &p, &b, &r) != 0) {
            fprintf(stderr, "erd: search failed\n");
            return 1;
        }
        svg_out(&g, xc, sc, xb, r.method, r.best, xh, sh);
        return 0;
    }

    printf("%ld tables already placed, %ld added by a migration, %ld foreign keys.\n",
           g.nfixed, nnew, g.ne);
    printf("A real schema, anonymized; see data/PROVENANCE.md. Only the new tables move.\n"
           "Objective: edges through tables and edge crossings, weighted 100, plus total\n"
           "edge length. Lower is better.\n\n");

    centroid_place(&g, x);
    legal(x, &g);
    printf("%-10s %12.6g   (place each new table at its neighbours' centroid)\n",
           "centroid", score(x, &g));
    printf("%-10s %12.6g   (where the human actually put them)\n\n",
           "human", score(xh, &g));

    if (cjitter_compare(&p, &b, SEEDS, stdout) != 0) {
        fprintf(stderr, "erd: comparison failed\n");
        return 1;
    }

    r.x = x;
    if (cjitter_run("climb", &p, &b, &r) == 0) {
        /* One run at seed 1, the run --svg draws. Its score sits somewhere in the table's
         * per-seed spread; calling it "best" implied the best of the panel, which it is not. */
        printf("\nthe layout %s found at seed 1, score %.6g:\n", r.method, r.best);
        for (k = 0; k < nnew; k++)
            printf("  %s at (%.0f, %.0f)\n", erd_name[g.nfixed + k], x[2*k], x[2*k+1]);
    }
    return 0;
}
