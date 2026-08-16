/* labels_movie.c -- the 2001 debugging view, recreated: watch annealing settle the labels.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * The original label placer painted every step into a double-buffered applet canvas, and
 * watching the layout settle, labels retracting from their neighbours until nothing
 * overlapped, was how the method was understood. That placer moved one label at a time; the
 * library's anneal moves every coordinate per proposal and plateaus on this problem, so the
 * film follows the method that ends clean the way the memory does, the GA, one frame per
 * improvement of the best layout so far. Labels are drawn half-transparent so an overlap
 * shows as a darker patch, which is the objective made visible.
 *
 *     example/labels_movie [dir]        # writes dir/frame000.svg ... (dir must exist)
 *     make movie                        # builds the animated gif the README embeds
 */
#define main labels_main
#include "labels.c"
#undef main

#define MAXFRAMES 256

static struct {
    Labels *L;
    long    calls, nframes;
    double  best;
    long    evals[MAXFRAMES];
    double  frame[MAXFRAMES][2 * DEF_LABELS];
    double  fscore[MAXFRAMES];
} M;

static double filmed(const double *x, void *ctx)
{
    double f = overlap(x, ctx);
    if (M.calls == 0 || f < M.best) {
        M.best = f;
        if (M.nframes == MAXFRAMES) {
            /* full: keep every other frame and carry on, the ending matters most */
            long i;
            for (i = 1; i < MAXFRAMES / 2; i++) {
                memcpy(M.frame[i], M.frame[2*i], sizeof M.frame[i]);
                M.fscore[i] = M.fscore[2*i];
                M.evals[i] = M.evals[2*i];
            }
            M.nframes = MAXFRAMES / 2;
        }
        memcpy(M.frame[M.nframes], x, (size_t)(2 * M.L->n) * sizeof *x);
        M.fscore[M.nframes] = f;
        M.evals[M.nframes] = M.calls;
        M.nframes++;
    }
    M.calls++;
    return f;
}

static void write_frame(const char *dir, long i, const Labels *L,
                        const double *x, double f, long evals)
{
    char path[512];
    FILE *fp;
    long j;
    snprintf(path, sizeof path, "%s/frame%03ld.svg", dir, i);
    fp = fopen(path, "w");
    if (!fp) { perror(path); exit(1); }
    fprintf(fp, "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g' "
                "font-family='sans-serif'>\n", L->cw * 10, L->ch * 10 + 40);
    fprintf(fp, "<rect width='%g' height='%g' fill='white'/>\n", L->cw * 10, L->ch * 10 + 40);
    fprintf(fp, "<rect x='0' y='40' width='%g' height='%g' fill='#fafafa' stroke='#ccc'/>\n",
            L->cw * 10, L->ch * 10);
    fprintf(fp, "<text x='8' y='28' font-size='24' fill='#333'>"
                "ga, evaluation %ld, overlap %.1f</text>\n", evals, f);
    for (j = 0; j < L->n; j++)
        fprintf(fp, "<rect x='%g' y='%g' width='%g' height='%g' rx='3' "
                    "fill='#fc3' fill-opacity='0.55' stroke='#963'/>\n",
                (x[2*j] - L->w/2) * 10, 40 + (x[2*j+1] - L->h/2) * 10,
                L->w * 10, L->h * 10);
    fprintf(fp, "</svg>\n");
    fclose(fp);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "movie";
    Labels L;
    cjitter_problem p;
    cjitter_budget b;
    cjitter_result r;
    double lo[2 * DEF_LABELS], hi[2 * DEF_LABELS], x[2 * DEF_LABELS];
    long i;

    L.n = DEF_LABELS; L.w = LABEL_W; L.h = LABEL_H; L.cw = AREA_W; L.ch = AREA_H;
    for (i = 0; i < L.n; i++) {
        lo[2*i] = 0; hi[2*i] = L.cw;
        lo[2*i+1] = 0; hi[2*i+1] = L.ch;
    }
    p.n = 2 * L.n; p.lo = lo; p.hi = hi;
    p.fitness = filmed; p.repair = inside; p.ctx = &L;
    b.evals = DEF_EVALS; b.seed = 1; b.jitter = JITTER; b.pop = POP;
    M.L = &L;

    r.x = x;
    if (cjitter_run("ga", &p, &b, &r) != 0) {
        fprintf(stderr, "labels_movie: run failed\n");
        return 1;
    }
    for (i = 0; i < M.nframes; i++)
        write_frame(dir, i, &L, M.frame[i], M.fscore[i], M.evals[i]);
    /* the settled answer, held as the final frame */
    write_frame(dir, M.nframes, &L, x, r.best, r.evals);
    printf("%ld frames in %s/, final overlap %.6g\n", M.nframes + 1, dir, r.best);
    return 0;
}
