# cjitter: four stochastic searches in C, and the control that says whether any of them helped

### It runs uniform random sampling at the same budget and tells you when your search did no better

You supply a fitness function over a box of real variables, lower being better, and a budget in
evaluations. Four methods spend that budget:

    random    draw uniformly from the box. The control.
    climb     jitter a neighbour, keep it if better, restart when stuck.
    anneal    the same, but accept a worse neighbour with a probability that decays.
    ga        population, tournament selection, blend crossover, jittered mutation.

    make
    ./labels          # 90 rectangles in a container, minimise overlap
    ./erd             # place new tables on an existing diagram

No dependencies beyond libm. Deterministic from a seed.

## Why the control is the point

Most optimisation libraries will spend a million evaluations and never mention that uniform
sampling would have done as well. `cjitter_compare` runs all four methods at the same budget over
several seeds and reports which beat the control, where "beat" means the method's median is better
than the control's luckiest seed. Anything short of that is inside the luck.

On the label problem, at 36% area coverage:

    method         median       spread  vs random
    random        187.046      22.2028 the control
    climb         36.9614      37.2604     better
    anneal        114.626      55.8446     better
    ga            192.247      41.6418  no better

The GA is no better than uniform sampling at equal cost. That is the sort of thing this library
exists to say, and it said it on the first run.

## The examples

**`example/labels.c`** places rectangles in a container with minimum overlap. Cheap, exact,
deterministic objective; hard constraint (stay inside) enforced by clamping in the repair
callback rather than by a penalty term, so it can never be traded against the objective.

**`example/erd/`** is the application this was written for. MySQL Workbench lays out an
entity-relationship diagram by heuristics and does it badly, and every reverse-engineering of the
schema scrambles the positions. Redrawing a 44-table diagram by hand costs about an hour.

The observation that makes it tractable: when a migration adds three tables, only those three need
placing. Freezing the rest is not a compromise for tractability -- a reader who knows where a table
sits should still find it there -- and it turns 88 free variables into 6.

    12 tables already placed, 3 added by a migration, 23 foreign keys.

    centroid        55887.3   (place each new table at its neighbours' centroid)

    method         median       spread  vs random
    random        16047.9      3584.17 the control
    climb         10464.1      5275.91     better
    anneal        11117.1      7029.57     better
    ga            12362.7      4060.81     better

All three searches beat the control here, and the obvious heuristic loses badly: placing a table at
its neighbours' centroid drops it on top of the edges running between them.

## Writing an objective that a search can follow

The tiers in the ERD example are worth copying. Edges passing through a table are scored by the
*length of the segment inside the rectangle*, not by a count: a count is flat under small moves, so
the search has nothing to follow and walks at random on the plateau. Crossings stay a count, at a
weight two orders of magnitude above edge length, so ties break toward a tidy diagram without any
weight having been guessed.

Node overlap and canvas bounds are hard constraints in the repair callback. A hard constraint
enforced by construction cannot trade itself off against the objective, and no infeasible point can
be returned as the best.

## Build and test

    make            build both examples
    make check      run them
    make pedantic   -pedantic -Wextra over every source; must be clean
    make clean

`-ffp-contract=off` is in the flags: a run must reproduce from its seed on another compiler and
another architecture, and gcc contracts `a*b+c` into one FMA by default even under `-std=c99`.

## Status

Working library, two examples, no unit suite yet and no CI. The `.mwb` reader is not written: the
ERD example builds its graph in code. `.mwb` is a zip around `document.mwb.xml`, and the first task
is mapping where the table objects and the diagram figure positions live and how they link.

## See also

- [linearr](https://github.com/Anode1/linearr): least squares in C, which says when a line is the
  wrong shape.
- [bpnn](https://github.com/Anode1/bpnn): a backpropagation network for the tables where it is,
  which says when not to trust the fit.

## License

BSD 2-Clause; see `LICENSE`.
