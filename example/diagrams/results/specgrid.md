# The specification grid

specgrid.py runs the directional test for length, stress and alignment A1 over every
combination of radius (0.005, 0.01, 0.02, 0.05), direction count (8, 16, 32, 64),
reference-length rule (fit, median, rsqrt for the distance terms) and route treatment
(stored, chords), hand and neato, all three corpora: 1,008 cells, specgrid.csv.

Under the fitted L, no cell flips a verdict: hand length and stress read exactly 0.00
in all 192 of their fitted-L cells, hand A1 stays above every neato A1 cell at the same
radius and direction count, and neato's stress stays at 0.90 or above (its sub-1.00
cells are all at d = 0.005, the radius sweep's documented left edge). The only cells
where a hand distance term rises above 0.05 are median-L cells at d = 0.02 and 0.05 (up
to 0.17), the misfit the Reference length paragraph already prices; the same cells under
the fitted L read 0.00.
