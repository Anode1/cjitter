# diagrams: which aesthetic criteria hold a hand-drawn layout

**A hand layout is held by overlap, by nothing that prices distance, and by alignment for
0.52, 0.21 and 0.91 of its boxes by corpus; of four tools laid out on the same graphs, the
layered ones match that profile on every criterion but alignment and flow, where they are
exact.** The paper that reads these tables, *What Do People Optimize in Diagram Layout?*, is
[articles/cjitter](https://github.com/Anode1/articles/tree/main/cjitter).

A layout tool places the boxes of a diagram by minimising a weighted sum of criteria. If
people drew the same way, a layout a person accepted would be a local minimum of every
criterion that carries weight, and a tool started from it would find nothing to change. This
directory tests that, one criterion at a time, on diagrams whose coordinates a person chose.

    make station
    ./station direct --corpus example/diagrams/data/hs.txt --weights 1,1,0,0,0,0,0,0
    make profile                 # the table below, about five minutes

## The test

For every box, try sixteen moves of length d (2% of the drawing width), every other box
fixed. The box is **held** if none of the moves lowers the energy; a move that changes
nothing is not an improvement, so ties hold. q is the fraction of boxes held: 1 means the
layout is a local minimum over single-box moves of that length, 0 means every box has
somewhere better to be. It needs no gradient, so it is defined for a crossing count and for
the corners of the overlap term, and it needs no seed, so it reproduces exactly. For a term
that is a count or a step (crossings, gridiness) a box is held wherever the term is flat, so
the table prints the term's value beside q: 0 is satisfied. A move that lowers the energy by
less than 1e-12 of its value is a tie.

The length and stress terms compare distances with a reference length L. Fixed at the median
edge length, L is not the scale at which the term is least for the layout, so at a true
minimum every box still has a scaling move that lowers the term: neato's stress layouts
scored q = 0.07 to 0.12 under their own criterion. L is therefore fitted to the layout as
loaded (sum of l squared over sum of l, over edges for length and over pair distance per hop
for stress) and fixed before any move; neato's stress layouts then score 1.00. `--L median`
and `--L rsqrt` (1 / sqrt n) are the sensitivities.

## The corpora

Three communities, three authoring tools, every diagram's largest connected component of 15
to 40 boxes (`data/make_corpus.py` takes the component and says why). Beside each, the same
graphs with the same box sizes laid out by four tools, what a layout that does
minimise something looks like under the same test: `neato` (stress, seeded so the file
reproduces, boxes left where they overlap), `prism` (neato followed by its overlap removal,
`-Goverlap=false`), `dot` (layered, edge directions kept) and ELK layered 0.12.0 (`elkjs`,
direction RIGHT, `data/elk_layout.py`), the engine the BPMN editors run. `station check`
confirms a control is the same graphs with the same edges, directions and box sizes. The BPMN
corpus is the first 300 models by id whose every edge is a BPMN flow or association: the
Academic Initiative archive is two fifths EPC, Petri net and UML models, whose flows the
pilot's node rules turned into degree-2 nodes.

| corpus | source | graphs | median edges per box |
| --- | --- | --- | --- |
| WikiPathways | GPML, Homo sapiens, CC0, drawn in PathVisio | 305 | 1.06 |
| Reactome | SBGN-ML, curated by hand | 248 | 1.04 |
| BPMN | Academic Initiative, students in Signavio, BPMN models only | 300 | 1.11 |

## The terms

One function each in `energy.c`; `energy.h` gives the formulas. Crossings C, overlap O,
edge-length uniformity L, stress S, orthogonality R, alignment A (four definitions: A1 a
corner at exact alignment, A2 rows and columns separately, A3 a smooth kernel, grid HOLA's
gridiness), node-edge separation N, flow F (the backward component of directed edges along
the reading direction, the axis direction under which the term is least for the layout).

An edge is the route the file stores: the attachment point on the source box, the interior
waypoints, the attachment point on the target box; C, R and N are evaluated on it, L and S
on the centres. When a box moves its attachment points move with it and the waypoints stay.
`data/*_chords.txt` are the same corpora as chords, the sensitivity. Of the edges in the
band, 29% of WikiPathways', 7% of Reactome's and 33% of BPMN's carry an interior waypoint;
GPML's Elbow and Curved connectors store endpoints only (15% and 3% of WikiPathways edges)
and read as chords, and 22% end on an anchor of another interaction, where the last segment
to the node is manufactured. Directed: 91%, 100%, 95%.

## The profile

Median q over graphs at d = 0.02, 16 directions, fitted L. Hand layouts, then the four tools'
layouts of the same graphs. In brackets, for a single term, the median value of that term at
the layout. `make profile` prints the same table with a 95% bootstrap interval on every median
(1000 resamples of graphs); the widest on a hand cell spans 0.10.

| energy | WikiPathways hand | Reactome hand | BPMN hand | WikiPathways neato | Reactome neato | BPMN neato | WikiPathways prism | Reactome prism | BPMN prism | WikiPathways dot | Reactome dot | BPMN dot | WikiPathways elk | Reactome elk | BPMN elk |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| crossings alone | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.029) | 1.00 (0.056) | 1.00 (0.000) | 1.00 (0.036) | 1.00 (0.066) | 1.00 (0.000) |
| overlap alone | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 0.60 (0.030) | 0.76 (0.022) | 0.41 (0.073) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) |
| length alone | 0.00 (0.219) | 0.00 (0.234) | 0.00 (0.291) | 0.24 (0.011) | 0.35 (0.005) | 0.42 (0.006) | 0.19 (0.014) | 0.26 (0.009) | 0.27 (0.014) | 0.00 (0.241) | 0.00 (0.232) | 0.00 (0.250) | 0.00 (0.127) | 0.00 (0.205) | 0.00 (0.273) |
| stress alone | 0.00 (0.217) | 0.00 (0.212) | 0.00 (0.203) | 1.00 (0.042) | 1.00 (0.034) | 1.00 (0.017) | 0.94 (0.044) | 0.85 (0.042) | 0.90 (0.024) | 0.00 (0.201) | 0.00 (0.177) | 0.00 (0.169) | 0.00 (0.241) | 0.00 (0.197) | 0.00 (0.190) |
| orthogonality alone | 0.19 (0.157) | 0.17 (0.193) | 0.73 (0.011) | 0.00 (0.280) | 0.00 (0.277) | 0.00 (0.280) | 0.00 (0.281) | 0.00 (0.277) | 0.00 (0.278) | 0.19 (0.244) | 0.15 (0.256) | 0.26 (0.167) | 0.19 (0.158) | 0.12 (0.203) | 0.25 (0.139) |
| alignment A1 alone | 0.52 (0.003) | 0.21 (0.004) | 0.91 (0.000) | 0.06 (0.011) | 0.06 (0.009) | 0.06 (0.011) | 0.07 (0.010) | 0.06 (0.010) | 0.06 (0.011) | 1.00 (0.000) | 1.00 (0.000) | 0.89 (0.000) | 0.57 (0.001) | 0.42 (0.002) | 0.62 (0.001) |
| alignment A3 alone | 0.20 (0.284) | 0.10 (0.300) | 0.31 (0.207) | 0.03 (0.446) | 0.03 (0.430) | 0.00 (0.453) | 0.03 (0.451) | 0.03 (0.428) | 0.00 (0.452) | 0.45 (0.204) | 0.39 (0.206) | 0.19 (0.168) | 0.35 (0.176) | 0.31 (0.213) | 0.20 (0.155) |
| gridiness alone | 0.87 (0.424) | 0.65 (0.640) | 0.95 (0.176) | 0.78 (1.000) | 0.77 (1.000) | 0.78 (1.000) | 0.79 (1.000) | 0.75 (1.000) | 0.78 (1.000) | 1.00 (0.125) | 0.95 (0.176) | 0.79 (0.219) | 0.76 (0.200) | 0.74 (0.306) | 0.69 (0.250) |
| node-edge alone | 0.88 (0.004) | 1.00 (0.000) | 1.00 (0.000) | 0.89 (0.000) | 0.93 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 0.78 (0.011) | 0.80 (0.007) | 0.82 (0.003) | 0.74 (0.016) | 0.75 (0.021) | 0.76 (0.016) |
| flow alone | 0.59 (0.131) | 0.62 (0.096) | 0.85 (0.045) | 0.50 (0.211) | 0.45 (0.217) | 0.62 (0.121) | 0.50 (0.211) | 0.44 (0.222) | 0.60 (0.123) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) | 1.00 (0.000) |
| C+O | 1.00 | 1.00 | 1.00 | 0.60 | 0.76 | 0.41 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| C+O+L | 0.00 | 0.00 | 0.00 | 0.17 | 0.33 | 0.24 | 0.19 | 0.27 | 0.29 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| C+O+S | 0.00 | 0.00 | 0.00 | 0.60 | 0.76 | 0.43 | 0.94 | 0.89 | 0.92 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| C+O+R | 0.21 | 0.17 | 0.73 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.19 | 0.14 | 0.28 | 0.22 | 0.13 | 0.40 |
| C+O+A1 | 0.51 | 0.22 | 0.89 | 0.04 | 0.05 | 0.04 | 0.07 | 0.07 | 0.09 | 1.00 | 1.00 | 0.88 | 0.57 | 0.43 | 0.68 |
| C+O+A3 | 0.24 | 0.11 | 0.33 | 0.03 | 0.03 | 0.03 | 0.03 | 0.03 | 0.03 | 0.50 | 0.42 | 0.33 | 0.41 | 0.33 | 0.45 |
| C+O+grid | 0.84 | 0.64 | 0.95 | 0.45 | 0.52 | 0.28 | 0.79 | 0.75 | 0.78 | 0.96 | 0.94 | 0.75 | 0.74 | 0.73 | 0.67 |
| C+O+N | 0.86 | 0.94 | 1.00 | 0.55 | 0.71 | 0.40 | 1.00 | 1.00 | 1.00 | 0.78 | 0.80 | 0.82 | 0.74 | 0.75 | 0.76 |
| C+O+F | 0.56 | 0.61 | 0.84 | 0.30 | 0.33 | 0.26 | 0.50 | 0.44 | 0.61 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |

