/* cjitter.h -- four stochastic searches behind one interface, with uniform sampling as the
 * control.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * You supply a fitness function over a box of real variables, lower being better, and a budget
 * in evaluations. Four methods spend that budget:
 *
 *     random    draw uniformly from the box. The CONTROL, and the reason this library exists.
 *     climb     propose a jittered neighbour, keep it if it is better, restart when stuck.
 *     anneal    the same, but accept a worse neighbour with a probability that decays.
 *     ga        a population, tournament selection, blend crossover, jittered mutation.
 *
 * cjitter_compare runs all four at the SAME budget over several seeds and reports which won and
 * whether the margin clears the spread across seeds. A search that cannot beat uniform sampling
 * at equal cost is a result worth having, and most libraries never measure it.
 *
 * Everything is deterministic given a seed. Nothing allocates outside cjitter_run.
 */
#ifndef CJITTER_H
#define CJITTER_H

#include <stddef.h>
#include <stdint.h>

/* Lower is better. CTX is yours, untouched. */
typedef double (*cjitter_fitness)(const double *x, void *ctx);

/* Optional. Called on every proposal before it is scored, so a hard constraint is enforced by
 * construction rather than by a penalty term: clamping cannot trade itself off against the
 * objective, and no infeasible point can ever be returned as the best. */
typedef void (*cjitter_repair)(double *x, void *ctx);

typedef struct {
    long             n;        /* variables */
    const double    *lo;       /* box, n values */
    const double    *hi;
    cjitter_fitness  fitness;
    cjitter_repair   repair;   /* may be NULL */
    void            *ctx;
} cjitter_problem;

typedef struct {
    long     evals;      /* the budget, spent by every method equally */
    uint32_t seed;
    double   jitter;     /* first move size, as a fraction of each variable's range */
    long     pop;        /* ga only; 0 takes a default */
} cjitter_budget;

/* The methods' internal constants. Every field taken as zero means the default printed beside
 * it, so a zeroed struct is exactly the shipped behaviour. A tuning is part of a result's
 * identity: two runs compare only if they share one. anneal_cool_ln is the natural log of the
 * temperature's total decay; pass a literal rather than log(x) if runs must reproduce across
 * platforms, because log is not correctly rounded and one ulp moves the trajectory. */
typedef struct {
    long   climb_patience;    /* rejections before the move size halves; 0: 40 + 10n     */
    double climb_shrink;      /* the halving; 0: 0.5                                     */
    double climb_restart_at;  /* scale/jitter ratio that restarts; 0: 1/64               */
    long   anneal_probes;     /* uphill probes setting the start temperature; 0: 20      */
    double anneal_cool_ln;    /* ln of the total temperature decay; 0: -6.907755278982137,
                                 which is ln 1e-3                                        */
    double anneal_move_decay; /* move-size fraction the cooling removes; 0: 0.9          */
    double ga_mutate;         /* mutation move size as a fraction of jitter; 0: 0.3      */
} cjitter_tuning;

typedef struct {
    double      best;    /* the fitness found */
    double     *x;       /* the point, n values, owned by the caller */
    long        evals;   /* actually spent */
    long        restarts;
    const char *method;
} cjitter_result;

/* METHOD is "random", "climb", "anneal" or "ga". OUT->x must have room for n doubles.
 * Returns 0, or -1 on a bad argument or an allocation failure. cjitter_run takes the default
 * tuning; the _tuned variant takes an explicit one, where NULL means the same defaults and a
 * negative field is refused. */
int cjitter_run(const char *method, const cjitter_problem *p, const cjitter_budget *b,
                cjitter_result *out);
int cjitter_run_tuned(const char *method, const cjitter_problem *p, const cjitter_budget *b,
                      const cjitter_tuning *t, cjitter_result *out);

/* All four, SEEDS times each, at one budget. Prints a table to STREAM: the median best per
 * method, the spread over seeds, and which of them beat the control by more than that spread.
 * Returns 0, or -1 on a bad argument or an allocation failure. The _tuned variant as above. */
int cjitter_compare(const cjitter_problem *p, const cjitter_budget *b, long seeds, void *stream);
int cjitter_compare_tuned(const cjitter_problem *p, const cjitter_budget *b,
                          const cjitter_tuning *t, long seeds, void *stream);

/* The four names, NULL-terminated, in the order compare reports them. */
extern const char *const cjitter_methods[];

#endif /* CJITTER_H */
