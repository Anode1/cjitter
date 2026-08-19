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
#include <stdio.h>   /* FILE, for the compare stream */

/* The library's version, bumped only when the interface or a method's trajectory changes:
 * either one changes what a seed reproduces. */
#define CJITTER_VERSION "0.11.0"
#define CJITTER_VERSION_MAJOR 0
#define CJITTER_VERSION_MINOR 11
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

/* A budget is what a run may spend and where its randomness starts: the two things two runs
 * must share before their results compare at all. Everything about HOW the budget is spent,
 * the first move size and the ga's population included, is tuning, and moved there in 0.11.0:
 * a tuning comes from cjitter_tuning_default, so a field there is initialised for every
 * caller who follows the contract, where a budget is routinely filled field by field. */
typedef struct {
    long     evals;      /* the budget, spent by every method exactly */
    uint32_t seed;       /* run seed; compare uses it as the base of its seed panel */
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
 * method.
 *
 * verify buys an honest number out of a noisy objective; the cjitter_result comment says what
 * goes wrong without it. It lives here rather than in the budget for two reasons. A tuning is
 * already part of a result's identity, and two runs verified differently do not compare. And
 * a tuning must come from cjitter_tuning_default, where a budget is routinely filled field by
 * field, so a field added here is initialised for every caller who follows the contract and a
 * field added there would have been uninitialised for every caller who already exists.
 *
 * jitter and pop lived in the budget until 0.11.0 and moved here under the same reasoning:
 * they are part of how a method spends, so they are part of a result's identity. jitter is
 * the first move size as a fraction of each variable's range, read literally like every
 * other field, so 0 pins every proposal to its parent, a real ablation. pop is the ga's
 * population; one only re-scores a point and zero is no population at all, so anything
 * under 2 is refused. */
typedef struct {
    long   climb_patience;    /* >= 1; rejections before the move size shrinks              */
    double climb_shrink;      /* in (0, 1); the shrink factor                               */
    double climb_restart_at;  /* in [0, 1]; scale/jitter ratio that restarts; 0 never does  */
    long   anneal_probes;     /* >= 1; uphill probes setting the start temperature          */
    double anneal_cool_ln;    /* <= 0; ln of the total temperature decay                    */
    double anneal_move_decay; /* in [0, 1]; move-size fraction the cooling removes          */
    double ga_mutate;         /* >= 0; mutation move size as a fraction of jitter           */
    double ga_mutate_decay;   /* in [0, 1]; mutation-size fraction removed over the run     */
    double ga_crossover;      /* in [0, 1]; chance a child blends two tournament winners
                                 rather than copying one. 1 is the shipped GA and draws no
                                 gate, so the default moves no trajectory; 0 is a crossover
                                 ablation, tournament winners surviving on mutation alone,
                                 which is what a claim about recombination must beat        */
    long   block;             /* >= 1; variables one proposal moves; n or more moves all    */
    long   verify;            /* >= 0; fresh evaluations of the RETURNED point, spent after
                                 the search and NOT against the budget; 0 disables         */
    double jitter;            /* >= 0; first move size, fraction of each variable's range   */
    long   pop;               /* >= 2; the ga's population                                  */
} cjitter_tuning;

/* The shipped constants: patience 40 + 10n, shrink 0.5, restart at 1/64, 20 probes,
 * cooling ln 1e-3, move decay 0.9, mutation 0.3 of jitter decaying by 0.9, crossover 1,
 * block n, verify 0, jitter 0.1, pop 30. N is the
 * problem's variable count, which the patience and block defaults scale with, so a tuning
 * taken for one n and used at another is a different tuning: block would no longer cover the
 * vector. */
cjitter_tuning cjitter_tuning_default(long n);

/* BEST is the smallest fitness OBSERVED during the run, which is what every optimiser reports
 * and what this one reported alone until 0.11.0. On a deterministic objective that is the
 * fitness of the point returned and there is nothing more to say. On a NOISY one it is not:
 * it is the luckiest draw the search happened to take, and the search is the thing that went
 * looking for lucky draws. Worse, the size of that luck depends on how much a method resamples
 * one place, which is exactly what distinguishes the methods being compared -- on a sphere in
 * ten variables at noise sigma 20, climbing takes 559 of its 4000 evaluations within half a
 * unit of the point it finally returns and annealing 317, where uniform sampling takes one, so
 * their reported bests carry very different amounts of luck. cjitter_compare has declared
 * "better" at p = 0.002 for a method that, judged on what it actually delivered, was 7 wins in
 * 9 and not shown.
 *
 * Set tuning.verify > 0 and VERIFIED holds the mean of that many fresh evaluations at the
 * returned point: an estimate of what the search delivered, made from draws the search could
 * not select on. INFLATION is verified - best, the luck. Both equal best and 0 when the
 * fitness is deterministic, whatever verify is, so the check costs nothing but evaluations and
 * is self-evidently inert where it is not needed.
 *
 * The verification evaluations are spent AFTER the search and are not deducted from evals, so
 * turning verify on does not shorten the search or move any trajectory; VERIFY_EVALS reports
 * what they cost. cjitter_compare judges on verified whenever verify > 0. */
typedef struct {
    double      best;    /* the smallest fitness observed; see above before trusting it */
    double     *x;       /* the point, n values, owned by the caller */
    long        evals;   /* actually spent */
    long        restarts;
    const char *method;  /* points into cjitter_methods: static storage, never freed */
    /* Added in 0.11.0 at the END, so an existing positional initialiser still puts every value
     * in the field it meant. Under -Wextra such an initialiser now warns that these three are
     * missing, which is the compiler telling you the struct grew rather than anything being
     * wrong; `cjitter_result r = { 0 };` and then setting r.x is the form that stays quiet
     * through this addition and the next. */
    double      verified;     /* mean of verify fresh evaluations at x; best when verify = 0 */
    double      inflation;    /* verified - best: how much of best was luck; 0 when verify = 0 */
    long        verify_evals; /* what the verification cost, over and above evals */
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
int cjitter_compare(const cjitter_problem *p, const cjitter_budget *b, long seeds, FILE *stream);
int cjitter_compare_tuned(const cjitter_problem *p, const cjitter_budget *b,
                          const cjitter_tuning *t, long seeds, FILE *stream);

/* The four names, NULL-terminated, in the order compare reports them. */
extern const char *const cjitter_methods[];

#endif /* CJITTER_H */
