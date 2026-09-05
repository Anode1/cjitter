/* erd_movie.c -- the search, watched: climb settling the migration's ten tables into the
 * frozen diagram, from the first random draw to the pinned layout.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * Keyframes are the improvements of the best layout so far, at the shipped budget and seed,
 * with cjitter_tuning.block set to 2 so each proposal moves one table.
 * The search moves in jumps; the frames between keyframes are linear interpolation, so the
 * film is a smoothed replay of the real trajectory, not the trajectory itself. Connectors
 * are re-routed at every frame, so the reader watches the routing give way
 * and settle along with the tables.
 *
 *     example/erd/erd_movie [dir]      # writes dir/frame0000.svg ... (dir must exist)
 *     make movie                       # builds the animated gif the README embeds
 */
#define main erd_main
#include "erd.c"
#undef main

#define MAXKEY 512
#define STEP   44.0   /* canvas units of the largest table move per film frame */
#define MINVIS 6.0    /* canvas units below which an improvement is invisible at 640px and
                       * merges into the next glide instead of popping in one frame */
#define MAXF   105    /* frame budget; tweens scale down proportionally to fit. Sized for the
                       * reader, not the search: at 33 frames a second this is about four
                       * seconds of motion, and a film nobody watches to the end shows nothing.
                       * The floor is one frame per kept improvement, so a run with many more
                       * improvements than this gets a jumpier film rather than a longer one. */

#define BAND   110    /* the caption's height above the canvas, in canvas units */

static struct {
    long   nkey, calls;
    double best;
    double key[MAXKEY][64];
    Terms  kt[MAXKEY];    /* what each keyframe's score is made of, for the caption */
    long   kev[MAXKEY];
} T;

/* The fitness the search sees, and the trace: the same terms score() sums, kept apart so
 * the caption can say what the number is. */
static double traced(const double *x, void *ctx)
{
    Erd *g = ctx;
    Terms t;
    double f;
    terms(g, x, &t);
    f = total_of(g, &t);
    if (T.calls == 0 || f < T.best) {
        T.best = f;
        if (T.nkey == MAXKEY) {
            long i;
            for (i = 1; i < MAXKEY / 2; i++) {
                memcpy(T.key[i], T.key[2*i], sizeof T.key[i]);
                T.kt[i] = T.kt[2*i];
                T.kev[i] = T.kev[2*i];
            }
            T.nkey = MAXKEY / 2;
        }
        memcpy(T.key[T.nkey], x, (size_t)(2 * (g->n - g->nfixed)) * sizeof *x);
        T.kt[T.nkey] = t;
        T.kev[T.nkey] = T.calls;
        T.nkey++;
    }
    T.calls++;
    return f;
}

/* One frame: the caption names the keyframe the glide leaves, its score and the three terms
 * the score is made of, so the reader watches crossings and penetration fall rather than a
 * number. G is written because the panel reroutes the migration's edges. */
static void frame(const char *dir, long idx, Erd *g, const double *v,
                  long eval, const Terms *t)
{
    char path[512];
    snprintf(path, sizeof path, "%s/frame%04ld.svg", dir, idx);
    if (!freopen(path, "w", stdout)) { perror(path); exit(1); }
    printf("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g' "
           "font-family='sans-serif'>\n", g->cw + 10, g->ch + BAND);
    printf("<rect width='%g' height='%g' fill='white'/>\n", g->cw + 10, g->ch + BAND);
    printf("<text x='%g' y='40' text-anchor='middle' font-size='32' fill='#444'>"
           "climb, one table per proposal, seed 1: evaluation %ld, score %.6g</text>\n",
           (g->cw + 10) / 2, eval, total_of(g, t));
    printf("<text x='%g' y='90' text-anchor='middle' font-size='40' fill='#111'>"
           "%ld crossings, %.0f units of connector under a table, length %.0f</text>\n",
           (g->cw + 10) / 2, t->crs, t->pen, t->len);
    printf("<g transform='translate(0,%d)'>\n", BAND);
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
    cjitter_tuning tun;
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
    frozen_part(&g);

    nv = 2 * nnew;
    for (i = 0; i < nv; i += 2) {
        lo[i] = 0; hi[i] = g.cw;
        lo[i+1] = 0; hi[i+1] = g.ch;
    }
    p.n = nv; p.lo = lo; p.hi = hi; p.fitness = traced; p.repair = legal; p.ctx = &g;
    p.start = NULL;
    b.evals = EVALS; b.seed = 1;
    /* One table per proposal. The film exists to show the search working, and at the default
     * block every proposal displaces all ten tables at once, so the reader watches the whole
     * migration teleport on each of the 47 acceptances instead of tables finding their
     * neighbours. The tuning comment in cjitter.h has the reason it is the better search too. */
    tun = cjitter_tuning_default(nv);
    tun.jitter = JITTER; tun.pop = POP;
    tun.block = 2;

    r.x = x;
    if (cjitter_run_tuned("climb", &p, &b, &tun, &r) != 0) {
        fprintf(stderr, "erd_movie: run failed\n");
        return 1;
    }

    /* Every improvement is a keyframe, except those too small to see: an improvement whose
     * largest table move from the last kept keyframe is under MINVIS merges into the next
     * glide, so the film's tail settles instead of stuttering, one invisible pop per tiny
     * refinement. The tween count per kept gap is proportional to the largest single-table
     * move in it, so a long glide gets many small steps and nothing ever teleports; if the
     * total overruns the frame budget every gap scales down proportionally, and a visible
     * move keeps at least one frame. */
    {
        static long tw[MAXKEY], kept[MAXKEY];
        long total = 0, nkept = 0, gap;
        kept[nkept++] = 0;
        for (key = 1; key < T.nkey; key++) {
            double dmax = 0;
            for (k = 0; k < nv; k += 2) {
                double dx = T.key[key][k]   - T.key[kept[nkept-1]][k];
                double dy = T.key[key][k+1] - T.key[kept[nkept-1]][k+1];
                double d = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
                if (d > dmax) dmax = d;
            }
            if (dmax >= MINVIS || key == T.nkey - 1) kept[nkept++] = key;
        }
        for (gap = 0; gap + 1 < nkept; gap++) {
            double dmax = 0;
            for (k = 0; k < nv; k += 2) {
                double dx = T.key[kept[gap+1]][k]   - T.key[kept[gap]][k];
                double dy = T.key[kept[gap+1]][k+1] - T.key[kept[gap]][k+1];
                double d = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
                if (d > dmax) dmax = d;
            }
            tw[gap] = (long)(dmax / STEP) + 1;
            total += tw[gap];
        }
        if (total > MAXF)
            for (gap = 0; gap + 1 < nkept; gap++) {
                tw[gap] = tw[gap] * MAXF / total;
                if (tw[gap] < 1) tw[gap] = 1;
            }
        for (gap = 0; gap + 1 < nkept; gap++)
            for (t = 0; t < tw[gap]; t++) {
                double u = (double)t / (double)tw[gap];
                for (k = 0; k < nv; k++)
                    v[k] = T.key[kept[gap]][k]
                         + u * (T.key[kept[gap+1]][k] - T.key[kept[gap]][k]);
                frame(dir, nf++, &g, v, T.kev[kept[gap]], &T.kt[kept[gap]]);
            }
    }
    frame(dir, nf++, &g, T.key[T.nkey - 1], T.kev[T.nkey - 1], &T.kt[T.nkey - 1]);
    fprintf(stderr, "%ld frames from %ld improvements, final %.6g\n",
            nf, T.nkey, r.best);
    return 0;
}
