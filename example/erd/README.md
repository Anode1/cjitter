# The diagram example: a real schema's migration, placed into a frozen drawing

Laying out a diagram with the fewest edge crossings is NP-complete (Garey and Johnson,
*Crossing Number is NP-Complete*, SIAM J. Algebraic Discrete Methods 4:312-316, 1983), so
there is no polynomial-time exact algorithm unless P = NP, and every drawing tool runs
heuristics. MySQL Workbench offers one unparameterised command for it, `Arrange > Autolayout`,
documented in full as "Automatically arranges objects on the canvas": no scope, no options,
no way to hold a table where it is. Oracle SQL Developer Data Modeler offers the same one
button, whose documented remedy for a bad result is Undo. Neither states what becomes of
hand-placed positions when the model is refreshed from the database, and Workbench's
reverse-engineering builds a *new* auto-placed diagram rather than merging into yours.
Restoring this 44-table diagram by hand cost the author about an hour each time. That
recurring hour is the problem this example solves.

Freezing a subset and optimising the rest is not new and this example does not claim it.
It is Brandes and Wagner's stability term (GD 1997), written out by Brandes and Mader as
`(1-a) * stress + a * sum_i phi_i * |p_i - p_i0|^2`, of which a hard freeze is the
`phi_i -> infinity` limit; Frishman and Tal named the step "pinning weight" in 2007. It
ships as `pin=true` in Graphviz neato, `org.eclipse.elk.stress.fixed` in ELK, `fixed=` in
NetworkX, `fx`/`fy` in d3-force, `respectFixedPosition` in draw.io, and `PartialLayouter`
in yFiles. What none of them models is the routed edge: they minimise straight-line stress,
while an ERD draws orthogonal polylines with waypoints, which is what this objective scores
and what `data/*_chords.txt` measures the difference of.

The graph is real: an anonymized production schema, 44 tables and the 59 foreign-key edges on
its diagram (`data/`, provenance and anonymization in `data/PROVENANCE.md`). The last
migration added ten tables. The observation that makes the problem tractable: only those ten
need placing. Freezing the rest is no compromise, since a reader who knows where a table sits
should still find it there, and it turns 88 free variables into 20.

## The score

Three terms, tiered so that the tiers are lexicographic in fact: connector length lying
inside a table, in canvas units, at 100; crossings at 100 each; connector length at 1 per
canvas half-perimeter, so the whole drawing's length is under 60 and never outweighs one
crossing. A crossing counts when a reader sees it. Two edges sharing a table are priced like
any other pair, since two connectors leaving one table at their own attachment slots cross
far from it as visibly as any; a crossing under a table is not priced, since the table is
drawn over it and the segments beneath are already paid for as penetration. Node overlap and
the canvas are hard constraints in the repair callback.

Both rules replaced earlier ones, and the film is where the difference showed. Raw length at
1 against crossings at 100 made length 81 to 90 percent of everything that varied once
penetration reached zero, so the search shortened connectors by adding crossings; and the
crossing-number convention of not pricing adjacent pairs hid 24 of the 47 crossings on the
film's last frame while its caption counted 23. Measured over the same fifteen seeds, climb at
`block` 2 returned drawings with 39 visible crossings on average under the old score and 25
under this one.

## The film

The search itself is worth watching. The film is a smoothed replay of climb's improvements
at the shipped budget and seed, with `block` set to 2: the frozen diagram stands still, the
migration's ten tables glide from the first random draw into their neighbourhoods, and the
connectors re-route at every frame. The caption carries the score and the three terms it is
made of, so what falls is a count: 3016 units of connector under tables and 67 crossings at
the first draw, penetration gone by evaluation 713, crossings at 28 from evaluation 1038,
and the remaining 7000 evaluations shortening connectors within that count. The fifteen-seed
panel's median is 24 crossings, reached by nine seeds; seed 1 is one of the six that stall
above it. `make movie` rebuilds it.

