# Is the BPMN corpus a fair draw?

The corpus is the first 300 models by id of the 6{,}723 in the 15 to 40 band. A reviewer
cannot tell a selection from a sample, so this measures the difference.

## The funnel reproduces

`bpmai.tar.gz` holds 29,810 models. `parsers/parse_bpmn.py` keeps 18,308 with at least ten
nodes and one edge; `data/make_corpus.py --bpmn` takes the largest component of 15 to 40
nodes whose every edge is a BPMN flow or association and reports **6,723 graphs in the
band**, the recorded figure. Sorting those by file name and taking the first 300 reproduces
the committed `data/bpmn.txt` byte for byte.

## What the selection moves

`station direct` per graph over all 6,723, median q, against the 300 in the corpus:

| term | 300 by id | all 6,723 | difference |
| --- | --- | --- | --- |
| crossings | 1.000 | 1.000 | 0.000 |
| overlap | 1.000 | 1.000 | 0.000 |
| length | 0.000 | 0.000 | 0.000 |
| stress | 0.000 | 0.000 | 0.000 |
| node-edge | 1.000 | 1.000 | 0.000 |
| C+O | 1.000 | 1.000 | 0.000 |
| C+O+L | 0.000 | 0.000 | 0.000 |
| A3 | 0.310 | 0.312 | +0.002 |
| orthogonality | 0.726 | 0.733 | +0.007 |
| flow | 0.854 | 0.842 | -0.012 |
| **alignment A1** | **0.912** | **0.872** | **-0.041** |

Resampling 300 of the 6,723 two thousand times, the median alignment hold is 0.872 with a
95% range of [0.828, 0.903]. The committed corpus reads 0.912, at the **99th percentile**.
It is the one number in the paper the selection moves, and it is the headline, so the paper
states the alignment claim at 0.87 and keeps the tables on the 300, which is the corpus the
tool controls were generated for.

## Why the corpus was not simply redrawn

A new sample needs new `neato`, `prism`, `dot` and ELK controls. ELK is elkjs under node,
and node is not installed here, so a redrawn corpus would carry no ELK column for BPMN and
the tool-control table would lose a cell it currently has. Reporting the population value for
the hand layout costs nothing and answers the same question, since q is computed per graph
and needs no control.
