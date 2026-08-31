#!/bin/sh
# Every measurement the paper reads, after the corpora are regenerated. Run from the
# repository root: ./station and .venv/ are looked up there.
S=$(cd "$(dirname "$0")" && pwd)
P=example/diagrams/profile.py
D=example/diagrams/data
mkdir -p $S/cells $S/desc $S/fit
set -e
python3 $P --md --ci --save $S/cells > $S/full.md 2> $S/full.err
python3 $P --tex --ci > $S/tex.txt 2> $S/tex.err
python3 $P --md --layouts hand,chords --only "crossings alone,orthogonality alone,node-edge alone,C+O,C+O+R,C+O+N" > $S/chords.md 2> $S/chords.err
python3 $P --md --sweep --layouts hand,neato,elk --only "overlap alone,length alone,stress alone,alignment A1 alone,gridiness alone,flow alone,C+O,C+O+A1" > $S/sweep.md 2> $S/sweep.err
python3 $P --md --L median --only "length alone,stress alone,C+O+L,C+O+S" > $S/median.md 2> $S/median.err
python3 $P --md --L rsqrt --layouts hand,neato --only "length alone,stress alone" > $S/rsqrt.md 2> $S/rsqrt.err
echo PROFILES_DONE > $S/profiles_done
for c in hs sbgn bpmn; do
  ./station direct --corpus $D/$c.txt --weights 1,1,1,1,1,1,1,1 --diffs > $S/fit/$c.diffs
  for t in C,O,L C,O,L,S,R,A,N,F L,S R,A,N,F C,O,A; do
    echo "== $c terms $t"; .venv/bin/python3 example/diagrams/fit.py $S/fit/$c.diffs --terms $t
  done
  for t in L S A F; do echo "== $c sweep $t on C,O"; .venv/bin/python3 example/diagrams/fit.py $S/fit/$c.diffs --sweep $t --base C,O; done
done > $S/fit/report.txt 2>&1
echo FIT_DONE > $S/fit/done
for c in hs sbgn bpmn; do
  for w in "1,1,1,0,0,0,0,0 COL" "1,1,0,0,0,1,0,0 COA1" "0,0,0,1,0,0,0,0 S" "1,1,0,0,0,0,0,1 COF"; do
    set -- $w
    ./station descend --corpus $D/$c.txt --weights $1 --align a1 --converge 60000 > $S/desc/${c}_$2.csv 2> $S/desc/${c}_$2.err
  done
done
echo DESC_DONE > $S/desc/done

.venv/bin/python3 example/diagrams/results/robustness.py
