# Building an objective a search can follow

Advice extracted from the diagram example, stated generally because none of it is specific
to diagrams.

## Score by penetration, not by a count

A connector passing through a table is scored by the *length of the overlap*, not by a
count: a count is flat under small moves, so the search has nothing to follow and walks at
random on the plateau. Where a count is unavoidable, keep a continuous term beside it.
Crossings in the diagram example stay a count, and connector length is the continuous term
beside them; the plateau is still visible in the film, where the count stops falling at
evaluation 2842 of 8000 and the rest of the budget shortens connectors.

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
different things. The diagram objective priced crossings at 100 and raw connector length at
1, and once the searches drove penetration to zero, which all of them did, the surviving
score was a few dozen crossings at 100 against sixty connectors of a few hundred units each:
at the search's own optimum, length was 81 to 90 percent of everything that varied, and the
layout it picked was the one length preferred, 47 crossings on screen under a film caption
that counted 23. The opposite tiering failed the opposite way: with a crossing priced above
any length, the search sent tables to the corners of the canvas, where a connector along the
border crosses nothing. The score that holds is priced in one unit from the diagram itself:
a crossing costs one connector of the frozen edges' mean length, and each connector costs
its length squared over that mean, so a nudge is cheap, an exile costs a dozen crossings,
and nothing is a tie-breaker. Over the same fifteen seeds the drawings climb returns went
from 39 visible crossings to 26. Tier if you must, then go and look at what the tiers
actually weigh at the answer, and look at the drawing.

## Price what the person keeps

Length alone, at any price, wedged every new table into the nearest cluster at the repair's
12 units with a quarter of the canvas empty, because the repair's gap is the tightest pair
in the maintainer's diagram and not their habit: the median nearest clearance between its
44 tables is 53. A room term, the shortfall of a new table's clearance below 50, priced at
three units of connector per unit, is what backs a table off its neighbours. The number came
from measuring the diagram, and the term exists because the drawing was wrong while the
score was falling.

## Count what the reader sees

The crossing count excluded pairs of edges that share a table, the convention of
crossing-number theory, where an optimal drawing can be assumed to have none. Routed
connectors leave fixed attachment slots, and two edges out of one table cross far from it as
visibly as any other pair; on the diagram the exclusion hid 24 of 47 crossings on screen.
The rule is now the reader's: every proper crossing counts, and one under a table does not,
because the table is drawn over it and the segments beneath are already paid for as
penetration. A term named for what a person sees has to be measured where the person looks.

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
