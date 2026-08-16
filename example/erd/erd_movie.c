/* erd_movie.c -- the search, watched: climb settling the migration's ten tables into the
 * frozen diagram, from the first random draw to the pinned layout.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * Keyframes are the improvements of the best layout so far, at the shipped budget and seed.
 * The search moves in jumps; the frames between keyframes are linear interpolation, so the
 * film is a smoothed replay of the real trajectory, not the trajectory itself, and says so
 * here. Connectors are re-routed at every frame, so the reader watches the routing give way
 * and settle along with the tables.
 *
 *     example/erd/erd_movie [dir]      # writes dir/frame0000.svg ... (dir must exist)
 *     make movie                       # builds the animated gif the README embeds
 */
#define main erd_main
#include "erd.c"
#undef main

#define MAXKEY 512
#define STEP   22.0   /* canvas units of the largest table move per film frame */
#define MAXF   420    /* frame budget; tweens scale down proportionally to fit */

static struct {
    long   nkey, calls;
    double best;
    double key[MAXKEY][64];
    double ksc[MAXKEY];
    long   kev[MAXKEY];
} T;

static double traced(const double *x, void *ctx)
{
    Erd *g = ctx;
    double f = score(x, ctx);
    if (T.calls == 0 || f < T.best) {
        T.best = f;
        if (T.nkey == MAXKEY) {
            long i;
            for (i = 1; i < MAXKEY / 2; i++) {
                memcpy(T.key[i], T.key[2*i], sizeof T.key[i]);
                T.ksc[i] = T.ksc[2*i];
                T.kev[i] = T.kev[2*i];
            }
            T.nkey = MAXKEY / 2;
        }
        memcpy(T.key[T.nkey], x, (size_t)(2 * (g->n - g->nfixed)) * sizeof *x);
        T.ksc[T.nkey] = f;
        T.kev[T.nkey] = T.calls;
        T.nkey++;
    }
    T.calls++;
    return f;
}

static void frame(const char *dir, long idx, const Erd *g, const double *v,
                  long eval, double sc)
{
    char path[512];
    snprintf(path, sizeof path, "%s/frame%04ld.svg", dir, idx);
    if (!freopen(path, "w", stdout)) { perror(path); exit(1); }
    printf("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g' "
           "font-family='sans-serif'>\n", g->cw + 10, g->ch + 90);
    printf("<rect width='%g' height='%g' fill='white'/>\n", g->cw + 10, g->ch + 90);
    printf("<text x='%g' y='52' text-anchor='middle' font-size='40' fill='#111'>"
           "climb, evaluation %ld, best %.6g</text>\n", (g->cw + 10) / 2, eval, sc);
    printf("<g transform='translate(0,80)'>\n");
    svg_panel(g, v, 5);
    printf("</g>\n</svg>\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "movie";
    static Erd g;
    cjitter_problem p;
    cjitter_budget b;
    cjitter_result r;
    double lo[64], hi[64], x[64], v[64];
    long i, k, nnew, nv, nf = 0, key, t;

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
    g.straight = 0;
    g.konst = frozen_part(&g);

    nv = 2 * nnew;
    for (i = 0; i < nv; i += 2) {
        lo[i] = 0; hi[i] = g.cw;
        lo[i+1] = 0; hi[i+1] = g.ch;
    }
    p.n = nv; p.lo = lo; p.hi = hi; p.fitness = traced; p.repair = legal; p.ctx = &g;
    b.evals = EVALS; b.seed = 1; b.jitter = JITTER; b.pop = POP;

    r.x = x;
    if (cjitter_run("climb", &p, &b, &r) != 0) {
        fprintf(stderr, "erd_movie: run failed\n");
        return 1;
    }

    /* Every improvement is a keyframe; the tween count per gap is proportional to the
     * largest single-table move in it, so a long glide gets many small steps and nothing
     * ever teleports. If the total overruns the frame budget, every gap scales down
     * proportionally, one frame minimum. */
    {
        static long tw[MAXKEY];
        long total = 0;
        for (key = 0; key + 1 < T.nkey; key++) {
            double dmax = 0;
            for (k = 0; k < nv; k += 2) {
                double dx = T.key[key+1][k]   - T.key[key][k];
                double dy = T.key[key+1][k+1] - T.key[key][k+1];
                double d = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
                if (d > dmax) dmax = d;
            }
            tw[key] = (long)(dmax / STEP) + 1;
            total += tw[key];
        }
        if (total > MAXF)
            for (key = 0; key + 1 < T.nkey; key++) {
                tw[key] = tw[key] * MAXF / total;
                if (tw[key] < 1) tw[key] = 1;
            }
        for (key = 0; key + 1 < T.nkey; key++)
            for (t = 0; t < tw[key]; t++) {
                double u = (double)t / (double)tw[key];
                for (k = 0; k < nv; k++)
                    v[k] = T.key[key][k] + u * (T.key[key+1][k] - T.key[key][k]);
                frame(dir, nf++, &g, v, T.kev[key], T.ksc[key]);
            }
    }
    frame(dir, nf++, &g, T.key[T.nkey - 1], T.kev[T.nkey - 1], T.ksc[T.nkey - 1]);
    fprintf(stderr, "%ld frames from %ld improvements, final %.6g\n",
            nf, T.nkey, r.best);
    return 0;
}
