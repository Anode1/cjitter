/* energy.c -- the terms. See energy.h for what each one measures.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE. */
#include "energy.h"
#include <math.h>

const char *const term_name[NTERMS] = {
    "crossings", "overlap", "length", "stress", "orthogonality", "alignment", "node-edge"
};
const char term_letter[NTERMS + 1] = "COLSRAN";

double energy_exp_neg(double x)
{
    double y, t = 1.0, s = 1.0;
    int i;
    if (x > 0) x = 0;
    if (x < -40) return 0;
    y = x / 64.0;
    for (i = 1; i <= 8; i++) { t *= y / (double)i; s += t; }
    for (i = 0; i < 6; i++) s *= s;
    return s;
}

/* Proper crossing of segments ab and cd: each pair strictly separates the other's endpoints.
 * Touching and collinear overlap do not count. */
static int crosses(double ax, double ay, double bx, double by,
                   double cx, double cy, double dx, double dy)
{
    double d1 = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    double d2 = (bx - ax) * (dy - ay) - (by - ay) * (dx - ax);
    double d3 = (dx - cx) * (ay - cy) - (dy - cy) * (ax - cx);
    double d4 = (dx - cx) * (by - cy) - (dy - cy) * (bx - cx);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0))
        && d1 != 0 && d2 != 0 && d3 != 0 && d4 != 0;
}

static double t_crossings(const graph *g, const double *x)
{
    long i, j, c = 0;
    for (i = 0; i < g->m; i++)
        for (j = i + 1; j < g->m; j++) {
            if (g->ea[i] == g->ea[j] || g->ea[i] == g->eb[j] ||
                g->eb[i] == g->ea[j] || g->eb[i] == g->eb[j]) continue;
            if (crosses(x[2 * g->ea[i]], x[2 * g->ea[i] + 1], x[2 * g->eb[i]], x[2 * g->eb[i] + 1],
                        x[2 * g->ea[j]], x[2 * g->ea[j] + 1], x[2 * g->eb[j]], x[2 * g->eb[j] + 1]))
                c++;
        }
    return (double)c / (double)g->m;
}

static double t_overlap(const graph *g, const double *x)
{
    long i, j;
    double ov = 0, area = 0;
    for (i = 0; i < g->n; i++) {
        area += g->w[i] * g->h[i];
        for (j = i + 1; j < g->n; j++) {
            double ox = (g->w[i] + g->w[j]) / 2 - fabs(x[2 * i] - x[2 * j]);
            double oy = (g->h[i] + g->h[j]) / 2 - fabs(x[2 * i + 1] - x[2 * j + 1]);
            if (ox > 0 && oy > 0) ov += ox * oy;
        }
    }
    return area > 0 ? ov / area : 0;
}

static double t_length(const graph *g, const double *x)
{
    long i;
    double s = 0;
    for (i = 0; i < g->m; i++) {
        double dx = x[2 * g->ea[i]] - x[2 * g->eb[i]], dy = x[2 * g->ea[i] + 1] - x[2 * g->eb[i] + 1];
        double r = (sqrt(dx * dx + dy * dy) - g->L) / g->L;
        s += r * r;
    }
    return s / (double)g->m;
}

static double t_stress(const graph *g, const double *x)
{
    long i, j, pairs = 0;
    double s = 0;
    for (i = 0; i < g->n; i++)
        for (j = i + 1; j < g->n; j++) {
            double dx = x[2 * i] - x[2 * j], dy = x[2 * i + 1] - x[2 * j + 1];
            double ideal = g->L * (double)g->dist[i * g->n + j];
            double r = (sqrt(dx * dx + dy * dy) - ideal) / ideal;
            s += r * r;
            pairs++;
        }
    return pairs > 0 ? s / (double)pairs : 0;
}

static double t_orth(const graph *g, const double *x)
{
    long i;
    double s = 0;
    for (i = 0; i < g->m; i++) {
        double dx = fabs(x[2 * g->ea[i]] - x[2 * g->eb[i]]), dy = fabs(x[2 * g->ea[i] + 1] - x[2 * g->eb[i] + 1]);
        if (dx + dy > 0) s += (dx < dy ? dx : dy) / (dx + dy);
    }
    return s / (double)g->m;
}

static double t_align(const graph *g, const double *x, const energy_spec *e)
{
    long i, j;
    double s = 0;
    for (i = 0; i < g->n; i++) {
        double bx = 1e9, by = 1e9, acc = 0;
        long rows = 0, cols = 0;
        for (j = 0; j < g->n; j++) {
            double ax, ay;
            if (j == i) continue;
            ax = fabs(x[2 * i] - x[2 * j]);
            ay = fabs(x[2 * i + 1] - x[2 * j + 1]);
            if (ax < bx) bx = ax;
            if (ay < by) by = ay;
            if (ax <= e->tol) cols++;
            if (ay <= e->tol) rows++;
            if (e->align == ALIGN_A3) {
                double u = ax / e->s, v = ay / e->s;
                acc += energy_exp_neg(-u * u) + energy_exp_neg(-v * v);
            }
        }
        switch (e->align) {
        case ALIGN_A1:   s += bx < by ? bx : by; break;
        case ALIGN_A2:   s += 0.5 * (bx + by); break;
        case ALIGN_A3:   s += 1.0 / (1.0 + acc); break;
        case ALIGN_GRID: s += (rows >= 2 || cols >= 2) ? 0 : 1; break;
        }
    }
    return s / (double)g->n;
}

static double t_nedge(const graph *g, const double *x)
{
    long i, j;
    double s = 0;
    for (i = 0; i < g->m; i++) {
        double ax = x[2 * g->ea[i]], ay = x[2 * g->ea[i] + 1];
        double ex = x[2 * g->eb[i]] - ax, ey = x[2 * g->eb[i] + 1] - ay;
        double l2 = ex * ex + ey * ey;
        if (l2 <= 0) continue;
        for (j = 0; j < g->n; j++) {
            double px, py, t, qx, qy, dd, r;
            if (j == g->ea[i] || j == g->eb[i]) continue;
            px = x[2 * j] - ax; py = x[2 * j + 1] - ay;
            t = (px * ex + py * ey) / l2;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            qx = px - t * ex; qy = py - t * ey;
            dd = sqrt(qx * qx + qy * qy);
            r = (g->w[j] + g->h[j]) / 4;
            if (r > 0 && dd < r) s += (r - dd) * (r - dd) / (r * r);
        }
    }
    return s / (double)g->m;
}

double term_value(int k, const graph *g, const double *x, const energy_spec *e)
{
    switch (k) {
    case TERM_C: return t_crossings(g, x);
    case TERM_O: return t_overlap(g, x);
    case TERM_L: return t_length(g, x);
    case TERM_S: return t_stress(g, x);
    case TERM_R: return t_orth(g, x);
    case TERM_A: return t_align(g, x, e);
    case TERM_N: return t_nedge(g, x);
    }
    return 0;
}

double energy(const graph *g, const double *x, const energy_spec *e, double t[NTERMS])
{
    int k;
    double s = 0;
    for (k = 0; k < NTERMS; k++) {
        double v = term_value(k, g, x, e);
        if (t) t[k] = v;
        s += e->w[k] * v;
    }
    return s;
}