The block is why the film is watchable, and the reason is worth stating. At the default
block, one proposal moves all ten tables at once, so a proposal that seats one table beside
its neighbour usually unseats another and is rejected for it: the run improves 30 times in
8000 evaluations and each improvement displaces the whole migration, which reads as ten
tables teleporting together. At `block` 2 the same climb improves 126 times, each moving one
table, and ends at 28 crossings against 45. What the film shows is what the objective was
always asking for and the proposal shape could not express.

![The migration's tables settling into the frozen diagram](erd_settle.gif)

Both revisions are in the repository, drawn by `data/render.py` from the extracted geometry.
Neither image is a Workbench export; an export would carry the real names. The diagram before
the migration:

![Before the migration: 36 tables in the human's layout](data/ERD_prev_routed.png)

and after it, the ten added tables in amber:

![After the migration: 44 tables, the ten added in amber](data/ERD_routed.png)

## Two edge models, one boolean

There are two ways to draw an edge, and the example runs its whole experiment under each,
toggled by one boolean in the code. The straight diagonal segment is the general-graph
representation, and it is what surprisingly rich tools draw for ERDs; the orthogonal routed
connector, an L or a Z with the middle segment sliding across the channel the way a person
nudges a connector past an obstacle, is what Workbench draws and what a reader sees. The
same current revision, drawn with straight edges this time; the routed drawing of it is just
above:

![The diagonal representation: straight center-to-center edges](data/ERD_straight.png)

Each edge model is calibrated against the one certain fact about the human's layout: it
achieved zero crossings and zero edges under a table on screen. The straight model charges
that clean screen 20 crossings and 1944 penetration; the routed model 32 and 323. Every
score means only as much as its calibration line, and the run prints both:

    34 tables already placed, 10 added by a migration, 59 foreign keys.

    ---- straight diagonal edges ----

    centroid         195619   (the centroid rule: each new table at its neighbours' centroid)
    human            196401   (the human placement, where the maintainer put them)
               20 crossings, 1943.96 penetration under this edge model

    method         median        range    wins    sign-p      holm   vs random
    random         190985      51337.1       -         -         - the control
    climb         69360.1      72832.6   15/15  3.05e-05  9.16e-05      better
    anneal        96896.5      65128.6   15/15  3.05e-05  9.16e-05      better
    ga            50470.7      12399.9   15/15  3.05e-05  9.16e-05      better

    ---- orthogonal routed connectors ----

    centroid         171750   (the centroid rule: each new table at its neighbours' centroid)
    human            125781   (the human placement, where the maintainer put them)
               32 crossings, 323 penetration under this edge model

    method         median        range    wins    sign-p      holm   vs random
    random        42108.4      37775.6       -         -         - the control
    climb         5309.83      11275.6   15/15  3.05e-05  9.16e-05      better
    anneal        6008.26        19401   15/15  3.05e-05  9.16e-05      better
    ga            4608.25      16602.3   15/15  3.05e-05  9.16e-05      better

A routed score reads as a crossing count: 100 per crossing, the eleven among the frozen
connectors included, plus a few units of length. Climb's median of 5310 is 53 crossings on
screen; the human's 125781 is mostly 1227 units of connector under tables, 1082 of them
frozen connectors their ten tables sit on, which the routing the score reads does not move.

This example runs fifteen seeds, not five. Three methods are tested against the one
control, so the verdict column is Holm-corrected, and the smallest corrected value a
five-seed panel can reach is 3 x 0.0312 = 0.0938: five seeds cannot certify anything here
whatever the data. Fifteen puts the floor at 9.16e-05 and the verdicts stand. The panel, not
the method, was the binding constraint, which is worth knowing before reading any table of
this shape.

Those are the default tuning's numbers, one proposal moving all twenty variables. `./erd
--block 2` moves one table per proposal instead and reports, under the routed model:

    method         median        range    wins    sign-p      holm   vs random
    random        42108.4      37775.6       -         -         - the control
    climb          2406.6      400.759   15/15  3.05e-05  9.16e-05      better
    anneal        3007.43      1200.64   15/15  3.05e-05  9.16e-05      better
    ga            5207.77      18599.6   14/15  0.000488  0.000488      better

Read the range column beside the medians. Climb's median falls from 53 crossings to 24 and
its spread over the fifteen seeds from 11276 to 401, four crossings, so the blocked search
returns much the same drawing whichever seed it is given, which in a tool run once is worth
as much as the median. The genetic algorithm is the exception and it is not a bug: only its
mutation is blocked while crossover still blends every coordinate, so a narrow block leaves
it a weak-mutation GA, and here it loses ground. On the label problem the same change is
decisive rather than incremental: `./labels 90 20000 7 2` puts climb, annealing and the GA
all at exactly 0 on all seven seeds, the clean layout none of them reaches at any budget
with whole-vector proposals.

All three searches beat the control on every seed under both models at the default block,
and the heuristic loses under both: a table at its neighbours' centroid lands on the
connectors running between its neighbours. Read the human rows through the calibration
lines: most of the human's score in each section is that edge model failing to reproduce
their real connectors, which is why even the control's median outscores them in the routed
section. The comparison that survives is the feasibility pair. The routed seed-1 search
layout reaches 0 penetration and 45 crossings (28 at block 2, the panel's median 24); the
human's routes to 323 and 32. Each is winning a different half of what the tool and the
person jointly achieved as 0 and 0, and closing that gap is the router's open problem, not
the search's. Connectors leave a table's border at that edge's own attachment point, spread
by the table's degree, so no two connectors ever share a segment: an edge joins two tables
and nothing else. The router's detours are not confined to the canvas, and a connector
that circles the drawing along its border to cross nothing is what a crossing-first score
asks for; length breaks the tie only among routes with the same count.

## What the human row is worth

Scoring a search against the maintainer assumes the maintainer's own placement is
distinguishable from a random draw. On this diagram it is not. `data/null_check.py` redraws
the same ten tables uniformly in the canvas box 2,000 times and reports where the maintainer
sits in the resulting null. P is the fraction of draws scoring at least as well, so 0.5 is
no signal and a small P is signal.

    criterion                                maintainer   random med        P
    aligned to frozen lines, tol 5                   12           14    0.915
    exact ties, tol 1                                 4            4    0.690
    mutual alignment among the ten, tol 5             0            0    1.000
    alignment to FK neighbours, tol 5                 1            0    0.174
    rms spread from own centroid                    864        871.3    0.478
    box overlap area                                  0    6.969e+04    0.000

Only overlap separates the two, and the search hands its own control that same property
through the repair callback. With 34 frozen tables there are about 100 candidate alignment
lines on each axis of a 2,889 by 1,916 canvas, so the base rate swamps whatever the ten
could contribute: a random draw sits on 14 of the 20 axes where the maintainer sits on 12.
Seeds 2 and 3 give 0.918 and 0.908 on that row.

The human row therefore calibrates the edge model and does nothing else. This diagram is not
a standard an objective can be scored against, and the alignment term that holds hand-drawn
layouts across the corpora in `example/diagrams` does not rank this maintainer above random
either. The example demonstrates the library on a real graph; it is not a benchmark.

Each section ends with the layout one run at seed 1 found and its three terms as scored,
pinned digit for digit by `tests/cli.sh`. `./erd --svg > erd.svg` draws four states stacked
with the routes the score read, frozen connectors where the frozen diagram left them, so a
new table parked on one is drawn on it: the scramble a reverse-engineering leaves (the state
that used to cost the hour), the centroid initial, the search's final, and the human's own
placement, the frozen 34 identical in the last three. `--svg-straight` draws the same four
with straight edges.

The general lessons this example taught, tiering, scoring the medium, and the repair defect
that shipped infeasible layouts for a year, are collected in
[docs/objective.md](../../docs/objective.md).

## The data, and the .mwb format

The graph is carried as data (`data/`): an anonymized `.mwb`, both revisions rendered to PNG,
and the extracted geometry as JSON. `null_check.py` is the null above; `gen_data.py`
extracted the geometry and `render.py` draws the PNGs. A C reader for `.mwb` files is not written. The format is
a zip around `document.mwb.xml`; the figure positions live in `workbench.physical.TableFigure`
objects (the attribute order is `type="real" key="left"`, which matters when grepping),
foreign keys in `db.mysql.ForeignKey` objects whose `owner` link names the child table.
