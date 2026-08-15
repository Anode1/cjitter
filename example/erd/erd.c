/* erd.c -- placing new tables on an existing entity-relationship diagram.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * MySQL Workbench lays out a diagram by heuristics and does it badly: edges run under tables and
 * cross each other, and every reverse-engineering of the schema scrambles the positions again.
 * Redrawing a 44-table diagram by hand costs about an hour.
 *
 * The observation that makes it tractable: when a migration adds three tables, only those three
 * need placing. The rest of the diagram must NOT move -- a reader who knows where a table sits
 * should still find it there. So the old coordinates are frozen and the search has 2k variables
 * for k new tables, not 2n.
 *
 * That is also why this is a fair demonstration of the library rather than a graph-drawing
 * paper: the search is small, the objective is cheap and exact, and there is a control (place
 * each new table at the centroid of its neighbours) that might simply win.
 *
 * OBJECTIVE, in tiers so that no weight has to be guessed:
 *   edges passing through a table   the length of the segment inside the rectangle, x100
 *   edges crossing each other       the count, x100, plus a continuous nearness term
 *   edge length                     the total, x1, which breaks ties toward a tidy diagram
 * Node overlap and staying on canvas are HARD, enforced by the repair callback, so they can
 * never be traded against the tiers above.
 *
 * This POC uses a graph built in code. Reading real coordinates out of a .mwb file (a zip around
 * document.mwb.xml) is the remaining work and is a parsing job, not a search one.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../../c/cjitter.h"

#define MAXN 64
#define MAXE 128

typedef struct {
    long   n, nfixed, ne;
    double x[MAXN], y[MAXN];       /* fixed tables keep these; free ones are read from the vector */
    double w[MAXN], h[MAXN];
    long   e[MAXE][2];
    double cw, ch;
} Erd;

/* Where table i sits: the frozen coordinate, or the searched one. */
static void pos(const Erd *g, const double *v, long i, double *px, double *py)
{
    if (i < g->nfixed) { *px = g->x[i]; *py = g->y[i]; }
    else { *px = v[2 * (i - g->nfixed)]; *py = v[2 * (i - g->nfixed) + 1]; }
}

/* Length of segment AB lying inside rectangle i, sampled. Continuous in the table's position,
 * which a crossing COUNT is not: a count is flat under small moves and gives the search nothing
 * to follow. */
