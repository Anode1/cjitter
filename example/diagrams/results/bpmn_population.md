# The full population profile, all 6,723 BPMN models in the band

data/bpmn_population_a1.csv tracks five terms; review asked for the rest, since the
committed 300's one demonstrated bias is on alignment. Each row of this table is one
`station direct --corpus bpmn_all.txt --weights <one-hot>` over the whole band
(regeneration of bpmn_all.txt in bpmn_random300.md); the A1 rerun reproduced the
tracked CSV on all 6,723 rows exactly.

| term | population median | committed 300 |
| --- | --- | --- |
| crossings | 1.000 | 1.000 |
| orthogonality | 0.733 | 0.726 |
| alignment A1 | 0.872 | 0.912 |
| alignment A3 | 0.312 | 0.310 |
| gridiness | 0.947 | 0.952 |
| node-edge | 1.000 | 1.000 |

With overlap, length, stress and flow already in the tracked CSV (1.000, 0.000, 0.000,
0.842 at the population, against the committed 300's 0.854 on flow), every term but
alignment A1 sits at or within 0.012 of its population value in the committed 300: the
selection bias is confined to the one term the headline was already corrected for.
