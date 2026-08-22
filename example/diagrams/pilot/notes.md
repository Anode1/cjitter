# VERIFIED so far
Klammler, Mchedlidze, Pak. Aesthetic Discrimination of Graph Layouts.
 GD 2018 doi 10.1007/978-3-030-04414-5_12 ; JGAA 23(3) 2019 doi 10.7155/jgaa.00501 ; arXiv 1809.01017 (PDF read this session)
 - corpus: force-directed layouts + "native" generator layouts, worsened by PERTURB/FLIP/MOVLSQ + interpolation. ~36000 labeled pairs. NO humans.
 - quote p.? "As manually-labelled data were unavailable, we have fixed the values of t as follows."
 - quote conclusion: "We admit that this study should ideally be repeated with human-labeled data. However, this requires that a dataset be collected with a size similar to ours, which is a challenging task. Creating such a dataset may become a critically important accomplishment in the graph drawing field."
 - they DID fit weights of a linear metric combo (COMB, Huang et al. 2016 metric) by Nelder-Mead on prediction accuracy: wEL=+0.4803+-0.0855, wCC=+0.4679+-0.1069, wCR=-0.0431+-0.0315, wAR=-0.0087+-0.0072
 - accuracy 95.70% (arXiv v1 abstract) / 96.48% (later version line 22)
Q2 leads found in Klammler related work (RECALLED-from-their-text, need own verification):
 - Masui, T. Evolutionary learning of graph layout constraints from examples. UIST'94 pp103-108 doi 10.1145/192426.192468. Linear combination, weights via genetic programming, from pairs of good/bad layouts provided by users.
 - Barbosa & Barreto, GECCO'01 pp203-210, co-evolution of weights to match user ranking.
 - Spoenemann, Duderstadt, von Hanxleden. Evolutionary meta layout of graphs. Diagrams 2014 pp16-30 doi 10.1007/978-3-662-44043-8_3. slider weights / selection-adjusted weights.
 - Rosete-Suarez, Sebag, Ochoa-Rodriguez. A study of evolutionary graph drawing. LRI TR 1228, 1999. relative importance of metrics from user input.
 - Huang, Huang, Lin. Info Sciences 330:444-454 2016 doi 10.1016/j.ins.2015.05.028 aggregate aesthetics metric.
TOOL STATE: WebSearch exhausted (200/200). OpenAlex daily budget exhausted (429 till midnight UTC). dblp 503. Semantic Scholar 429. Crossref API works. arXiv API works (https). Direct PDF fetch works.

## Q1/Q4 finds
Purchase, Archambault, Kobourov, Nollenburg, Pupyrev, Wu. The Turing Test for Graph Drawing Algorithms. GD2020, arXiv 2008.04869v4 (read). 9 graphs (15-108 nodes), 4 human drawers (GD researchers), 4 algorithms (yEd/GraphViz FD, MDS/stress, circular, orthogonal). Supplementary https://www.dcs.gla.ac.uk/~hcp/GD2020/ LIVE, directory listing = JPEG images only (g{i}d{k}.jpg human, g{i}a[fd|mds|c|o].jpg algo) + READ ME.txt. NO coordinates. Finding: hand-drawn distinguishable from algorithmic overall; FD and (marginally) MDS pass; hand-drawn judged higher quality.
van Ham & Rogowitz TVCG 14(6) 2008 doi 10.1109/tvcg.2008.155, OA pdf pure.tue.nl/ws/files/2857622/Metis225962.pdf (read). Users lay out graphs by hand; measured crossings, edge-length uniformity, cluster recovery; humans produced FEWER crossings than the force-directed algorithm used. No descent-from-human-layout test, no fitted weights.
Dwyer, Lee, Fisher, Quinn, Isenberg, Robertson, North. A Comparison of User-Generated and Automatic Graph Layouts. TVCG 2009 doi 10.1109/tvcg.2009.109. NOT open access (unpaywall says closed); not read this session.
Flud: Bharadwaj, Gwizdala, Kim, Luther, Murali. ACM TOCHI 2022 doi 10.1145/3479196; arXiv 1908.07471 (read). Crowd GWAP lays out signaling pathways; overall score OS = weighted sum of 5 criteria (downward paths DP, edge crossings EC, edge length EL, node distribution ND, node-edge distance NED), weights = "priorities assigned by the requester" (ASSERTED, not fitted). SA baseline AND hybrid Crowd-SA: SA continues from crowd layouts and raises the score. 360 crowd workers, 3 networks, multiple workers per network => same pathway laid out by many humans. Quote: "simulated annealing in Crowd-SA100 achieved 1,875% more average improvement in total score than in SA" (for G3).
