/* corpus.c -- the reader. See corpus.h for the format and the refusals.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE. */
#include "corpus.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int fail(char *err, size_t len, const char *msg, const char *id, long line)
{
    snprintf(err, len, "line %ld, graph %s: %s", line, id[0] ? id : "?", msg);
    return -1;
}

static void graph_free(graph *g)
{
    free(g->x); free(g->w); free(g->h); free(g->ea); free(g->eb); free(g->dist);
}

void corpus_free(graph *gs, long k)
{
    long i;
    for (i = 0; i < k; i++) graph_free(&gs[i]);
    free(gs);
}

/* Reads one non-comment line into BUF; returns 0 at end of file. */
static int next_line(FILE *f, char *buf, size_t len, long *line)
{
    while (fgets(buf, (int)len, f)) {
        (*line)++;
        if (buf[0] != '#' && buf[0] != '\n' && buf[0] != '\r') return 1;
    }
    return 0;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

/* Rescale, the reference lengths, and the distances. Returns an error message or NULL. */
static const char *finish(graph *g)
{
    long i, j, head, tail, *queue;
    double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300, s, a, b, *len;
    for (i = 0; i < g->n; i++) {
        if (g->x[2 * i] < minx) minx = g->x[2 * i];
        if (g->x[2 * i] > maxx) maxx = g->x[2 * i];
        if (g->x[2 * i + 1] < miny) miny = g->x[2 * i + 1];
        if (g->x[2 * i + 1] > maxy) maxy = g->x[2 * i + 1];
    }
    s = maxx - minx > maxy - miny ? maxx - minx : maxy - miny;
    if (!(s > 0)) return "zero extent";
    g->scale = s;
    for (i = 0; i < g->n; i++) {
        g->x[2 * i] = (g->x[2 * i] - minx) / s;
        g->x[2 * i + 1] = (g->x[2 * i + 1] - miny) / s;
        g->w[i] /= s;
        g->h[i] /= s;
    }
    len = malloc((size_t)g->m * sizeof *len);
    if (!len) return "out of memory";
    for (i = 0; i < g->m; i++) {
        double dx = g->x[2 * g->ea[i]] - g->x[2 * g->eb[i]];
        double dy = g->x[2 * g->ea[i] + 1] - g->x[2 * g->eb[i] + 1];
        len[i] = sqrt(dx * dx + dy * dy);
    }
    qsort(len, (size_t)g->m, sizeof *len, cmp_double);
    g->L_median = len[g->m / 2];
    a = b = 0;
    for (i = 0; i < g->m; i++) { a += len[i]; b += len[i] * len[i]; }
    g->L_len = a > 0 ? b / a : 0;
    free(len);
    if (!(g->L_median > 0)) return "median edge length is zero";

    g->dist = malloc((size_t)(g->n * g->n) * sizeof *g->dist);
    queue = malloc((size_t)g->n * sizeof *queue);
    if (!g->dist || !queue) { free(queue); return "out of memory"; }
    for (i = 0; i < g->n * g->n; i++) g->dist[i] = -1;
    for (i = 0; i < g->n; i++) {
        int *d = g->dist + i * g->n;
        d[i] = 0; head = tail = 0; queue[tail++] = i;
        while (head < tail) {
            long u = queue[head++];
            for (j = 0; j < g->m; j++) {
                long v = g->ea[j] == u ? g->eb[j] : g->eb[j] == u ? g->ea[j] : -1;
                if (v >= 0 && d[v] < 0) { d[v] = d[u] + 1; queue[tail++] = v; }
            }
        }
        if (tail < g->n) { free(queue); return "not connected"; }
    }
    free(queue);
    a = b = 0;
    for (i = 0; i < g->n; i++)
        for (j = i + 1; j < g->n; j++) {
            double dx = g->x[2 * i] - g->x[2 * j], dy = g->x[2 * i + 1] - g->x[2 * j + 1];
            double q = sqrt(dx * dx + dy * dy) / (double)g->dist[i * g->n + j];
            a += q; b += q * q;
        }
    g->L_stress = b / a;
    return NULL;
}

long corpus_read(FILE *f, graph **out, char *err, size_t errlen)
{
    char buf[512], id[128] = "";
    long line = 0, k = 0, cap = 0;
    graph *gs = NULL;
    err[0] = 0;
    while (next_line(f, buf, sizeof buf, &line)) {
        graph g;
        long n, m, i;
        const char *msg;
        if (sscanf(buf, "G %127s %ld %ld", id, &n, &m) != 3)
            { corpus_free(gs, k); return fail(err, errlen, "expected 'G id n m'", id, line); }
        if (n < 2) { corpus_free(gs, k); return fail(err, errlen, "need at least two nodes", id, line); }
        if (m < 1) { corpus_free(gs, k); return fail(err, errlen, "need at least one edge", id, line); }
        memset(&g, 0, sizeof g);
        strcpy(g.id, id);
        g.n = n; g.m = m;
        g.x = malloc((size_t)(2 * n) * sizeof *g.x);
        g.w = malloc((size_t)n * sizeof *g.w);
        g.h = malloc((size_t)n * sizeof *g.h);
        g.ea = malloc((size_t)m * sizeof *g.ea);
        g.eb = malloc((size_t)m * sizeof *g.eb);
        if (!g.x || !g.w || !g.h || !g.ea || !g.eb)
            { graph_free(&g); corpus_free(gs, k); return fail(err, errlen, "out of memory", id, line); }
        for (i = 0; i < n; i++) {
            if (!next_line(f, buf, sizeof buf, &line) ||
                sscanf(buf, "V %lf %lf %lf %lf", &g.x[2 * i], &g.x[2 * i + 1], &g.w[i], &g.h[i]) != 4)
                { graph_free(&g); corpus_free(gs, k); return fail(err, errlen, "expected 'V x y w h'", id, line); }
            if (g.w[i] < 0 || g.h[i] < 0)
                { graph_free(&g); corpus_free(gs, k); return fail(err, errlen, "negative box size", id, line); }
        }
        for (i = 0; i < m; i++) {
            long a, b;
            if (!next_line(f, buf, sizeof buf, &line) || sscanf(buf, "E %ld %ld", &a, &b) != 2)
                { graph_free(&g); corpus_free(gs, k); return fail(err, errlen, "expected 'E a b'", id, line); }
            if (a < 0 || b < 0 || a >= n || b >= n || a == b)
                { graph_free(&g); corpus_free(gs, k); return fail(err, errlen, "bad edge endpoint", id, line); }
            g.ea[i] = a < b ? a : b;
            g.eb[i] = a < b ? b : a;
        }
        if ((msg = finish(&g)) != NULL)
            { graph_free(&g); corpus_free(gs, k); return fail(err, errlen, msg, id, line); }
        if (k == cap) {
            graph *more;
            cap = cap ? 2 * cap : 64;
            more = realloc(gs, (size_t)cap * sizeof *gs);
            if (!more) { graph_free(&g); corpus_free(gs, k); return fail(err, errlen, "out of memory", id, line); }
            gs = more;
        }
        gs[k++] = g;
    }
    *out = gs;
    return k;
}
