# The BPMN battery on a seeded random 300, tool controls regenerated

The committed corpus is the first 300 models by id, and bpmn_selection.md shows that
sample at the 99th percentile of a random 300 for alignment A1 (0.912 against the
population 0.872). Review asked what else moved. Answer: nothing else. This file is the
battery rerun on a seeded random 300 with all four tool controls regenerated.

## Regeneration

    tar xzf bpmai.tar.gz               # sha256 in data/bpmn_population_a1.csv
    python3 parsers/parse_bpmn.py <models> parsed_all
    python3 data/make_corpus.py parsed_all bpmn_all.txt --bpmn        # 6,723 graphs
    # sanity: --limit 300 reproduces data/bpmn.txt body byte for byte
    # sample: random.Random(1).sample(ids, 300) -> data/bpmnr_ids.txt, then
    python3 data/make_corpus.py <sampled parsed> data/bpmnr.txt --bpmn \
      --tool neato data/bpmnr_neato.txt --tool prism data/bpmnr_prism.txt \
      --tool dot data/bpmnr_dot.txt
    python3 data/elk_layout.py --node node --elk <elkjs 0.12.0> data/bpmnr.txt data/bpmnr_elk.txt
    # station check passes for all four; cells, analyse.py, fit.py, lanes.py as in run_all.sh

## Median q, hand

| energy | committed 300 | random 300 |
| --- | --- | --- |
| crossings alone | 1.000 | 1.000 |
| overlap alone | 1.000 | 1.000 |
| length alone | 0.000 | 0.000 |
| stress alone | 0.000 | 0.000 |
| orthogonality alone | 0.726 | 0.733 |
| alignment A1 alone | 0.912 | 0.846 |
| alignment A3 alone | 0.310 | 0.329 |
| gridiness alone | 0.952 | 0.947 |
| node-edge alone | 1.000 | 1.000 |
| flow alone | 0.854 | 0.862 |
| C+O+A1 | 0.889 | 0.846 |
| C+O+F | 0.845 | 0.860 |

The A1 hold moves from 0.912 to 0.846, around the population 0.872; every other energy
moves by at most 0.017. The tool controls keep their profiles (neato A1 0.056, stress
1.000; dot A1 1.000, stress 0.000; ELK A1 0.590).

## The pair the bias touched

Hand against dot on alignment A1, Holm within the corpus family: the committed 300 read
HL +0.04 [-0.01, +0.09] (p = 0.14); the random 300 reads HL -0.050 [-0.101, -0.001],
Holm p = 0.042. On the unbiased sample the hand layout sits below dot on alignment, as
it does in the other corpora; the committed sample's apparent parity was the selection's.

## Fit, sweep, lanes

R,A,N,F fit: alignment 0.881 (committed: 0.886), held-out q 0.651 [0.617, 0.689]
(committed: 0.660 [0.647, 0.674]). Sweep A on C+O at w = 0.001: 0.790 (committed:
0.811); L and S sweeps at 0.012 and 0.013. Lanes: of held boxes, 28% a column, 14% a
cross-lane row, 58% a same-lane row in lanes of median height 6.3 box heights
(committed: 30/13/57, 6.3).
