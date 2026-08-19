# Building an objective a search can follow

Advice extracted from the diagram example, stated generally because none of it is specific
to diagrams.

## Score by penetration, not by a count

A connector passing through a table is scored by the *length of the overlap*, not by a
count: a count is flat under small moves, so the search has nothing to follow and walks at
random on the plateau. Where a count is unavoidable, keep a continuous nearness term beside
it. Crossings in the diagram example stay a count, at a weight two orders of magnitude above
connector length, so no weight has to be guessed.

## Score the medium the reader sees

When the thing being scored is not a straight line, score the thing. The diagram objective
routes every connector orthogonally before reading it, because that is what the tool draws;
an objective over straight center-to-center segments scores penetrations and crossings that
do not exist on screen and misses ones that do. A routed metric is an approximation, so it
is calibrated against the one certain fact available, a human layout that achieved zero
crossings and zero penetration on screen, and the run prints the router's shortfall beside
every number that depends on it.

## A weight is not a magnitude

One caveat on tiering, measured rather than assumed. A weight two orders of magnitude down
does not make a term a tie-breaker, because a tier's weight and a tier's magnitude are
different things. Once the diagram searches drive penetration to zero, which all of them do,
the surviving score is a few dozen crossings at 100 against sixty connectors of a few
hundred units each: at the search's own optimum, length is 81 to 90 percent of everything
that varies, and the layout it picks is the one length prefers. Tier by orders of magnitude,
then go and look at what the tiers actually weigh at the answer.

## A constraint in the repair is hard only if the repair enforces it

Node overlap and canvas bounds are hard constraints in the repair callback, for the reason
`cjitter.h` gives at the `cjitter_repair` typedef. That reason has a precondition worth
stating, because the diagram example violated it for a year: putting a constraint in the
repair makes it hard *only if the repair actually enforces it*. That one pushed each new
table out of its neighbours in a single ordered walk, so a table repaired early could be
shoved back into an overlap by one repaired later, and nothing looked again. It shipped
infeasible layouts as answers, on four of five seeds, with tables through each other by up
to 121 units.

The search was not merely tolerating that, it was hunting for it: overlap costs nothing in
the objective, and tables stacked on top of each other have short connectors, which is
exactly what an objective whose surviving term is length wants. A repair that fails silently
does not weaken a hard constraint, it hands the search a reward. The repair now relaxes the
whole layout until a sweep moves nothing and seats anything still stuck by searching outward
for the nearest free spot, and it keeps 12 units of clearance rather than zero, because 12
is the tightest gap in the maintainer's own accepted diagram. If you put a constraint in a
repair, measure the returned answers against it; do not assume.
