/* audit.c -- the paired audit's verdicts, from cjitter's own two tests.
 *
 * Reads the flat export of the release's fixed-budget runs (export_flat.py) and a
 * control, pairs every implementation with the control per (function, instance, run),
 * counts strict wins and losses (a tie counts for neither side), and prints, per
 * (dimension, budget factor), the exact one-sided sign p in each direction with Holm
 * across the family: the non-baseline implementations form one family, the baselines
 * beside the control their own of eleven, so neither dilutes the other. Budget factor
 * 10 is printed without correction or verdict: below most initial population sizes the
 * compared value is the shared-seed initial sample, and the pre-registration excludes
 * it from every family.
 *
 * The control is the release's own RandomSearch by default; --control-csv <file>
 * substitutes an external control in uniform.c's output format, cjitter_uniform.
 *
 * Usage: audit fb_algs.txt fb_flat.txt [--control-csv uniform_control.csv] > verdicts.tsv
 * The per-cell summary goes to stderr, the per-implementation rows to stdout.
 *
 * Build: cc -std=c99 -ffp-contract=off -O2 -I $CJITTER/c -o audit audit.c \
 *        $CJITTER/libcjitter.a -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjitter.h"

#define MAXALG 1024
#define NBF 7
#define NDIM 4

static const long BFAC[NBF] = { 10, 50, 100, 500, 1000, 5000, 10000 };
static const long DIMS[NDIM] = { 2, 5, 10, 20 };

static char g_lib[MAXALG][32], g_name[MAXALG][64];
static int  g_base[MAXALG];
static long g_nalg;

/* control value per (bf, dim, fid, iid, run); have flags beside it */
static double g_ctl[NBF][NDIM][25][11][6];
static char   g_have[NBF][NDIM][25][11][6];

static long g_w[MAXALG][NBF][NDIM], g_l[MAXALG][NBF][NDIM], g_n[MAXALG][NBF][NDIM];

static int bf_index(long b)
{
    for (int i = 0; i < NBF; i++) if (BFAC[i] == b) return i;
    return -1;
}

static int dim_index(long d)
{
    for (int i = 0; i < NDIM; i++) if (DIMS[i] == d) return i;
    return -1;
}

