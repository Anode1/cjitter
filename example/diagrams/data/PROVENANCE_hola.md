# hola.txt

Human-drawn layouts of abstract graphs from the HOLA formative study (Kieffer,
Dwyer, Marriott, Wybrow, "HOLA: Human-like Orthogonal Network Layout", TVCG
2016). 17 participants each laid out the same 8 small graphs in the study's
orthogonal editor; 136 drawings, all extracted, none excluded.

## Fetch

2026-08-25, from https://data.graphlayout.net/HOLA/formative. The page embeds
one SVG per drawing at
`https://data.graphlayout.net/HOLA/formative/layouts/<participant>-<task>.svg`
(17 five-hex-digit participant ids x tasks 1..8), plus the 8 reference graphs
`h1.svg..h8.svg` and 8 yEd layouts `y1.svg..y8.svg`, which are not part of the
corpus. `http://data.graphlayout.net/HOLA/layouts/...` (the path the src
attributes suggest) 404s; the working path is under `/HOLA/formative/`.

## Parsing

Each SVG is the study editor's own format. Per node, a `<g transform=
"translate(x,y)">` inside `<g id="nodes">` gives the centre; its visible
`<rect>` (the one without `display="none"`) gives width and height, so box
sizes vary per node as drawn (30x30 squares and text-sized boxes such as
50x30). Per edge, the route `<path>` inside `<g id="edges">` is walked
(M/L/H/V and relative arcs) and its two endpoints are matched to the node
whose box they touch; every endpoint in the corpus lies exactly on a box
(worst distance 0.0 px). One edge in `78d30-5.svg` has `stroke:none` on its
route path; the identical halo path in the same group supplied the geometry.
Bend points are dropped: edges are emitted as chords (`U`, no route points),
which matches how the study fixed each graph while participants moved nodes
and bends. The outer `translate,scale,translate` on the SVG is uniform per
drawing and ignored; the reader rescales to the unit square anyway.

Task number in the file name is presentation order, randomized per
participant. Graph identity was recovered by isomorphism against h1..h8: each
of the 136 drawings matched exactly one reference, and each participant
covers all 8. Ids in the corpus are `<participant>_g<k>` with k the reference
graph: g1 K4 (4,6), g2 Dog (9,10), g3 Binary Tree (7,6), g4 Elephant (12,12),
g5 Snail (13,14), g6 Robot (7,6), g7 Montana (12,13), g8 Potted Plant
(10,10). All drawings are connected, self-loop free, duplicate free.

## Licensing

Neither the site root, the formative page, nor any reachable index states a
license or copyright for the data. The study is Monash University research
published in TVCG 2016; treat the drawings as citable research data with no
explicit license grant.

## Exclusions

None.

## Measurement

Private build of station.c, `direct --corpus hola.txt --weights <one-hot>
--align a1 --L fit`, median of the q column over the 136 drawings:

| term          | median q |
|---------------|----------|
| crossings     | 1.0000   |
| overlap       | 1.0000   |
| length        | 0.0000   |
| stress        | 0.0000   |
| orthogonality | 0.2265   |
| alignment     | 0.5000   |
| node-edge     | 1.0000   |
| flow          | 1.0000   |
