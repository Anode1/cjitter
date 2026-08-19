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
 * Edges are routed the way the tool draws them, orthogonal polylines chosen by route_edge,
 * and the OBJECTIVE reads the routed connectors, in tiers so that no weight has to be
 * guessed:
 *   a connector passing through a table   the length of the overlap, x100
 *   connectors crossing each other        the count, x100
 *   connector length                      the total, x1, breaking ties toward tidy
 * Node overlap and staying on canvas are HARD, enforced by the repair callback. The router's
 * quality is measured, not assumed: the run prints what it reproduces of the human layout's
 * known 0 crossings and 0 penetration, and every score means only as much as that line.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../c/cjitter.h"
#include "../../c/rng.h"
#include "erd_data.h"

#define MAXN 64
#define MAXE 128

/* Regenerating erd_data.h from a bigger schema must fail here, at compile time, never as a
 * silent overflow of the fixed arrays below. */
typedef char erd_tables_fit[(ERD_NFIXED + ERD_NNEW <= MAXN) ? 1 : -1];
typedef char erd_edges_fit[(ERD_NEDGE <= MAXE) ? 1 : -1];
typedef char erd_vector_fits[(2 * ERD_NNEW <= 64) ? 1 : -1];

/* The example's constants, in one place. W_TIER separates the objective's tiers by two orders
 * of magnitude; PUSH_PASSES bounds the repair's overlap resolution; the rest are the shipped
 * budget, and every pinned number in tests/cli.sh and the README is a function of them.
 *
 * W_TIER's two orders of magnitude are a weight, not a magnitude, and this comment used to
 * claim they made length a tie-breaker. Measured at a returned layout they do not: the
 * searches drive penetration to zero, leaving a few dozen crossings at 100 against 59
 * connectors of a few hundred units each, so length carries 81 to 90 percent of what varies
 * and picks the answer. The tiering is still right; the inference from it was not. */
#define W_TIER      100.0   /* penetration and crossings, against length at 1 */
#define PUSH_PASSES 4       /* overlap push-out sweeps per repaired table */
#define EVALS       8000
#define SEEDS       5
#define JITTER      0.25    /* first move size, as a fraction of the canvas */
#define POP         30

/* sqrt is correctly rounded by IEEE requirement and hypot is not; cjitter.h states the
 * discipline this length obeys. */
static double seglen(double dx, double dy) { return sqrt(dx * dx + dy * dy); }

/* A routed edge: an orthogonal polyline of up to four points, with its bounding box kept
 * beside it so a crossing test can reject most pairs in four comparisons. */
typedef struct {
    double px[4], py[4];
    int    np;
    double minx, maxx, miny, maxy;
} Route;