static double through(const Erd *g, const double *v, long i, double ax, double ay,
                      double bx, double by)
{
    double px, py, inside = 0;
    long s, S = 24;
    pos(g, v, i, &px, &py);
    for (s = 0; s <= S; s++) {
        double t = (double)s / (double)S;
        double qx = ax + t * (bx - ax), qy = ay + t * (by - ay);
        if (fabs(qx - px) < g->w[i] / 2 && fabs(qy - py) < g->h[i] / 2) inside++;
    }
    return inside / (double)S * hypot(bx - ax, by - ay);
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

static double score(const double *v, void *ctx)
{
    Erd *g = ctx;
    double total = 0, len = 0;
    long a, b, i;
    for (a = 0; a < g->ne; a++) {
        double ax, ay, bx, by;
        pos(g, v, g->e[a][0], &ax, &ay);
        pos(g, v, g->e[a][1], &bx, &by);
        len += hypot(bx - ax, by - ay);
        for (i = 0; i < g->n; i++) {
            if (i == g->e[a][0] || i == g->e[a][1]) continue;
            total += 100.0 * through(g, v, i, ax, ay, bx, by);
        }
        for (b = a + 1; b < g->ne; b++) {
            double cx, cy, dx, dy;
            if (g->e[a][0] == g->e[b][0] || g->e[a][0] == g->e[b][1] ||
                g->e[a][1] == g->e[b][0] || g->e[a][1] == g->e[b][1]) continue;
            pos(g, v, g->e[b][0], &cx, &cy);
            pos(g, v, g->e[b][1], &dx, &dy);
            if (cross(ax, ay, bx, by, cx, cy, dx, dy)) total += 100.0;
        }
    }
    return total + len;
}

/* Table k's centre clamped onto the canvas. Called after every move a repair makes, not once
 * before them: the overlap push-out below can shove a table outward, and a clamp that ran only
 * first left two thirds of repaired points off the canvas -- an infeasible layout returned as
 * best, which is the one thing a repair exists to make impossible. */
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
        for (pass = 0; pass < 4; pass++)
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

int main(void)
{
    static Erd g;
    cjitter_problem p;
    cjitter_budget b;
    cjitter_result r;
    double lo[16], hi[16], x[16], cx = 0, cy = 0;
    long i, k, nnew, nv;

    /* Twelve existing tables on a grid, as Workbench would have left a tidy diagram, plus their
     * foreign keys. Then three tables a migration has added, each keyed to existing ones. */
    g.cw = 900; g.ch = 600;
    g.nfixed = 12;
    for (i = 0; i < 12; i++) {
        g.x[i] = 120 + (double)(i % 4) * 220;
        g.y[i] = 100 + (double)(i / 4) * 200;
        g.w[i] = 130; g.h[i] = 90;
    }
    nnew = 3;
    g.n = g.nfixed + nnew;
    for (i = g.nfixed; i < g.n; i++) { g.w[i] = 130; g.h[i] = 90; }
    g.ne = 0;
    { long fk[][2] = { {0,1},{1,2},{2,3},{0,4},{4,5},{5,6},{6,7},{4,8},{8,9},{9,10},{10,11},
                       {1,5},{2,6},{3,7},{5,9},{6,10},
                       {12,1},{12,5},{13,3},{13,10},{14,0},{14,11},{14,6} };
      for (i = 0; i < (long)(sizeof fk / sizeof fk[0]); i++) {
          g.e[g.ne][0] = fk[i][0]; g.e[g.ne][1] = fk[i][1]; g.ne++;
      } }

    nv = 2 * nnew;
    for (i = 0; i < nv; i += 2) {
        lo[i] = 0; hi[i] = g.cw;
        lo[i+1] = 0; hi[i+1] = g.ch;
    }
    p.n = nv; p.lo = lo; p.hi = hi; p.fitness = score; p.repair = legal; p.ctx = &g;
    b.evals = 12000; b.seed = 1; b.jitter = 0.25; b.pop = 30;

    printf("%ld tables already placed, %ld added by a migration, %ld foreign keys.\n",
           g.nfixed, nnew, g.ne);
    printf("Only the new tables move. Objective: edges through tables and edge crossings,\n"
           "weighted 100, plus total edge length. Lower is better.\n\n");

    /* The control that might simply win: each new table at the centroid of its neighbours. */
    for (k = 0; k < nnew; k++) {
        long m = 0;
        cx = cy = 0;
        for (i = 0; i < g.ne; i++) {
            long u = g.e[i][0], w2 = g.e[i][1];
            if (u == g.nfixed + k && w2 < g.nfixed) { cx += g.x[w2]; cy += g.y[w2]; m++; }
            if (w2 == g.nfixed + k && u < g.nfixed) { cx += g.x[u]; cy += g.y[u]; m++; }
        }
        x[2*k] = m ? cx / (double)m : g.cw / 2;
        x[2*k+1] = m ? cy / (double)m : g.ch / 2;
    }
    legal(x, &g);
    printf("%-10s %12.6g   (place each new table at its neighbours' centroid)\n\n",
           "centroid", score(x, &g));

    if (cjitter_compare(&p, &b, 7, stdout) != 0) {
        fprintf(stderr, "erd: comparison failed\n");
        return 1;
    }

    r.x = x;
    if (cjitter_run("climb", &p, &b, &r) == 0) {
        printf("\nbest layout found by %s, score %.6g:\n", r.method, r.best);
        for (k = 0; k < nnew; k++)
            printf("  new table %ld at (%.0f, %.0f)\n", k + 1, x[2*k], x[2*k+1]);
    }
    return 0;
}
