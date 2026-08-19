/* cjitter.h -- four stochastic searches behind one interface, with uniform sampling as the
 * control.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * You supply a fitness function over a box of real variables, lower being better, and a budget
 * in evaluations. Four methods spend that budget:
 *
 *     random    draw uniformly from the box. The CONTROL.
 *     climb     propose a jittered neighbour, keep it if it is better, restart when stuck.
 *     anneal    the same, but accept a worse neighbour with a probability that decays.
 *     ga        a population, tournament selection, blend crossover, jittered mutation.
 *
 * cjitter_compare runs all four at the SAME budget on the SAME seeds and prints, per method,
 * the median, the range over seeds, the per-seed wins against the control, and the exact
 * one-sided sign-test probability of that many wins under a fair coin. The verdict is "better"
 * only when that probability is at or under 5%; anything else prints "not shown", which is a
 * failure to demonstrate improvement and says nothing about equality. A search that cannot
 * beat uniform sampling at equal cost is a result worth having, and most libraries never
 * measure it.
 *
 * Everything is deterministic given a seed, on every platform: the trajectories use integer
 * arithmetic, +-*, fabs and sqrt, the calls IEEE 754 computes exactly or rounds correctly,
 * and no other libm. No search step allocates; each entry point allocates once and frees on
 * exit.
 */
#ifndef CJITTER_H
#define CJITTER_H

#include <stddef.h>
#include <stdint.h>

/* The library's version, bumped only when the interface or a method's trajectory changes:
 * either one changes what a seed reproduces. */
#define CJITTER_VERSION "0.10.0"
#define CJITTER_VERSION_MAJOR 0
#define CJITTER_VERSION_MINOR 10
#define CJITTER_VERSION_PATCH 0

/* Lower is better. CTX is yours, untouched. */
typedef double (*cjitter_fitness)(const double *x, void *ctx);

/* Optional. Called on every proposal before it is scored, so a hard constraint is enforced by
 * construction rather than by a penalty term. The box is re-applied after it runs, so a repair
 * cannot move a point outside the box even by accident. cjitter.h owns this argument; the
 * examples only point here. */
typedef void (*cjitter_repair)(double *x, void *ctx);

typedef struct {
    long             n;        /* variables */
    const double    *lo;       /* box, n values, lo[j] <= hi[j] */
    const double    *hi;
    cjitter_fitness  fitness;
    cjitter_repair   repair;   /* may be NULL */
    void            *ctx;
} cjitter_problem;

typedef struct {
    long     evals;      /* the budget, spent by every method exactly */
    uint32_t seed;       /* run seed; compare uses it as the base of its seed panel */
    double   jitter;     /* first move size, as a fraction of each variable's range;
                            0 takes 0.1, negative is refused */
    long     pop;        /* ga only; 0 takes 30, negative is refused */
} cjitter_budget;

/* The methods' internal constants. Take the defaults from cjitter_tuning_default and change
 * the fields you mean to change; every field is read literally, so ga_mutate = 0 is a real
 * mutation ablation, not a default. A field outside its stated range is refused. A tuning is
 * part of a result's identity: two runs compare only if they share one.
 *
 * anneal_cool_ln is the natural log of the temperature's total decay; pass a literal rather
 * than log(x) if runs must reproduce across platforms, because log is not correctly rounded
 * and one ulp moves the trajectory.
 *
 * block is how many variables one proposal may move. The blocks tile the vector in order and
 * cycle, one per proposal, so climb, anneal and the ga's mutation all step through the
 * problem a block at a time. Anything at or above n moves the whole vector, which is the
 * default and what every result in README.md was measured at.
 *
 * Set it to the width of one object -- 2 for a point in the plane -- when the objective is a
 * sum over objects that interact weakly. A whole-vector proposal that improves one object and
 * spoils another is rejected for the spoiling, so on a nearly separable problem a search that
 * moves everything at once can stall where one that moves a block at a time walks straight
 * down. That is the mechanism this library is named after and it can be worth an order of
 * magnitude in budget; it is not the default only because the default must not change what an
 * existing seed reproduces. It is not free: on a genuinely coupled objective, where the good
 * moves are the ones that adjust several objects together, a small block cannot express them.
 * Measure it against block = n with cjitter_compare, the way you would measure any other
 * method. */
typedef struct {
    long   climb_patience;    /* >= 1; rejections before the move size shrinks              */
    double climb_shrink;      /* in (0, 1); the shrink factor                               */
    double climb_restart_at;  /* in [0, 1]; scale/jitter ratio that restarts; 0 never does  */
    long   anneal_probes;     /* >= 1; uphill probes setting the start temperature          */
    double anneal_cool_ln;    /* <= 0; ln of the total temperature decay                    */
    double anneal_move_decay; /* in [0, 1]; move-size fraction the cooling removes          */
    double ga_mutate;         /* >= 0; mutation move size as a fraction of jitter           */
    double ga_mutate_decay;   /* in [0, 1]; mutation-size fraction removed over the run     */
    long   block;             /* >= 1; variables one proposal moves; n or more moves all    */
} cjitter_tuning;

/* The shipped constants: patience 40 + 10n, shrink 0.5, restart at 1/64, 20 probes,
 * cooling ln 1e-3, move decay 0.9, mutation 0.3 of jitter decaying by 0.9, block n. N is the
 * problem's variable count, which the patience and block defaults scale with, so a tuning
 * taken for one n and used at another is a different tuning: block would no longer cover the
 * vector. */
cjitter_tuning cjitter_tuning_default(long n);

typedef struct {
    double      best;    /* the fitness found */
    double     *x;       /* the point, n values, owned by the caller */
    long        evals;   /* actually spent */
    long        restarts;
    const char *method;  /* points into cjitter_methods: static storage, never freed */
} cjitter_result;

/* METHOD is "random", "climb", "anneal" or "ga"; NULL or "auto" takes the default, currently
 * climb, tied to CJITTER_VERSION because changing it changes what a seed reproduces. That
 * choice comes from the eight-migration layout benchmark, where climb separates from the
 * control on seven of eight instances and the ga does not separate at all; it is not the
 * ranking of the two examples in README.md, where the ga's median wins at the default block.
 * A default is a guess about somebody else's problem, so measure yours with cjitter_compare
 * rather than inherit this one. OUT->x must have room for n doubles. Returns 0, or -1 on a bad argument or an allocation failure. cjitter_run
 * takes the default tuning; the _tuned variant takes an explicit one, where NULL means the
 * same defaults. */
int cjitter_run(const char *method, const cjitter_problem *p, const cjitter_budget *b,
                cjitter_result *out);
int cjitter_run_tuned(const char *method, const cjitter_problem *p, const cjitter_budget *b,
                      const cjitter_tuning *t, cjitter_result *out);

/* All four, SEEDS times each, at one budget, every method on the same seed panel derived
 * from b->seed, so the per-seed differences are paired. Prints the table described at the
 * top of this file to STREAM (a FILE*; NULL means stdout). Returns 0, or -1 on a bad
 * argument or an allocation failure, before anything is printed. SEEDS is at most 1000:
 * past that the exact tests' arithmetic stops being exact. */
int cjitter_compare(const cjitter_problem *p, const cjitter_budget *b, long seeds, void *stream);
int cjitter_compare_tuned(const cjitter_problem *p, const cjitter_budget *b,
                          const cjitter_tuning *t, long seeds, void *stream);

/* The four names, NULL-terminated, in the order compare reports them. */
extern const char *const cjitter_methods[];

#endif /* CJITTER_H */
