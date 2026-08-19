# cjitter: four stochastic searches in C, with uniform sampling as the control

### It warns when a search does no better than uniform random sampling at the same budget

You supply a fitness function over a box of real variables, lower being better, a budget in
evaluations, and, for hard constraints, a repair callback that moves a proposal into
feasibility before it is scored. One call optimizes it:

    cjitter_problem p = { n, lo, hi, my_fitness, my_repair, ctx };
    cjitter_budget  b = { 8000, 1, 0.1, 0 };   /* evals, seed, first move size, pop */
    cjitter_result  r = { 0 };                 /* zero it; x is the one field you set */
    r.x = best;                                /* your array of n doubles */
    cjitter_run("climb", &p, &b, &r);

No dependencies beyond libm. Deterministic from a seed on every platform. `make lib` builds
`libcjitter.a`; `make install` puts it and the one header under PREFIX.

The fit is any problem whose objective is cheap enough to call thousands of times over tens
to a few hundred variables, with no gradient on offer:

    placement and packing        labels, diagrams, floor plans, sensors, nesting
    calibration, inverse design  small branchy models, controller gains, cheap evaluators
    constants inside programs    thresholds and knobs tuned against a fast benchmark,
                                 hyperparameters of quick-to-train models
    permutations by random keys  ordering, assignment, small routing
    falsification                searching a program's input box for the distance to failure
    allocation under a repair    portfolio and budget weights the repair normalizes
    black-box fitting            game evaluation weights against an opponent suite,
                                 simulation likelihoods

Where it does not fit: objectives costing minutes (those want surrogate models), losses with
usable gradients, dimensions in the thousands.

Four methods spend the budget, and the comparison between them is the library's second half:

    random    draw uniformly from the box. The control.
    climb     jitter a neighbour, keep it if better, restart when stuck.
    anneal    the same, but accept a worse neighbour with a probability that decays.
    ga        population, tournament selection, blend crossover, jittered mutation.

    make
    ./labels          # 90 rectangles in a container, minimise overlap
    ./erd             # place new tables on an existing diagram

The methods' internal constants (patience, cooling, mutation) come from
`cjitter_tuning_default`; change the fields you mean to change and pass the struct to
`cjitter_run_tuned` or `cjitter_compare_tuned`. Every field is read literally, so
`ga_mutate = 0` is a real mutation ablation.

One of those fields is worth knowing about before you tune anything else. `block` is how many
variables a single proposal moves; the default is all of them. When the objective is a sum
over objects that interact weakly, a proposal that moves everything at once improves one
object and spoils another, and gets rejected for the spoiling. Setting `block` to the width
of one object, 2 for a point in the plane, steps the search through the problem an object at
a time, which is the mechanism this library is named after and can be worth an order of
magnitude in budget. It is not free: where the good moves adjust several objects together, a
narrow block cannot express them. `cjitter.h` says the rest, and `cjitter_compare_tuned`
measures it for your problem the way it measures everything else.

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

## When the objective is noisy

If your fitness returns a slightly different number each time it is called — a held-out error
from a training run, a simulation, anything sampled — then the smallest value a search
observes is not what the search found. It is the luckiest draw it took, and the search is the
thing that went looking for lucky draws.

That is not a small correction and it is not fair across methods, because how much luck a
method accumulates depends on how much it resamples one place, which is exactly what
distinguishes the methods being compared. Set `verify` in the tuning and the result carries
`verified`, the mean of that many fresh evaluations of the point actually returned, along with
`inflation`, the gap between the two. `cjitter_compare` then judges on `verified` and prints
the inflation beside each method.

    cjitter_tuning t = cjitter_tuning_default(n);
    t.verify = 30;                       /* 30 fresh evaluations of the answer */
    cjitter_compare_tuned(&p, &b, &t, seeds, stdout);

The verification evaluations are spent after the search and are not taken out of the budget,
so switching it on cannot shorten a search or move a trajectory. On a deterministic fitness
every draw is the same value and `verified` equals `best` exactly, so leaving it on costs
evaluations and changes nothing else. It is off by default.

`cjitter.h` gives the measurement behind this at the `cjitter_result` comment, including the
case where this library declared a method better at p = 0.002 that had not, in fact, beaten
the control.

## The examples

**`example/labels/`** places rectangles in a container with minimum overlap. This is the
problem the author solved for industry in 2001, deployed: label placement in a bounded area,
no edges between them, nothing allowed to intersect. Cheap, exact, deterministic objective;
staying inside the container is a hard constraint in the repair callback, and `cjitter.h`
says why that is not a penalty term. The 2001 system solved it with per-label random unit
steps kept when the summed overlap fell, a (1+1) strategy per label, and settled to zero
overlap in seconds whenever the labels fit at all.

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

The search itself is worth watching. The film is a smoothed replay of climb's improvements
at the shipped budget and seed, with `block` set to 2: the frozen diagram stands still, the
migration's ten tables glide from the first random draw into their neighbourhoods, and the
connectors re-route at every frame. `make movie` rebuilds it.

The block is why the film is watchable, and the reason is worth stating. At the default
block, one proposal moves all ten tables at once, so a proposal that seats one table beside
its neighbour usually unseats another and is rejected for it: the run accepts 42 proposals
out of 8000 and each one displaces the whole migration, which reads as ten tables
teleporting together. At `block` 2 the same climb accepts 158, each moving one table, and
finishes 31 percent lower (32428 against 46665). What the film shows is what the objective
was always asking for and the proposal shape could not express.

