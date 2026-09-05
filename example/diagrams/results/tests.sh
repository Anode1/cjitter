#!/bin/sh
# The paired tests: per corpus and tool control, hand q against the control's q over the
# single terms, one Holm family per (corpus, control). Reads cells/, writes tests/.
S=$(cd "$(dirname "$0")" && pwd)
A=$S/../analyse.py
mkdir -p $S/tests
TERMS="crossings_alone overlap_alone length_alone stress_alone orthogonality_alone alignment_A1_alone alignment_A3_alone gridiness_alone node-edge_alone flow_alone"
for c in hs sbgn bpmn bpmnr; do
  for t in neato prism dot elk; do
    args=""
    for e in $TERMS; do args="$args $e $S/cells/${c}_hand_$e.csv $S/cells/${c}_${t}_$e.csv"; done
    python3 $A --family --alternative two-sided --md $args > $S/tests/${c}_$t.md
  done
done
echo TESTS_DONE > $S/tests/done