Read down the hand columns. Overlap holds every box and is satisfied (value 0): people never
let boxes overlap, and neither does any tool that removes overlaps, so C+O separates a hand
layout from neato's configuration and from nothing else. Crossings hold every box in every
layout, because at radius 0.02 no single box can remove one; on the drawn routes the median
hand diagram has no crossing in any corpus (on chords WikiPathways reads one per 26 edges),
where dot and elk leave one per 15 to 35 edges on the biological corpora. Uniform edge
length and stress hold no hand box: every box has a move that evens the distances, which is
what neato did (stress 1.00) and what the person, dot and elk did not (0.00). The three-term
energy tools use, C+O+L, inherits the length term's verdict, which is what an earlier
version of this study reported as "hand layouts are not stationary". Alignment with a corner
(A1) holds half the WikiPathways boxes, a fifth of Reactome's and nine in ten BPMN boxes,
against 0.06 for neato, 0.89 to 1.00 for dot, whose layers put every box in a row, and 0.42
to 0.62 for elk; by the gridiness value, 58%, 36% and 82% of hand-placed boxes are in an
alignment of three or more, none of neato's, 88%, 82% and 78% of dot's. Flow is satisfied
exactly by dot and elk (1.00 at value 0) and partly by the person (0.59, 0.62 and 0.85): a
backward edge is something people tolerate and layered tools do not. On every term but
alignment and flow the hand column reads as dot's column does.

