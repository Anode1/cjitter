# diagrams: which aesthetic criteria hold a hand-drawn layout

**People draw at a minimum of overlap and crossings, and nowhere near one of uniform edge
length or stress; a tool's layout of the same graphs is the reverse.**

A layout tool places the boxes of a diagram by minimising a weighted sum of criteria. If
people drew the same way, a layout a person accepted would be a local minimum of every
criterion that carries weight, and a tool started from it would find nothing to change. This
directory tests that, one criterion at a time, on diagrams whose coordinates a person chose.

    make station
    ./station direct --corpus example/diagrams/data/hs.txt --weights 1,1,0,0,0,0,0
    make profile                 # the table below, about two minutes

## The test

For every box, try sixteen moves of length d (2% of the drawing width), every other box
fixed. The box is **held** if none of the moves lowers the energy; a move that changes
nothing is not an improvement, so ties hold. q is the fraction of boxes held: 1 means the
layout is a local minimum over single-box moves of that length, 0 means every box has
somewhere better to be. It needs no gradient, so it is defined for a crossing count and for
the corners of the overlap term, and it needs no seed, so it reproduces exactly. For a term
that is a count or a step (crossings, gridiness) a box is held wherever the term is flat, so
the table prints the term's value beside q: 0 is satisfied.

## The corpora

Three communities, three authoring tools, every diagram's largest connected component of 15
to 40 boxes (`data/make_corpus.py` takes the component and says why). Beside each, the same
graphs with the same box sizes laid out by `neato`, graphviz's stress layout, seeded so the
file reproduces: what a layout that does minimise something looks like under the same test.

| corpus | source | graphs | median edges per box |
| --- | --- | --- | --- |
| WikiPathways | GPML, Homo sapiens, CC0, drawn in PathVisio | 305 | 1.06 |
| Reactome | SBGN-ML, curated by hand | 248 | 0.96 |
| BPMN | Academic Initiative, students in Signavio | 147 | 1.03 |

## The terms

One function each in `energy.c`; `energy.h` gives the formulas. Crossings C, overlap O,
edge-length uniformity L, stress S, orthogonality R, alignment A (four definitions: A1 a
corner at exact alignment, A2 rows and columns separately, A3 a smooth kernel, grid HOLA's
gridiness), node-edge separation N.

## The profile

Median q over graphs at d = 0.02, 16 directions. Hand layouts, then neato's layouts of the
same graphs. In brackets, for a single term, the median value of that term at the layout.

| energy | WikiPathways hand | Reactome hand | BPMN hand | WikiPathways neato | Reactome neato | BPMN neato |
|---|---|---|---|---|---|---|
| crossings alone | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| overlap alone | 1.00 | 1.00 | 1.00 | 0.60 | 0.76 | 0.39 |
| length alone | 0.03 | 0.04 | 0.06 | 0.26 | 0.41 | 0.43 |
| stress alone | 0.00 | 0.00 | 0.00 | 0.07 | 0.08 | 0.12 |
| orthogonality alone | 0.16 | 0.05 | 0.41 | 0.00 | 0.00 | 0.00 |
| alignment A1 alone | 0.44 | 0.16 | 0.80 | 0.04 | 0.05 | 0.04 |
| alignment A3 alone | 0.19 | 0.09 | 0.27 | 0.00 | 0.00 | 0.00 |
| gridiness alone | 0.87 | 0.65 | 0.90 | 0.78 | 0.77 | 0.78 |
| node-edge alone | 0.76 | 0.85 | 0.86 | 0.89 | 0.93 | 0.91 |
| C+O | 1.00 | 1.00 | 1.00 | 0.60 | 0.76 | 0.39 |
| C+O+L (1,1,1) | 0.04 | 0.04 | 0.07 | 0.18 | 0.35 | 0.24 |
| C+O+S | 0.00 | 0.00 | 0.00 | 0.04 | 0.07 | 0.05 |
| C+O+R | 0.17 | 0.06 | 0.41 | 0.00 | 0.00 | 0.00 |
| C+O+A1 | 0.47 | 0.17 | 0.74 | 0.03 | 0.04 | 0.04 |
| C+O+A3 | 0.24 | 0.11 | 0.29 | 0.03 | 0.03 | 0.00 |
| C+O+grid | 0.83 | 0.64 | 0.82 | 0.45 | 0.52 | 0.30 |
| C+O+N | 0.76 | 0.84 | 0.80 | 0.55 | 0.71 | 0.38 |

(Table from `make profile` of 2026-08-22; the bracketed term values are printed by the
current profile.py and are to be pasted in on the next run.)

Read down the hand columns. Overlap and crossings hold every box: people satisfy them
exactly, and at radius 0.02 there is no crossing a single box can remove. Uniform edge
length and stress hold almost none: every box has a move that evens the lengths, which is
what a tool would do first and what the person did not. The three-term energy tools use,
C+O+L, inherits the length term's verdict, which is what an earlier version of this study
reported as "hand layouts are not stationary". Alignment with a corner (A1) holds half the
WikiPathways boxes and four in five BPMN boxes; neato's columns read the other way on every
row that separates them.

## Files

    energy.h energy.c    the terms; + - * / fabs sqrt and a polynomial exp, nothing else
    corpus.h corpus.c    the text format, the inclusion refusals, rescaling, L, distances
    station.c            direct (the test, per graph or per box) and terms (the values)
    profile.py           the table above over every corpus and declared energy
    data/make_corpus.py  pilot JSON to text corpora, the component taken here; --neato
    data/*.txt           the corpora; fixture.txt is three hand-computable layouts
    pilot/               the exploratory run of 2026-08-21, superseded, defects declared

`tests/diagrams.sh` pins the fixture's term values, worked out by hand, the directional
verdicts on them, order independence, every refusal, and that `energy.o` calls no
transcendental libm. The descent through the library, the matched-budget control, and the
weight fit are the next steps; the pre-registration governing them is in the articles
repository.
