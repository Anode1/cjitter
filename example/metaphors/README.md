# The metaphor audit: the paired test a 1.4-million-run benchmark never ran

Vermetten, Doerr, Wang, Kononova and Bäck (GECCO 2024, DOI 10.1145/3638529.3654122)
benchmarked the four metaphor-heavy libraries (mealpy, Opytimizer, NiaPy, EvoloPy),
282 implementations by the paper's count and 296 in its released data, plus twelve
baselines, on the 24 BBOB functions, ranked by an anytime measure, with no
statistical test anywhere. Their raw data is public (Zenodo
10.5281/zenodo.10561215, CC-BY), so the missing test is computable from their own
release: pair every implementation with random search per (function, instance,
repetition) at their own budgets, `cjitter_sign_p` on the paired wins,
`cjitter_holm` across the implementations tested at once.

At the top budget of 10000 x dim evaluations, 98/79/72/66 of the 296 (dimensions
2/5/10/20) are not shown better than uniform sampling at equal cost, and 79/66/58/53
of those are shown strictly worse. The count worse than random is lowest at middle
budgets and rises to the top budget in every dimension: a search that stalls loses to
a control that keeps sampling. The same algorithm name carries opposite verdicts
across libraries, differential evolution included, so the verdict attaches to the
implementation, never to the metaphor. The paper, *The Test the Metaphor Benchmark
Never Ran*, and its pre-registration are
[articles/cjitter](https://github.com/Anode1/articles/tree/main/cjitter); the audit
was frozen there before the confirmatory numbers were computed.

This directory is the machinery, not the data. The pipeline, run from a working
directory holding the Zenodo files:

    python3 condense.py          # Processed_data.zip -> fb.npz + fb_names.json
    python3 export_flat.py       # fb.npz -> fb_flat.txt + fb_algs.txt
    cc -std=c99 -ffp-contract=off -O2 -I $CJITTER/c -o audit $CJITTER/example/metaphors/audit.c \
       $CJITTER/libcjitter.a -lm
    ./audit fb_algs.txt fb_flat.txt > verdicts.tsv 2> summary.txt

`audit` prints one verdict row per (implementation, dimension, budget), the summary
counts per cell on stderr, and finishes the whole 10.2-million-row table in under 20
seconds. By default the control is the release's own RandomSearch, which is
nevergrad's and samples a Gaussian through its bound transform, not uniformly.
`uniform.c` generates the true uniform control, `cjitter_run("random")` on the same
functions through coco-experiment's C evaluator (fetch `cocoex-c-*.zip` from the
coco-experiment GitHub releases, unpack as `coco/cocoex-c`):

    cc -std=c99 -ffp-contract=off -O2 -I coco/cocoex-c -I $CJITTER/c -o uniform \
       $CJITTER/example/metaphors/uniform.c coco/cocoex-c/coco.c $CJITTER/libcjitter.a -lm
    ./uniform > uniform_control.csv
    ./audit fb_algs.txt fb_flat.txt --control-csv uniform_control.csv
    python3 trend_fig.py summary.txt > trend.tex   # the paper's budget-trend figure

The uniform run is 444 million function evaluations, about 13 minutes, deterministic
from the seed rule declared in `uniform.c`. Only 121 of 7,104 verdicts differ between
the two controls, which is itself one of the paper's findings.

This audit is what forced the pooled-panel fix to `cjitter_sign_p`: the plain
binomial sum overflows past 1028 pairs, and the first 1200-pair panel returned NaN.
The scaled sum that replaced it is pinned in `tests/tests.c` against exact rational
references.
