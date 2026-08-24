# The alignment controls: half-pitch jitter, and the grid-snapped null

Run 2026-08-24 with `station direct --weights 0,0,0,0,0,1,0,0 --align a1`, the A1 cell alone,
d = 0.02, 16 directions. Corpora and binary as in manifest.txt. Generator:
`align_controls.py <corpus> <out> jitter|snapped <pitch> <seed>`. Box sizes, node counts and
each diagram's bounding box are preserved by both controls.

## Pitch detection

Coordinates within 2% of a multiple of the candidate pitch:

| corpus | integer-valued | pitch 1 | pitch 2.5 | pitch 5 | pitch chosen |
| --- | --- | --- | --- | --- | --- |
| WikiPathways | 17.2% | 20.3% | 7.2% | 6.7% | none detected |
| Reactome | 67.4% | 67.4% | 20.2% | 13.1% | 1 |
| BPMN | 62.7% | 65.1% | 46.8% | 45.8% | 1 |

## The two controls, median q over the corpus

| corpus | hand | half-pitch jitter (3 seeds) | grid-snapped random (3 seeds) |
| --- | --- | --- | --- |
| WikiPathways | 0.520 | 0.500, 0.500, 0.500 | 0.065, 0.061, 0.062 |
| Reactome | 0.207 | 0.206, 0.200, 0.205 | 0.075, 0.056, 0.067 |
| BPMN | 0.912 | 0.889, 0.882, 0.882 | 0.067, 0.080, 0.071 |

The snapped null carries the same grid, the same boxes and the same bounding box, and no
author. It holds 6 to 8 percent of boxes in every corpus. The grid does not produce the
alignment hold.

## Why the declared jitter test cannot fail at the detected pitch

A1 is `min_j min(|dx|, |dy|)`, so a node offset by e from its nearest coordinate agreement
is un-held only if some move of radius d decreases A1. Moving d toward the agreement lands
at `d - e`, which exceeds e whenever `e < d/2`. The directional test therefore cannot
resolve any misalignment below half the probe radius, and A1's held fraction is the fraction
of boxes lying within d/2 of sharing a coordinate, not the fraction lying exactly on one.

Median diagram span is 797, 811 and 1289 corpus units, so d is about 16, 16 and 26 and d/2
is about 8, 8 and 13. Half of the detected pitch is 0.5 units, which is 16 to 26 times below
that resolution. The declared test passes because it perturbs two orders of magnitude below
what the instrument can see, and the section 6 prediction that H4 would fail on BPMN after
jitter was wrong for a reason independent of the data.

## The jitter magnitude sweep, median q, seed 1

Displacement is up to half the stated pitch in each axis.

| pitch | half-width | BPMN | WikiPathways | Reactome |
| --- | --- | --- | --- | --- |
| hand | 0 | 0.912 | 0.520 | 0.207 |
| 1 | 0.5 | 0.889 | 0.500 | 0.206 |
| 10 | 5 | 0.588 | 0.267 | 0.147 |
| 30 | 15 | 0.200 | 0.097 | 0.095 |
| 60 | 30 | 0.125 | 0.074 | 0.080 |
| 120 | 60 | 0.081 | 0.074 | 0.065 |
| 240 | 120 | 0.062 | 0.065 | 0.061 |

The hold falls to the snapped-random level exactly as the displacement crosses d/2, which is
what a within-d/2 coincidence counter must do.

## What the controls settle, and what they do not

Settled: the alignment hold is not an artifact of the editor's grid. A layout snapped to the
same grid, with the same boxes in the same box, holds 6 to 8 percent.

Not settled: the snapped null removes every kind of structure at once, rows, columns, lanes
and flow together, so it separates the author from the grid but not the author from the
notation. The lane decomposition in lanes.py carries that question.
