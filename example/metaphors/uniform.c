/* uniform.c -- the uniform-on-the-box control for the metaphor audit.
 *
 * One cjitter_run("random") per (function, instance, run) at the top budget of
 * 10000 x dim, on the bbob suite through coco-experiment's evaluator. A fitness
 * wrapper counts evaluations and records the best error, f minus the problem's
 * optimum, at the release's seven budget factors, so the output pairs row for row
 * with the condensed table from the Zenodo release.
 *
 * Output, stdout: budget_factor,algname,fid,iid,dim,run,fx
 * The seed is declared here and nowhere else: fid*1000000 + iid*10000 + dim*100 + run.
 *
 * Build (from this directory, cjitter built with `make lib` at CJITTER):
 *   cc -std=c99 -ffp-contract=off -O2 -I coco/cocoex-c -I $CJITTER/c \
 *      -o uniform uniform.c coco/cocoex-c/coco.c $CJITTER/libcjitter.a -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coco.h"
#include "cjitter.h"

/* Defined in coco.c, absent from coco.h. */
double coco_problem_get_best_value(const coco_problem_t *problem);

static const long BFAC[7] = { 10, 50, 100, 500, 1000, 5000, 10000 };

typedef struct {
    coco_problem_t *prob;
    double          fopt;
    long            evals, dim, next;
    double          best;
    double          at[7];
} Ctx;

static double fit(const double *x, void *vc)
{
    Ctx *c = (Ctx *)vc;
    double y;
    coco_evaluate_function(c->prob, x, &y);
    c->evals++;
    if (y - c->fopt < c->best) c->best = y - c->fopt;
    while (c->next < 7 && c->evals == BFAC[c->next] * c->dim) {
        c->at[c->next] = c->best;
        c->next++;
    }
    return y;
}

int main(void)
{
    coco_suite_t   *suite;
    coco_problem_t *prob;
    double lo[40], hi[40], x[40];

    for (int j = 0; j < 40; j++) { lo[j] = -5.0; hi[j] = 5.0; }
    coco_set_log_level("warning");
    suite = coco_suite("bbob", "instances: 1-10",
                       "dimensions: 2,5,10,20 function_indices: 1-24");
    if (!suite) { fprintf(stderr, "no bbob suite\n"); return 1; }

    printf("budget_factor,algname,fid,iid,dim,run,fx\n");
    while ((prob = coco_suite_get_next_problem(suite, NULL)) != NULL) {
        /* the id is "bbob_f001_i01_d02" */
        const char *id = coco_problem_get_id(prob);
        long fid, iid, dim;
        if (sscanf(id, "bbob_f%ld_i%ld_d%ld", &fid, &iid, &dim) != 3) {
            fprintf(stderr, "unparsed id %s\n", id);
            return 1;
        }
        for (long run = 1; run <= 5; run++) {
            Ctx c = { prob, coco_problem_get_best_value(prob), 0, dim, 0, 0.0, {0} };
            cjitter_problem p = { dim, lo, hi, fit, NULL, &c, NULL };
            cjitter_budget  b = { 10000 * dim, 0 };
            cjitter_result  r; memset(&r, 0, sizeof r); r.x = x;
            c.best = 1e300;
            b.seed = (uint32_t)(fid * 1000000 + iid * 10000 + dim * 100 + run);
            if (cjitter_run("random", &p, &b, &r) != 0) {
                fprintf(stderr, "run failed %s run %ld\n", id, run);
                return 1;
            }
            if (c.evals != b.evals || c.next != 7) {
                fprintf(stderr, "budget mismatch %s run %ld: %ld evals, %ld marks\n",
                        id, run, c.evals, c.next);
                return 1;
            }
            for (int k = 0; k < 7; k++)
                printf("%ld,cjitter_uniform,%ld,%ld,%ld,%ld,%.17g\n",
                       BFAC[k], fid, iid, dim, run, c.at[k]);
        }
    }
    coco_suite_free(suite);
    return 0;
}