## Files

    energy.h energy.c    the terms; + - * / fabs sqrt and a polynomial exp, nothing else
    corpus.h corpus.c    the text format (routes, directions), the refusals, rescaling, L, u
    station.c            direct (the test; --diffs for the fit), terms, check, descend (the
                         capped climber and the in-cap control through the library)
    profile.py           the table above; --ci, --sweep, --tex, --save for analyse.py
    fit.py               the weight fit: the LP on the simplex, 5x2 cross-validation, sweeps
                         (needs scipy: python3 -m venv .venv && .venv/bin/pip install scipy)
    analyse.py           paired Wilcoxon, sign test, Hodges-Lehmann, Holm; 630 self-checks
    parsers/             GPML, SBGN-ML and Signavio JSON to JSON with routes and directions;
                         check_against_pilot.py proves the node and edge sets unchanged
    parsed/              the parsers' JSON for the diagrams in the band
    data/make_corpus.py  JSON to text corpora, the component taken here; --tool, --chords
    data/elk_layout.py   the ELK control, through elkjs under a local node
    data/*.txt           the corpora, their controls, their chord variants; fixture.txt
    results/run_all.sh   every measurement the paper reads, into results/; tests.sh the
                         paired families, lanes.py the BPMN lane analysis, sparsity.py
                         q against m/n, manifest.txt the versions, seeds and hashes
    pilot/               the exploratory run of 2026-08-21, superseded, defects declared

`tests/diagrams.sh` (114 checks, under two seconds) pins the fixture's term values, worked
out by hand under every reference length, the directional verdicts on them including the
4-cycle that is a stress minimum at its fitted scale and at no other, order independence,
every refusal, that every control is the same graphs as its corpus, the q of the first
twenty graphs of each corpus under four energies, and that `energy.o` calls no
transcendental libm; and the route fixture whose chords cross and whose drawn routes do
not, the attachment point that moves with its box, the flow verdicts, and descend's
reproducibility. The table above predates the routes, the flow term and the BPMN corpus
rebuild; `results/run_all.sh` produces its successor.