![The migration's tables settling into the frozen diagram](example/erd/erd_settle.gif)

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
that clean screen 20 crossings and 1944 penetration; the routed model 27 and 323. Every
score means only as much as its calibration line, and the run prints both:

    34 tables already placed, 10 added by a migration, 59 foreign keys.

    ---- straight diagonal edges ----

    centroid         215066   (the centroid rule: each new table at its neighbours' centroid)
    human            218207   (the human placement, where the maintainer put them)
               20 crossings, 1943.96 penetration under this edge model

    method         median        range    wins    sign-p   vs random
    random         235567      39777.5       -         - the control
    climb         90661.2        57623    5/5     0.0312      better
    anneal         127730      72009.9    5/5     0.0312      better
    ga            72070.3        14062    5/5     0.0312      better

    ---- orthogonal routed connectors ----

    centroid         187368   (the centroid rule: each new table at its neighbours' centroid)
    human            157552   (the human placement, where the maintainer put them)
               27 crossings, 323 penetration under this edge model

    method         median        range    wins    sign-p   vs random
    random        80747.1      31356.4       -         - the control
    climb         46317.4      4584.05    5/5     0.0312      better
    anneal        47725.2      5057.59    5/5     0.0312      better
    ga            38565.5         6006    5/5     0.0312      better

Those are the default tuning's numbers, one proposal moving all twenty variables. `./erd
--block 2` moves one table per proposal instead and reports, under the routed model:

    method         median        range    wins    sign-p   vs random
    random        80747.1      31356.4       -         - the control
    climb         32572.2      1791.87    5/5     0.0312      better
    anneal        32916.8      2128.04    5/5     0.0312      better
    ga            53003.4      18528.4    5/5     0.0312      better

Read the range column beside the medians. Climb's median falls 30 percent and its spread over
five seeds falls from 4584 to 1792, so the blocked search returns much the same layout
whichever seed it is given, which in a tool run once is worth as much as the median. The genetic algorithm
is the exception and it is not a bug: only its mutation is blocked while crossover still
blends every coordinate, so a narrow block leaves it a weak-mutation GA. On the label problem
the same change is decisive rather than incremental --- `./labels 90 20000 7 2` puts climb,
annealing and the GA all at exactly 0 on all seven seeds, the clean layout none of them
reaches at any budget with whole-vector proposals.

All three searches beat the control on every seed under both models, and the heuristic loses
under both: a table at its neighbours' centroid lands on the connectors running between its
neighbours. Read the human rows through the calibration lines: most of the human's score in
each section is that edge model failing to reproduce their real connectors, which is why even
the control's median outscores them in the routed section. The comparison that survives is
the feasibility pair. The routed seed-1 search layout reaches 0 penetration and 48 crossings
(23 at block 2); the human's routes to 323 and 27. Each is winning a different half of what the tool and the
person jointly achieved as 0 and 0, and closing that gap is the router's open problem, not
the search's. Connectors leave a table's border at that edge's own attachment point, spread
by the table's degree, so no two connectors ever share a segment: an edge joins two tables
and nothing else.

Each section ends with the layout one run at seed 1 found and its feasibility under that
model, pinned digit for digit by `tests/cli.sh`. `./erd --svg > erd.svg` draws four states
stacked with their actual routes: the scramble a reverse-engineering leaves (the state that
used to cost the hour), the centroid initial, the search's final, and the human's reference,
the frozen 34 identical in the last three. `--svg-straight` draws the same four with straight
edges.

## The objective

The tiers in the ERD example are worth copying. A connector passing through a table is scored
by the *length of the overlap*, not by a count: a count is flat under small moves, so the
search has nothing to follow and walks at random on the plateau. Crossings stay a count, at a
weight two orders of magnitude above connector length, so no weight has to be guessed. And
when the medium being scored is not a straight line, score the medium: the ERD objective
routes every connector before reading it, and prints how well that router reproduces the one
layout whose on-screen quality is known.

One caveat on tiering, measured rather than assumed. A weight two orders of magnitude down
does not make a term a tie-breaker, because a tier's weight and a tier's magnitude are
different things. Once the ERD searches drive penetration to zero, which all of them do, the
surviving score is a few dozen crossings at 100 against sixty connectors of a few hundred
units each: at the search's own optimum, length is 81 to 90 percent of everything that
varies, and the layout it picks is the one length prefers. Tier by orders of magnitude, then
go and look at what the tiers actually weigh at the answer.

Node overlap and canvas bounds are hard constraints in the repair callback, for the reason
`cjitter.h` gives at the `cjitter_repair` typedef. That reason has a precondition worth
stating, because this example violated it for a year: putting a constraint in the repair
makes it hard *only if the repair actually enforces it*. This one pushed each new table out
of its neighbours in a single ordered walk, so a table repaired early could be shoved back
into an overlap by one repaired later, and nothing looked again. It shipped infeasible
layouts as answers, on four of five seeds, with tables through each other by up to 121 units.

The search was not merely tolerating that, it was hunting for it: overlap costs nothing in
the objective, and tables stacked on top of each other have short connectors, which is
exactly what an objective whose surviving term is length wants. A repair that fails silently
does not weaken a hard constraint, it hands the search a reward. The repair now relaxes the
whole layout until a sweep moves nothing and seats anything still stuck by searching outward
for the nearest free spot, and it keeps 12 units of clearance rather than zero, because 12 is
the tightest gap in the maintainer's own accepted diagram. If you put a constraint in a
repair, measure the returned answers against it; do not assume.

## Build and test

    make            build both examples
    make lib        libcjitter.a; make install puts it and cjitter.h under PREFIX
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
