/* energy.h -- the aesthetic energy of a diagram layout, one function per term.
 *
 * Copyright (c) 2026 Vasili Gavrilov. BSD 2-Clause; see LICENSE.
 *
 * A layout is x, two coordinates per node, boxes of fixed size centred on them. The energy is
 * the weighted sum of the terms below, every term normalised so that 0 is "the criterion is
 * satisfied" and the scale does not depend on the number of nodes or edges. The weights are
 * the caller's: the paper's question is which of these terms a hand layout sits at a minimum
 * of, so each term is also callable alone.
 *
 *   C crossings     pairs of edges sharing no endpoint that cross, over m
 *   O overlap       total overlap area of the boxes, over total box area
 *   L length        mean over edges of ((len - L) / L)^2, L the reference length (length_ref)
 *   S stress        mean over node pairs of ((dist - L d_ij) / (L d_ij))^2, d_ij graph distance
 *   R orthogonality mean over edges of min(|dx|, |dy|) / (|dx| + |dy|)
 *   A alignment     one of four definitions, chosen at run time; see align_def
 *   N node-edge     mean over edges of the squared shortfall of every non-endpoint node's
 *                   distance below (w + h) / 4 of that node
 *
 * Arithmetic is + - * / fabs sqrt and a polynomial exp, so a value reproduces across
 * platforms; the Makefile's -ffp-contract=off is part of that promise. */
#ifndef DIAGRAMS_ENERGY_H
#define DIAGRAMS_ENERGY_H

enum { TERM_C, TERM_O, TERM_L, TERM_S, TERM_R, TERM_A, TERM_N, NTERMS };
extern const char *const term_name[NTERMS];   /* "crossings", "overlap", ... */
extern const char  term_letter[NTERMS + 1];   /* "COLSRAN" */

/* A1: per node, the distance to the nearest other node along whichever axis is nearer, so a
 *     node in some row or column scores 0. A corner at exact alignment.
 * A2: rows and columns priced separately, half the sum of the two nearest distances.
 * A3: per node, 1 / (1 + sum over others of exp(-(dx/s)^2) + exp(-(dy/s)^2)); smooth, no corner.
 * GRID: HOLA's gridiness R4 turned into a cost: 1 minus the fraction of nodes that share a
 *     row or a column, within tol, with at least two other nodes. */
typedef enum { ALIGN_A1 = 1, ALIGN_A2 = 2, ALIGN_A3 = 3, ALIGN_GRID = 4 } align_def;

/* The reference length L of the length and stress terms, fixed from the layout as loaded so
 * the term cannot follow a move. FIT: the L at which the term is least for that layout,
 * sum(l^2) / sum(l) over edges for length and the same over r / d_ij for stress; a layout at
 * a true minimum of the term is then held, which the median cannot promise (a converged
 * stress layout under the median scores q near 0.1, under the fit 1.00). MEDIAN: the median
 * edge length, the upper one at even m. RSQRT: 1 / sqrt(n), fixed by the node count alone. */
typedef enum { L_FIT = 0, L_MEDIAN = 1, L_RSQRT = 2 } length_ref;

typedef struct {
    char    id[128];
    long    n, m;
    double *x;      /* 2n, in the unit square after corpus_read */
    double *w, *h;  /* n, same scale */
    long   *ea, *eb;/* m, endpoints, ea < eb */
    int    *dist;   /* n*n graph distances (hops); the corpus is connected components */
    double  scale;  /* the raw extent the layout was divided by; w * scale is the raw width */
    double  L_median, L_len, L_stress;  /* the reference-length candidates, see length_ref */
} graph;

typedef struct {
    double    w[NTERMS];
    align_def align;
    length_ref lref;
    double    s;    /* A3 kernel width, as a fraction of the drawing width */
    double    tol;  /* GRID tolerance, same units */
} energy_spec;

/* The reference length the length (K = TERM_L) or stress (TERM_S) term reads under E. */
double ref_length(int k, const graph *g, const energy_spec *e);

/* One term. K is TERM_*. */
double term_value(int k, const graph *g, const double *x, const energy_spec *e);

/* The weighted sum. T, if not NULL, receives the seven term values. A term with weight 0 is
 * still evaluated into T, so the per-term table costs nothing extra. */
double energy(const graph *g, const double *x, const energy_spec *e, double t[NTERMS]);

/* exp(x) for x <= 0 in + * / only: a Taylor series on x/64 squared six times, the same
 * polynomial c/cjitter.c uses for the annealer's acceptance, copied so the example does not
 * reach into the library's internals. Within 1e-12 relative on [-40, 0]; 0 below. */
double energy_exp_neg(double x);

#endif
