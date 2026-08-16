# cjitter: four stochastic searches in C, with uniform sampling as the control

### It warns when a search does no better than uniform random sampling at the same budget

You supply a fitness function over a box of real variables, lower being better, and a budget in
evaluations. Four methods spend that budget:

    random    draw uniformly from the box. The control.
    climb     jitter a neighbour, keep it if better, restart when stuck.
    anneal    the same, but accept a worse neighbour with a probability that decays.
    ga        population, tournament selection, blend crossover, jittered mutation.

    make
    ./labels          # 90 rectangles in a container, minimise overlap
    ./erd             # place new tables on an existing diagram

No dependencies beyond libm. Deterministic from a seed on every platform. The methods'
internal constants (patience, cooling, mutation) come from `cjitter_tuning_default`; change
the fields you mean to change and pass the struct to `cjitter_run_tuned` or
`cjitter_compare_tuned`. Every field is read literally, so `ga_mutate = 0` is a real
mutation ablation.

## The control

Most optimisation libraries will spend a million evaluations and never mention that uniform
sampling would have done as well. `cjitter_compare` runs all four methods at the same budget
on the same seeds and reports, per method, the median, the range, the per-seed wins against
the control, and the exact one-sided sign-test probability of that many wins under a fair
coin. "better" is declared only when that probability is at or under 5%; "not shown" is a
failure to demonstrate improvement, and says nothing about equality.

On the label problem, at 36% area coverage:

    method         median        range    wins    sign-p   vs random
    random        189.166      35.3242       -         - the control
    climb         12.8278       39.717    7/7    0.00781      better
    anneal        127.625      35.4232    7/7    0.00781      better
    ga            1.38243      3.55264    7/7    0.00781      better

The control has already earned its keep once. The first GA shipped here mutated at a fixed
scale, so every generation re-scattered whatever the population had converged to, and the
table read "ga 195.6 against random's 187.0": no better than uniform sampling at equal cost.
An outside review traced that verdict to the fixed scale; the mutation now decays over the
run, and the same GA is the best method on the problem. Both halves of that story are the
library working as intended.

## The examples

**`example/labels.c`** places rectangles in a container with minimum overlap. This is the
problem the author solved for industry in 2001, deployed: label placement in a bounded area,
no edges between them, nothing allowed to intersect. Cheap, exact, deterministic objective;
staying inside the container is a hard constraint in the repair callback, and `cjitter.h`
says why that is not a penalty term.

**`example/erd/`** is the application this was written for. Laying out a diagram with the
fewest edge crossings is NP-complete (Garey and Johnson, *Crossing Number is NP-Complete*,
SIAM J. Algebraic Discrete Methods 4:312-316, 1983), so there is no polynomial-time exact
algorithm unless P = NP, and every drawing tool runs heuristics. MySQL Workbench's heuristics
do it badly, every reverse-engineering of the schema scrambles the positions, and restoring
this 44-table diagram by hand after each one cost the author about an hour. That recurring
hour is the problem this example solves.

The graph is real: an anonymized production schema, 44 tables and the 59 foreign-key edges on
its diagram (`example/erd/data/`, provenance and anonymization in `data/PROVENANCE.md`). The
last migration added ten tables. The observation
that makes the problem tractable: only those ten need placing. Freezing the rest is no
compromise, since a reader who knows where a table sits should still find it there, and it
turns 88 free variables into 20.

Both revisions are in the repository, drawn by `data/render.py` from the extracted geometry.
Neither image is a Workbench export; an export would carry the real names. The diagram before
the migration:

