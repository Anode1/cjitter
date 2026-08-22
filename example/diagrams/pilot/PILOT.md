# Stationarity pilot, 2026-08-21. Exploratory; superseded by the pre-registered run.

Question: is a human-authored diagram layout a local minimum of the aesthetic energy layout
tools minimise? Procedure: from the human layout, a one-node hill climb (stat8.c, its own
climber, not the cjitter library) with every node's displacement capped at d of the drawing
width, budget 4000 evaluations, one seed. Report how far it still moves the layout as a
fraction of d (delta_d), and what fraction of the energy it removes (rho_d).

Corpora, parsed to pr_*/ as JSON (largest connected component, node boxes, edges):

| corpus | files | parsed | median m/n |
| --- | --- | --- | --- |
| WikiPathways GPML, Homo sapiens, CC0 | 1016 | 571 | 1.06 |
| Reactome SBGN-ML | 1382 | 701 | 0.96 |
| BPMN Academic Initiative | 1 archive | 299 | 1.03 |

Rome benchmark graphs for comparison: m/n 1.30. Human diagrams are near-trees.

Energy at the human layout under asserted weights (1,1,1): term shares length 0.94,
crossings 0.06, overlap 0.00.

rho_d on WikiPathways by cap d: 7.0% at 0.005, 13.9% at 0.01, 25.9% at 0.02, 55.0% at
0.05, 79% at 0.1, 98% uncapped.

Null: the same descent from a layout first descended to convergence removes 0.00% at every
cap, and recovers 81 to 98% of a known perturbation of size d, so the zero is power.
Descent from the human start beats descent from a random start on 313/373; the human
layout beats a matched-budget uniform control on 350/373.

Held-out delta_0.02 (fit on 40 graphs, report on 40 others, 40 to 60 random draws of
log-uniform weights, alignment definition A1):

| energy | WikiPathways | SBGN | BPMN |
| --- | --- | --- | --- |
| asserted (1,1,1) | 98.7 | 91.7 | 92.8 |
| base three fitted | 99.0 | 91.7 | 90.4 |
| + node-edge separation | 98.0 | 91.3 | 93.0 |
| + orthogonality | 88.5 | 85.8 | 65.1 |
| + alignment (A1) | 61.0 | 79.9 | 46.9 |
| all six | 57.2 | 79.7 | 58.4 |

Edge-length uniformity takes weight 0.00 in every fit on every corpus. Alignment under A3
(smooth kernel) gave under one point on Reactome, against 34 points under A1: the definition
matters, which is why the pre-registration makes A3 primary and A1, A2 sensitivities.
Alignment-only descent induces overlap of 0.0009 of node area; forcing overlap back costs 3
points, so the alignment result is not an overlap artifact.

Files: parse_*.py (corpus to JSON), stat.c to stat8.c (successive versions of the climber
and energy; stat8.c is current, ALIGN_DEF selects A1/A2/A3), fitd.py and fitw.py (weight
fitting), missing.py and missing2.py (one added term at a time), feednull*.py (the null and
the power check), robust.py, notes.md (citation log).
Raw archives: ~/corpora/diagrams/ (not in git).

Defect found 2026-08-22 when the corpora were rebuilt for station: parse_sbgn.py and
parse_bpmn.py did not take the largest connected component. In the 15 to 40 node band, 126
of 180 Reactome graphs and 68 of 161 BPMN graphs were unions of fragments, and every pilot
number for those two corpora was measured on them. data/make_corpus.py takes the component
itself; the bands are now 305, 248 and 147 graphs.