typedef struct {
    long   n, nfixed, ne;
    double x[MAXN], y[MAXN];       /* fixed tables keep these; free ones are read from the vector */
    double w[MAXN], h[MAXN];
    long   e[MAXE][2];
    int    enew[MAXE];             /* does edge a touch a new table? decided once */
    double ofr0[MAXE], ofr1[MAXE]; /* attachment offsets, a fraction of the table side: each
                                      edge leaves its table at its own point, spread by the
                                      table's degree, so no two connectors share a segment
                                      and draw as a three-way junction */
    int    straight;               /* the one style boolean: straight diagonal edges, the
                                      representation diagonal-edge tools draw, instead of
                                      routed orthogonal connectors */
    double konst;                  /* every term among frozen tables only, summed once */
    Route  rt[MAXE];               /* frozen routes fixed once; new-touching rerouted per score */
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

/* Penetration of an axis-aligned segment into table i: pure interval overlap, the fast path
 * for routed segments, which are never diagonal. Falls back to the clip for anything else. */
static double apen(const Erd *g, const double *v, long i,
                   double x1, double y1, double x2, double y2)
{
    double px, py, lo, hi;
    pos(g, v, i, &px, &py);
    if (y1 == y2) {
        if (y1 <= py - g->h[i]/2 || y1 >= py + g->h[i]/2) return 0;
        lo = x1 < x2 ? x1 : x2;
        hi = x1 < x2 ? x2 : x1;
        if (lo < px - g->w[i]/2) lo = px - g->w[i]/2;
        if (hi > px + g->w[i]/2) hi = px + g->w[i]/2;
        return hi > lo ? hi - lo : 0;
    }
    if (x1 == x2) {
        if (x1 <= px - g->w[i]/2 || x1 >= px + g->w[i]/2) return 0;
        lo = y1 < y2 ? y1 : y2;
        hi = y1 < y2 ? y2 : y1;
        if (lo < py - g->h[i]/2) lo = py - g->h[i]/2;
        if (hi > py + g->h[i]/2) hi = py + g->h[i]/2;
        return hi > lo ? hi - lo : 0;
    }
    return through(g, v, i, x1, y1, x2, y2);
}

/* Move a polyline endpoint from its table's centre to the table's border, along the segment
 * it starts: connectors leave a table's side, the way the tool draws them, never its centre.
 * T is the border's position as a fraction of the segment; past 1 the segment ends inside
 * the table (adjacent or overlapping endpoints) and the centre stays. */
static double trim_t(double x0, double y0, double x1, double y1, double w, double h)
{
    double dx = x1 - x0, dy = y1 - y0, t = 2;
    if (dx != 0) { double tx = (w / 2) / fabs(dx); if (tx < t) t = tx; }
    if (dy != 0) { double ty = (h / 2) / fabs(dy); if (ty < t) t = ty; }
    return t;
}

static void anchor(const Erd *g, const double *v, long e0, long e1, Route *r)
{
    double px, py, t;
    int last = r->np - 1;
    if (r->np == 2) {
        /* one segment: both trims share it, and they must not cross each other */
        double ta, tb;
        pos(g, v, e0, &px, &py);
        ta = trim_t(r->px[0], r->py[0], r->px[1], r->py[1], g->w[e0], g->h[e0]);
        pos(g, v, e1, &px, &py);
        tb = trim_t(r->px[1], r->py[1], r->px[0], r->py[0], g->w[e1], g->h[e1]);
        if (ta + tb < 1) {
            double dx = r->px[1] - r->px[0], dy = r->py[1] - r->py[0];
            r->px[0] += ta * dx; r->py[0] += ta * dy;
            r->px[1] -= tb * dx; r->py[1] -= tb * dy;
        }
        return;
    }
    t = trim_t(r->px[0], r->py[0], r->px[1], r->py[1], g->w[e0], g->h[e0]);
    if (t <= 1) {
        r->px[0] += t * (r->px[1] - r->px[0]);
        r->py[0] += t * (r->py[1] - r->py[0]);
    }
    t = trim_t(r->px[last], r->py[last], r->px[last-1], r->py[last-1], g->w[e1], g->h[e1]);
    if (t <= 1) {
        r->px[last] += t * (r->px[last-1] - r->px[last]);
        r->py[last] += t * (r->py[last-1] - r->py[last]);
    }
}

/* Which edges edge A can already see when it routes. Mode 0 is the frozen pass (earlier
 * frozen edges only), mode 1 the scoring pass (all frozen, plus earlier migration edges),
 * mode 2 the full report (everything earlier). Later edges pay for a pair's crossing, so
 * each pair is priced exactly once. */
static int routed_before(const Erd *g, long b, long a, int mode)
{
    if (b == a) return 0;
    if (mode == 0) return !g->enew[b] && b < a;
    if (mode == 1) return !g->enew[b] || b < a;
    return b < a;
}

static long routes_cross(const Route *p, const Route *q);

static long cand_cross(const Erd *g, const Route *rt, const Route *cand, long a, int mode)
{
    long b, k = 0;
    for (b = 0; b < g->ne; b++) {
        if (!routed_before(g, b, a, mode)) continue;
        if (g->e[a][0] == g->e[b][0] || g->e[a][0] == g->e[b][1] ||
            g->e[a][1] == g->e[b][0] || g->e[a][1] == g->e[b][1]) continue;
        k += routes_cross(cand, &rt[b]);
    }
    return k;
}

/* Route edge A as Workbench draws one: orthogonal segments at 0 and 90 degrees, never a
 * diagonal. The candidates are the two L shapes and Z shapes whose middle segment slides
 * across the channel between the endpoints and one step beyond it on either side, which is
 * the nudge a person applies to a connector to clear an obstacle. The candidate with the
 * least penetration wins, length breaking ties, the earlier shape winning exact ties so the
 * choice is deterministic. Only the first NTAB tables exist for penetration, which is how
 * frozen edges get routed against the frozen diagram alone. Returns the route's own cost,
 * penetration at W_TIER plus length; crossings are the caller's to add, since they depend on
 * every other route. */
static double route_edge(const Erd *g, const double *v, long a, long ntab,
                         const Route *rt, int mode, Route *r,
                         double *pen_out, long *crs_out)
{
    static const double TIN[5]  = { 0.15, 0.3, 0.5, 0.7, 0.85 };
    static const double TOUT[6] = { -0.75, -0.5, -0.25, 1.25, 1.5, 1.75 };
    double ax, ay, bx, by, bestcost = 0, bestpen = 0;
    long e0 = g->e[a][0], e1 = g->e[a][1], bestcrs = 0;
    int c, have = 0, nc = 24;
    pos(g, v, e0, &ax, &ay);
    pos(g, v, e1, &bx, &by);
    if (g->straight) {
        /* the diagonal representation: one candidate, the direct segment */
        Route cand;
        double pen = 0;
        long i;
        cand.px[0] = ax; cand.py[0] = ay; cand.px[1] = bx; cand.py[1] = by;
        cand.np = 2;
        anchor(g, v, e0, e1, &cand);
        cand.minx = cand.px[0] < cand.px[1] ? cand.px[0] : cand.px[1];
        cand.maxx = cand.px[0] < cand.px[1] ? cand.px[1] : cand.px[0];
        cand.miny = cand.py[0] < cand.py[1] ? cand.py[0] : cand.py[1];
        cand.maxy = cand.py[0] < cand.py[1] ? cand.py[1] : cand.py[0];
        for (i = 0; i < ntab; i++) {
            if (i == e0 || i == e1) continue;
            pen += through(g, v, i, cand.px[0], cand.py[0], cand.px[1], cand.py[1]);
        }
        *r = cand;
        {
            long crs = cand_cross(g, rt, &cand, a, mode);
            if (pen_out) *pen_out = pen;
            if (crs_out) *crs_out = crs;
            return W_TIER * (pen + (double)crs)
                 + seglen(cand.px[1] - cand.px[0], cand.py[1] - cand.py[0]);
        }
    }
    for (c = 0; c < nc; c++) {
        Route cand;
        double cost, pen = 0, len = 0;
        long i;
        int s;
        double ya = ay + g->ofr0[a] * g->h[e0];   /* horizontal departure line */
        double xa = ax + g->ofr0[a] * g->w[e0];   /* vertical departure line */
        double yb = by + g->ofr1[a] * g->h[e1];   /* horizontal arrival line */
        double xb = bx + g->ofr1[a] * g->w[e1];   /* vertical arrival line */
        if (c == 0) {                       /* L: horizontal, then vertical */
            cand.px[0] = ax; cand.py[0] = ya; cand.px[1] = xb; cand.py[1] = ya;
            cand.px[2] = xb; cand.py[2] = by; cand.np = 3;
        } else if (c == 1) {                /* L: vertical, then horizontal */
            cand.px[0] = xa; cand.py[0] = ay; cand.px[1] = xa; cand.py[1] = yb;
            cand.px[2] = bx; cand.py[2] = yb; cand.np = 3;
        } else if (c < 7) {                 /* Z, vertical middle, inside the channel */
            double mx = ax + TIN[c - 2] * (bx - ax);
            cand.px[0] = ax; cand.py[0] = ya; cand.px[1] = mx; cand.py[1] = ya;
            cand.px[2] = mx; cand.py[2] = yb; cand.px[3] = bx; cand.py[3] = yb;
            cand.np = 4;
        } else if (c < 12) {                /* Z, horizontal middle, inside the channel */
            double my = ay + TIN[c - 7] * (by - ay);
            cand.px[0] = xa; cand.py[0] = ay; cand.px[1] = xa; cand.py[1] = my;
            cand.px[2] = xb; cand.py[2] = my; cand.px[3] = xb; cand.py[3] = by;
            cand.np = 4;
        } else if (c < 18) {                /* Z, vertical middle, detouring beyond it */
            double mx = ax + TOUT[c - 12] * (bx - ax);
            cand.px[0] = ax; cand.py[0] = ya; cand.px[1] = mx; cand.py[1] = ya;
            cand.px[2] = mx; cand.py[2] = yb; cand.px[3] = bx; cand.py[3] = yb;
            cand.np = 4;
        } else {                            /* Z, horizontal middle, detouring */
            double my = ay + TOUT[c - 18] * (by - ay);
            cand.px[0] = xa; cand.py[0] = ay; cand.px[1] = xa; cand.py[1] = my;
            cand.px[2] = xb; cand.py[2] = my; cand.px[3] = xb; cand.py[3] = by;
            cand.np = 4;
        }
        /* collapse zero-length segments so the border trim always sees a real direction */
        {
            int m2 = 1;
            for (s = 1; s < cand.np; s++)
                if (cand.px[s] != cand.px[m2-1] || cand.py[s] != cand.py[m2-1]) {
                    cand.px[m2] = cand.px[s]; cand.py[m2] = cand.py[s]; m2++;
                }
            cand.np = m2;
        }
        if (cand.np >= 2) anchor(g, v, e0, e1, &cand);
        cand.minx = cand.maxx = cand.px[0];
        cand.miny = cand.maxy = cand.py[0];
        for (s = 1; s < cand.np; s++) {
            if (cand.px[s] < cand.minx) cand.minx = cand.px[s];
            if (cand.px[s] > cand.maxx) cand.maxx = cand.px[s];
            if (cand.py[s] < cand.miny) cand.miny = cand.py[s];
            if (cand.py[s] > cand.maxy) cand.maxy = cand.py[s];
        }
        for (i = 0; i < ntab; i++) {
            double px, py;
            if (i == e0 || i == e1) continue;
            pos(g, v, i, &px, &py);
            if (px + g->w[i]/2 < cand.minx || px - g->w[i]/2 > cand.maxx ||
                py + g->h[i]/2 < cand.miny || py - g->h[i]/2 > cand.maxy) continue;
            for (s = 0; s + 1 < cand.np; s++)
                pen += apen(g, v, i, cand.px[s], cand.py[s],
                            cand.px[s+1], cand.py[s+1]);
        }
        for (s = 0; s + 1 < cand.np; s++)
            len += fabs(cand.px[s+1] - cand.px[s]) + fabs(cand.py[s+1] - cand.py[s]);
        cost = W_TIER * pen + len;
        if (!have || cost < bestcost + 1e-9) {
            long crs;
            /* crossings are the expensive part of a candidate's price, so they are only
             * computed when penetration and length alone have not already lost */
            crs = cand_cross(g, rt, &cand, a, mode);
            cost += W_TIER * (double)crs;
            if (!have || cost < bestcost) {
                have = 1; bestcost = cost; bestpen = pen; bestcrs = crs; *r = cand;
            }
            /* A clean candidate among the in-channel shapes ends the search: those shapes
             * share their Manhattan length, the detours are longer, and an equal-cost later
             * shape would lose the tie anyway. */
            if (bestpen == 0 && bestcrs == 0 && c < 12) break;
        }
    }
    if (pen_out) *pen_out = bestpen;
    if (crs_out) *crs_out = bestcrs;
    return bestcost;
}

/* Crossings between two routed edges, bounding boxes first. Collinear overlaps are not
 * counted: two connectors sharing a grid line is what an offset exists to fix in a tool,
 * and the orientation test only sees proper crossings. */
static long routes_cross(const Route *p, const Route *q)
{
    long k = 0;
    int i, j;
    if (p->maxx < q->minx || p->minx > q->maxx ||
        p->maxy < q->miny || p->miny > q->maxy) return 0;
    for (i = 0; i + 1 < p->np; i++)
        for (j = 0; j + 1 < q->np; j++)
            if (cross(p->px[i], p->py[i], p->px[i+1], p->py[i+1],
                      q->px[j], q->py[j], q->px[j+1], q->py[j+1])) k++;
    return k;
}

/* Everything among frozen tables only, summed once. Frozen edges are routed here against the
 * frozen diagram and never again: a maintained diagram does not re-route its existing
 * connectors when a migration adds tables, and the search should have to work around them
 * where they already run. */
static double frozen_part(Erd *g)
{
    double total = 0;
    long a;
    for (a = 0; a < g->ne; a++)
        g->enew[a] = g->e[a][0] >= g->nfixed || g->e[a][1] >= g->nfixed;
    for (a = 0; a < g->ne; a++)
        if (!g->enew[a])
            total += route_edge(g, NULL, a, g->nfixed, g->rt, 0, &g->rt[a], NULL, NULL);
    return total;
}

static double score(const double *v, void *ctx)
{
    Erd *g = ctx;
    double total = g->konst;
    long a, i;
    int s;
    /* the migration's edges, routed in index order, each seeing every fixed connector and
     * the migration edges already routed this evaluation; the route's cost already carries
     * its crossings, so no pair is priced twice */
    for (a = 0; a < g->ne; a++)
        if (g->enew[a])
            total += route_edge(g, v, a, g->n, g->rt, 1, &g->rt[a], NULL, NULL);
    /* the fixed connectors, penetrating any new table parked on top of them */
    for (a = 0; a < g->ne; a++) {
        if (g->enew[a]) continue;
        for (i = g->nfixed; i < g->n; i++)
            for (s = 0; s + 1 < g->rt[a].np; s++)
                total += W_TIER * apen(g, v, i, g->rt[a].px[s], g->rt[a].py[s],
                                       g->rt[a].px[s+1], g->rt[a].py[s+1]);
    }
    return total;
}

/* Route every edge against every table at layout V, as the diagram would actually be drawn,
 * and report what a reader sees: crossings and total penetration. This is the calibration
 * against the one certain fact about the human's layout, that it achieved zero of both by
 * hand; a router that cannot reproduce that on the human's own coordinates is weaker than
 * the tool the human was using. RT must hold ne routes. */
static void layout_report(const Erd *g, const double *v, Route *rt,
                          long *ncross, double *pen)
{
    long a, k = 0;
    double p = 0;
    for (a = 0; a < g->ne; a++) {
        double ep;
        long ec;
        route_edge(g, v, a, g->n, rt, 2, &rt[a], &ep, &ec);
        p += ep;
        k += ec;
    }
    *ncross = k;
    *pen = p;
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

/* One panel of the picture: the canvas, the routed connectors under the tables (the new
 * tables' edges darker, since they are the ones being judged), then every table with its
 * name. The routing drawn is the routing scored: the picture and the number cannot drift. */
static void svg_panel(const Erd *g, const double *v, double ox)
{
    static Route rt[MAXE];
    double pen;
    long a, i, nc;
    layout_report(g, v, rt, &nc, &pen);
    printf("  <rect x='%g' y='0' width='%g' height='%g' fill='#fafafa' stroke='#ccc'/>\n",
           ox, g->cw, g->ch);
    for (a = 0; a < g->ne; a++) {
        int nu = g->e[a][0] >= g->nfixed || g->e[a][1] >= g->nfixed, s;
        printf("  <polyline points='");
        for (s = 0; s < rt[a].np; s++)
            printf("%s%g,%g", s ? " " : "", ox + rt[a].px[s], rt[a].py[s]);
        printf("' fill='none' stroke='%s' stroke-width='2'/>\n", nu ? "#c60" : "#999");
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

/* Four states, stacked: the scramble reverse-engineering leaves, the centroid heuristic, the
 * search's answer, and the layout the human actually accepted. The frozen tables are
 * identical in the middle two and the reference; the scrambled panel is the diagram nobody
 * wants, the reason the hour was spent. */
static void svg_out(const Erd *g, const Erd *gs, const double *xs,
                    const double *xc, double sc,
                    const double *xb, const char *method, double sb,
                    const double *xh, double sh)
{
    double W = g->cw + 10, band = 90, H = 4 * (g->ch + band) + 10;
    const double *v[4];
    const Erd *ge[4];
    const char *title[4];
    char t1[96], t2[96], t3[96];
    long j;
    ge[0] = gs; ge[1] = g; ge[2] = g; ge[3] = g;
    v[0] = xs; v[1] = xc; v[2] = xb; v[3] = xh;
    title[0] = "scrambled: the kept positions are gone after a reverse-engineering";
    snprintf(t1, sizeof t1, "initial: neighbours&#8217; centroid, score %.6g", sc);
    snprintf(t2, sizeof t2, "final: %s at seed 1, score %.6g", method, sb);
    snprintf(t3, sizeof t3, "reference: the human&#8217;s accepted layout, score %.6g", sh);
    title[1] = t1; title[2] = t2; title[3] = t3;
    printf("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g' "
           "font-family='sans-serif'>\n", W, H);
    printf("<rect width='%g' height='%g' fill='white'/>\n", W, H);
    for (j = 0; j < 4; j++) {
        double oy = (double)j * (g->ch + band);
        printf("<text x='%g' y='%g' text-anchor='middle' font-size='44' fill='#111'>"
               "%s</text>\n", W / 2, oy + 60, title[j]);
        printf("<g transform='translate(0,%g)'>\n", oy + band);
        svg_panel(ge[j], v[j], 5);
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
    cjitter_tuning t;
    double lo[64], hi[64], x[64], xh[64];
    long i, k, nnew, nv, block = 0;
    int style, ai;
    int want_svg = 0;

    /* --block N is cjitter_tuning.block, how many variables one proposal moves. Omitted, the
     * tuning default stands: the whole vector, which is what every pinned number here was
     * measured at. Two is one table per proposal, the setting this objective rewards. */
    for (ai = 1; ai < argc; ai++) {
        if (!strcmp(argv[ai], "--svg")) want_svg = 1;
        else if (!strcmp(argv[ai], "--svg-straight")) want_svg = 2;
        else if (!strcmp(argv[ai], "--block") && ai + 1 < argc) {
            block = atol(argv[++ai]);      /* atol reads junk as 0, which this refuses */
            if (block < 1) {
                fprintf(stderr, "erd: --block needs at least one variable\n");
                return 2;
            }
        } else {
            fprintf(stderr, "erd: options are --svg, --svg-straight and --block N\n");
            return 2;
        }
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
    /* Attachment slots: the j-th of a table's d edges leaves at fraction
     * ((j+1)/(d+1) - 1/2) * 0.8 of the side, in edge order, deterministically. */
    {
        long deg[MAXN] = { 0 }, seen[MAXN] = { 0 };
        for (i = 0; i < g.ne; i++) { deg[g.e[i][0]]++; deg[g.e[i][1]]++; }
        for (i = 0; i < g.ne; i++) {
            g.ofr0[i] = ((double)(seen[g.e[i][0]]++ + 1) / (double)(deg[g.e[i][0]] + 1)
                         - 0.5) * 0.8;
            g.ofr1[i] = ((double)(seen[g.e[i][1]]++ + 1) / (double)(deg[g.e[i][1]] + 1)
                         - 0.5) * 0.8;
        }
    }
    g.konst = frozen_part(&g);

    nv = 2 * nnew;
    for (i = 0; i < nv; i += 2) {
        lo[i] = 0; hi[i] = g.cw;
        lo[i+1] = 0; hi[i+1] = g.ch;
    }
    p.n = nv; p.lo = lo; p.hi = hi; p.fitness = score; p.repair = legal; p.ctx = &g;
    b.evals = EVALS; b.seed = 1; b.jitter = JITTER; b.pop = POP;
    t = cjitter_tuning_default(nv);
    if (block > 0) t.block = block;

    /* The human's own answer: where the migration's tables sit in the accepted diagram. */
    for (k = 0; k < nnew; k++) {
        xh[2*k] = erd_cx[g.nfixed + k];
        xh[2*k+1] = erd_cy[g.nfixed + k];
    }

    /* The picture instead of the report: centroid, search, and the human's layout, one SVG to
     * stdout, computed exactly as below so the two never disagree. */
    if (want_svg) {
        static Erd gs;
        double xc[64], xb[64], xs[64], sc, sh;
        Rng sr;
        g.straight = want_svg == 2;
        g.konst = frozen_part(&g);
        /* the scrambled state: every table where a reverse-engineering drops it */
        gs = g;
        cjitter_rng_seed(&sr, 42);
        for (i = 0; i < gs.n; i++) {
            double px = gs.w[i] / 2 + cjitter_rng_uniform(&sr, 0, gs.cw - gs.w[i]);
            double py = gs.h[i] / 2 + cjitter_rng_uniform(&sr, 0, gs.ch - gs.h[i]);
            if (i < gs.nfixed) { gs.x[i] = px; gs.y[i] = py; }
            else { xs[2*(i - gs.nfixed)] = px; xs[2*(i - gs.nfixed) + 1] = py; }
        }
        centroid_place(&g, xc);
        legal(xc, &g);
        sc = score(xc, &g);
        sh = score(xh, &g);
        r.x = xb;
        if (cjitter_run_tuned("climb", &p, &b, &t, &r) != 0) {
            fprintf(stderr, "erd: search failed\n");
            return 1;
        }
        svg_out(&g, &gs, xs, xc, sc, xb, r.method, r.best, xh, sh);
        return 0;
    }

    printf("%ld tables already placed, %ld added by a migration, %ld foreign keys.\n",
           g.nfixed, nnew, g.ne);
    printf("A real schema, anonymized; see data/PROVENANCE.md. Only the new tables move.\n"
           "The same experiment runs twice: first with straight diagonal edges, the\n"
           "representation diagonal-edge tools draw, then with orthogonal routed\n"
           "connectors, what the reader actually sees. Objective in both: penetration\n"
           "and crossings at 100, length at 1. Lower is better.\n");
    printf("One proposal moves %ld of the %ld variables%s.\n", t.block, nv,
           t.block >= nv ? " (the whole vector)" : ", so a table at a time");

    /* The one style boolean, both ways: the two sections differ in nothing but it. */
    for (style = 1; style >= 0; style--) {
        static Route rt[MAXE];
        double pen;
        long nc;
        g.straight = style;
        g.konst = frozen_part(&g);
        printf("\n---- %s ----\n\n", style ? "straight diagonal edges"
                                           : "orthogonal routed connectors");
        centroid_place(&g, x);
        legal(x, &g);
        printf("%-10s %12.6g   (the centroid rule: each new table at its neighbours' centroid)\n",
               "centroid", score(x, &g));
        printf("%-10s %12.6g   (the human placement, where the maintainer put them)\n",
               "human", score(xh, &g));
        /* The edge model's calibration, against the one certain fact about the human's
         * layout: it achieved no crossings and no edge under a table on screen. What the
         * model reproduces of that is the model's quality, and every score in this section
         * is only as meaningful as this line. */
        layout_report(&g, xh, rt, &nc, &pen);
        printf("%-10s %ld crossings, %.6g penetration under this edge model (the hand\n"
               "%-10s layout achieved 0 and 0; the shortfall is the model's, not the\n"
               "%-10s human's)\n\n", "", nc, pen, "", "");

        if (cjitter_compare_tuned(&p, &b, &t, SEEDS, stdout) != 0) {
            fprintf(stderr, "erd: comparison failed\n");
            return 1;
        }

        r.x = x;
        if (cjitter_run_tuned("climb", &p, &b, &t, &r) == 0) {
            /* One run at seed 1, the run --svg draws (routed style). Its score sits
             * somewhere in the table's per-seed spread. */
            printf("\nthe layout %s found at seed 1, score %.6g:\n", r.method, r.best);
            for (k = 0; k < nnew; k++)
                printf("  %s at (%.0f, %.0f)\n", erd_name[g.nfixed + k], x[2*k], x[2*k+1]);
            layout_report(&g, x, rt, &nc, &pen);
            printf("under this edge model: %ld crossings, %.6g penetration\n", nc, pen);
        }
    }
    return 0;
}
