# The registered 41 to 100 node band

The pre-registration named the band and the battery never ran it. Run: the largest
connected component of 41 to 100 nodes, the same test, hand against neato.

    python3 data/make_corpus.py parsed/hs hs41.txt --band 41 100 \
      --tool neato hs41_neato.txt --exclude WP5037 WP3391          # 108 graphs
    python3 data/make_corpus.py parsed/sbgn sbgn41.txt --band 41 100 \
      --tool neato sbgn41_neato.txt                                # 265 graphs
    python3 data/make_corpus.py <full bpmn parse> data/bpmn41.txt --bpmn --band 41 100 \
      --limit 300 --tool neato data/bpmn41_neato.txt               # tracked, needs the archive

The BPMN band file is the first 300 by id, the committed corpus's selection caveat with
it. Median q, hand / neato:

| term | hs41 (108) | sbgn41 (265) | bpmn41 (300) |
| --- | --- | --- | --- |
| crossings | 0.97 / 1.00 | 0.98 / 1.00 | 1.00 / 1.00 |
| overlap | 1.00 / 0.48 | 1.00 / 0.66 | 1.00 / 0.41 |
| length | 0.00 / 0.24 | 0.00 / 0.39 | 0.00 / 0.51 |
| stress | 0.00 / 1.00 | 0.00 / 1.00 | 0.00 / 1.00 |
| orthogonality | 0.19 / 0.00 | 0.18 / 0.00 | 0.74 / 0.01 |
| alignment A1 | 0.39 / 0.07 | 0.16 / 0.08 | 0.74 / 0.07 |
| gridiness | 0.61 / 0.30 | 0.44 / 0.28 | 0.71 / 0.33 |
| node-edge | 0.83 / 0.61 | 0.92 / 0.75 | 1.00 / 0.76 |
| flow | 0.54 / 0.43 | 0.56 / 0.42 | 0.78 / 0.55 |

Every verdict of the 15 to 40 band stands: length and stress hold nothing by hand and
neato's stress holds 1.00; overlap is satisfied; alignment holds 0.16 to 0.74 by hand
against neato's 0.07 to 0.08. The alignment hold is lower than in the small band (0.39
against 0.52, 0.16 against 0.21, 0.74 against 0.91), so larger diagrams carry less of
it, and crossings no longer hold every box of the biological hand layouts (0.97, 0.98):
size buys improving crossing moves.