![Before the migration: 36 tables in the human's layout](example/erd/data/ERD_prev_routed.png)

and after it, the ten added tables in amber:

![After the migration: 44 tables, the ten added in amber](example/erd/data/ERD_routed.png)

There are two ways to draw an edge, and the example runs its whole experiment under each,
toggled by one boolean in the code. The straight diagonal segment is the general-graph
representation, and it is what surprisingly rich tools draw for ERDs; the orthogonal routed
connector, an L or a Z with the middle segment sliding across the channel the way a person
nudges a connector past an obstacle, is what Workbench draws and what a reader sees. The
same current revision, drawn with straight edges this time; the routed drawing of it is just
above:

![The diagonal representation: straight center-to-center edges](example/erd/data/ERD_straight.png)

Each edge model is calibrated against the one certain fact about the human's layout: it
achieved zero crossings and zero edges under a table on screen. The straight model charges
that clean screen 29 crossings and 1944 penetration; the routed model 36 and 422. Every
score means only as much as its calibration line, and the run prints both:

    34 tables already placed, 10 added by a migration, 59 foreign keys.

    ---- straight diagonal edges ----

    centroid         251673   (place each new table at its neighbours' centroid)
    human            231877   (where the human actually put them)
               29 crossings, 1943.96 penetration under this edge model

    method         median        range    wins    sign-p   vs random
    random         258611        23778       -         - the control
    climb          126176      52628.2    5/5     0.0312      better
    anneal         134884      55274.9    5/5     0.0312      better
    ga            85123.4        16702    5/5     0.0312      better

    ---- orthogonal routed connectors ----

    centroid         207656   (place each new table at its neighbours' centroid)
    human            131154   (where the human actually put them)
               36 crossings, 422 penetration under this edge model

    method         median        range    wins    sign-p   vs random
    random         108472      37413.6       -         - the control
    climb         58150.6      37085.2    5/5     0.0312      better
    anneal        63407.2      20087.3    5/5     0.0312      better
    ga            59542.6      25886.3    5/5     0.0312      better

All three searches beat the control on every seed under both models, and the heuristic loses
under both: a table at its neighbours' centroid lands on the connectors running between its
neighbours. Read the human rows through the calibration lines: most of the human's score in
each section is that edge model failing to reproduce their real connectors, which is why even
the control's median outscores them in the routed section. The comparison that survives is
the feasibility pair. The routed seed-1 search layout reaches 0 penetration and 63 crossings;
the human's routes to 422 and 36. Each is winning a different half of what the tool and the
person jointly achieved as 0 and 0, and closing that gap is the router's open problem, not
the search's.

Each section ends with the layout one run at seed 1 found and its feasibility under that
model, pinned digit for digit by `tests/cli.sh`; `./erd --svg > erd.svg` draws the three
states stacked with their actual routes, the frozen 34 identical in all three.

## The objective

The tiers in the ERD example are worth copying. A connector passing through a table is scored
by the *length of the overlap*, not by a count: a count is flat under small moves, so the
search has nothing to follow and walks at random on the plateau. Crossings stay a count, at a
weight two orders of magnitude above connector length, so ties break toward a tidy diagram
without any weight having been guessed. And when the medium being scored is not a straight
line, score the medium: the ERD objective routes every connector before reading it, and
prints how well that router reproduces the one layout whose on-screen quality is known.

Node overlap and canvas bounds are hard constraints in the repair callback, for the reason
`cjitter.h` gives at the `cjitter_repair` typedef.

## Build and test

    make            build both examples
    make check      ut + cliut -- the commit gate
    make ut         unit suite: refusals, the exact budget, determinism, box and repair
    make cliut      black-box: the built examples through a shell, exit codes and reproducibility
    make ut-asan    both suites under AddressSanitizer
    make ut-ubsan   both suites under UBSan
    make pedantic   -pedantic -Wextra over every source; must be clean
    make clean

CI runs everything on Linux and macOS, six compiler/flag pairs, and a job that builds four
ways and compares both examples' output byte for byte; `AGENTS.md` states the exact coverage
and its limits.

`-ffp-contract=off` is in the flags, and no search trajectory calls libm beyond `fabs` and
`sqrt`; `cjitter.h` states that discipline and the reason for it.

## Status

Working library, two examples, both suites and CI. The ERD example's graph is the real schema,
carried as data (`example/erd/data/`): an anonymized `.mwb`, both revisions rendered to PNG,
and the extracted geometry as JSON. A C reader for `.mwb` files is not written. The format is
a zip around `document.mwb.xml`; the figure positions live in `workbench.physical.TableFigure`
objects (the attribute order is `type="real" key="left"`, which matters when grepping),
foreign keys in `db.mysql.ForeignKey` objects whose `owner` link names the child table.

## See also

- [linearr](https://github.com/Anode1/linearr): least squares in C, which says when a line is the
  wrong shape.
- [bpnn](https://github.com/Anode1/bpnn): a backpropagation network for the tables where it is,
  which says when not to trust the fit.

## License

BSD 2-Clause; see `LICENSE`.