static void read_algs(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    while (g_nalg < MAXALG &&
           fscanf(f, "%ld %31s %63s %d", &(long){0}, g_lib[g_nalg], g_name[g_nalg],
                  &g_base[g_nalg]) == 4)
        g_nalg++;
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *ctl_csv = NULL;
    long ctl_id = -1;
    FILE *f;

    if (argc < 3) { fprintf(stderr, "usage: audit algs flat [--control-csv file]\n"); return 1; }
    if (argc >= 5 && strcmp(argv[3], "--control-csv") == 0) ctl_csv = argv[4];

    read_algs(argv[1]);
    for (long i = 0; i < g_nalg; i++)
        if (g_base[i] && strcmp(g_name[i], "RandomSearch") == 0) ctl_id = i;
    if (ctl_id < 0) { fprintf(stderr, "no RandomSearch in %s\n", argv[1]); return 1; }

    /* the control: either RandomSearch's rows from the flat file, or the external csv */
    if (ctl_csv) {
        char head[256];
        long b, fid, iid, d, run;
        double fx;
        f = fopen(ctl_csv, "r");
        if (!f) { perror(ctl_csv); return 1; }
        if (!fgets(head, sizeof head, f)) { fprintf(stderr, "empty %s\n", ctl_csv); return 1; }
        while (fscanf(f, "%ld,cjitter_uniform,%ld,%ld,%ld,%ld,%lf",
                      &b, &fid, &iid, &d, &run, &fx) == 6) {
            int bi = bf_index(b), di = dim_index(d);
            if (bi < 0 || di < 0 || fid < 1 || fid > 24 || iid < 1 || iid > 10 ||
                run < 1 || run > 5) { fprintf(stderr, "bad control row\n"); return 1; }
            g_ctl[bi][di][fid][iid][run] = fx;
            g_have[bi][di][fid][iid][run] = 1;
        }
        fclose(f);
    }

    f = fopen(argv[2], "r");
    if (!f) { perror(argv[2]); return 1; }
    if (!ctl_csv) {
        long a, bi, di, fid, iid, run;
        double fx;
        while (fscanf(f, "%ld %ld %ld %ld %ld %ld %lf",
                      &a, &bi, &di, &fid, &iid, &run, &fx) == 7)
            if (a == ctl_id) {
                g_ctl[bi][di][fid][iid][run] = fx;
                g_have[bi][di][fid][iid][run] = 1;
            }
        rewind(f);
    }

    {
        long a, bi, di, fid, iid, run;
        double fx;
        while (fscanf(f, "%ld %ld %ld %ld %ld %ld %lf",
                      &a, &bi, &di, &fid, &iid, &run, &fx) == 7) {
            if (a == ctl_id && !ctl_csv) continue;
            if (a < 0 || a >= g_nalg) { fprintf(stderr, "alg id %ld out of range\n", a); return 1; }
            if (!g_have[bi][di][fid][iid][run]) continue;
            g_n[a][bi][di]++;
            if (fx < g_ctl[bi][di][fid][iid][run]) g_w[a][bi][di]++;
            else if (fx > g_ctl[bi][di][fid][iid][run]) g_l[a][bi][di]++;
        }
        fclose(f);
    }

    printf("lib\talg\tdim\tbudget_factor\twins\tlosses\tpairs\tp_better\tholm_better\t"
           "p_worse\tholm_worse\tverdict\n");
    for (int di = 0; di < NDIM; di++)
        for (int bi = 0; bi < NBF; bi++) {
            double pb[MAXALG], pw[MAXALG], ab[MAXALG], aw[MAXALG];
            long id[MAXALG], nf;
            long better = 0, worse = 0, notshown = 0;
            for (int fam = 0; fam < 2; fam++) {          /* 0 metaphors, 1 baselines */
                nf = 0;
                for (long a = 0; a < g_nalg; a++) {
                    if (a == ctl_id || g_base[a] != fam) continue;
                    id[nf] = a;
                    pb[nf] = cjitter_sign_p(g_w[a][bi][di], g_w[a][bi][di] + g_l[a][bi][di]);
                    pw[nf] = cjitter_sign_p(g_l[a][bi][di], g_w[a][bi][di] + g_l[a][bi][di]);
                    nf++;
                }
                if (nf == 0) continue;
                if (bi > 0) {
                    if (cjitter_holm(pb, nf, ab) != 0 || cjitter_holm(pw, nf, aw) != 0) {
                        fprintf(stderr, "holm failed\n");
                        return 1;
                    }
                }
                for (long k = 0; k < nf; k++) {
                    long a = id[k];
                    const char *v;
                    if (bi == 0) v = "descriptive";
                    else if (g_w[a][bi][di] + g_l[a][bi][di] == 0) v = "no pairs";
                    else if (ab[k] <= 0.05) v = "better";
                    else if (aw[k] <= 0.05) v = "worse";
                    else v = "not shown";
                    if (fam == 0 && bi > 0 && strcmp(v, "no pairs") != 0) {
                        if (!strcmp(v, "better")) better++;
                        else if (!strcmp(v, "worse")) worse++;
                        else notshown++;
                    }
                    printf("%s\t%s\t%ld\t%ld\t%ld\t%ld\t%ld\t%.6g\t%.6g\t%.6g\t%.6g\t%s\n",
                           g_lib[a], g_name[a], DIMS[di], BFAC[bi],
                           g_w[a][bi][di], g_l[a][bi][di], g_n[a][bi][di],
                           pb[k], bi > 0 ? ab[k] : -1.0, pw[k], bi > 0 ? aw[k] : -1.0, v);
                }
            }
            if (bi > 0)
                fprintf(stderr, "D=%-2ld bf=%-5ld  better %3ld  not shown %3ld  worse %3ld\n",
                        DIMS[di], BFAC[bi], better, notshown, worse);
        }
    return 0;
}
