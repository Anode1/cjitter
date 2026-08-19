# AGENTS.md -- how to develop cjitter (for humans and AI agents)

`cjitter` is four stochastic searches over a box of real variables, in **C99**, behind one fitness
interface, with uniform random sampling as a control that runs at the same budget. It is the third
of three: [linearr](https://github.com/Anode1/linearr) fits the line, [bpnn](https://github.com/Anode1/bpnn)
fits the curve, and each says when not to believe itself. This one searches, with the control run
beside it so it can say when the search was luck.

## Why it exists

Two real problems, twenty-five years apart. Label placement in a bounded rectangle (2001,
deployed), and placing tables added by a migration onto an existing MySQL Workbench diagram, which
Workbench does badly and which costs about an hour by hand on a 44-table schema.

The name is jitter, the regulariser from the author's 1997 thesis and the mechanism that made the
2001 layout settle. It names what all four methods share, which is why it does not name one of
them: an earlier working title, `gaop`, would have baked in a misattribution, because what
solved the 2001 problem was called a genetic algorithm and was in fact per-label descent
under the one summed cost: each label nudged a pixel at a time in the direction of least
intersection with its neighbours, with a little noise on the step, the cycle stopping when
the summed intersection area reached zero. On 2001 hardware it took a few seconds: the
labels retracted from every neighbour and the space filled. Started from one corner instead
of at random, they spread out like gas atoms; it behaved like a physical system. There was a
practical ceiling, found heuristically, a label density above which the cycle stopped
reaching zero. Descent
plus noise is jitter, which is why the name fits the whole family.

## The contract (read first)

- **`c/cjitter.h`** -- the interface, and the specification. A problem is `n`, a box, a fitness
  function and an optional repair callback. A budget is evaluations, a seed and a first move
  size. A tuning starts from `cjitter_tuning_default` and every field is read literally, so
  results only compare across runs that share one.
- **`README.md`** -- what the library promises and the measured output of both examples. Every
  number in it comes from running the shipped binaries; the code is the ground truth and the
  documents follow it.
- Hard constraints belong in `repair`, never in the fitness; `cjitter.h` says why at the
  `cjitter_repair` typedef.

## Build and test

    make            # both examples
    make lib        # libcjitter.a; install/uninstall honor PREFIX (default /usr/local)
    make check      # ut + cliut -- the commit gate
    make ut         # unit suite, 81 checks: the library's contract, called as functions
    make cliut      # black-box, 61 checks: the built examples through a shell
    make ut-asan    # both suites under AddressSanitizer
    make ut-ubsan   # both suites under UBSan
    make pedantic   # -pedantic -Wextra over every source; must be clean
    make clean

**`make ut` cannot see a refusal's exit code or message**, only a return value; those checks live
in `tests/cli.sh`, along with byte-for-byte reproducibility of a whole run. When something no
test caught gets through, the first question is which suite could have caught it. CI
(`.github/workflows/checks.yml`) is three separate jobs, not a cross-product: the full battery
on Linux and macOS with each platform's default compiler; `make check` across
gcc-12/gcc-13/clang at `-O0` and `-O2` on Linux only; and a reproducibility job that builds
four ways and compares both examples' output byte for byte. `-march=native` is in that matrix
because it is the flag that exposes FMA contraction. macOS never sees gcc and the sanitizers
only ever run under the default compiler; do not read more coverage into it than that.

`-ffp-contract=off` is in the flags. gcc contracts `a*b+c` into one FMA by default even under
`-std=c99`, and x86-64 baseline hides it only because SSE2 has no FMA instruction; without the flag
a run stops reproducing the moment anyone builds with `-march=native` or on arm64.

## Module map

    c/cjitter.h     the interface
    c/cjitter.c     the four searches and cjitter_compare
    c/rng           deterministic xorshift64*, period 2^64-1. The 32-bit generator it replaced
                    had compare's seed streams overlapping after 1.6e8 draws, a budget one
                    command-line argument could reach; its bpnn ancestor once understated a
                    study's standard error by 2.5x the same way.
    example/labels/     rectangles in a container, minimum overlap
    example/sixty/      the sixty-draws question (QUESTIONS.md item 8 next door): what n
                        uniform draws actually buy per landscape, and the n at which each
                        method matches random's n = 59. Exploratory instrument, output
                        unpinned; confirmatory runs wait for a signed pre-registration.
                        Phase two wires the smbpann engine (a ~/bpnn checkout) in as the
                        constant-optimization landscape.
    example/erd/erd.c   tables added by a migration onto a frozen diagram, the experiment
                        run under both edge models (one style boolean); --svg draws it
    example/erd/data/   the real schema, anonymized: ERD.mwb, both revisions as PNG, the
                        geometry as JSON, and PROVENANCE.md for what the anonymization changed
    tests/tests.c       unit suite: refusals, the exact budget, determinism, box and repair
    tests/cli.sh        black-box: exit codes, refusal messages, runs byte-identical over reruns

## The verdict

`cjitter_compare` runs every method on the same seed panel and judges each against the control
by an exact one-sided sign test on the paired per-seed differences: "better" needs the wins to
be explainable by a fair coin with probability at most 5%, and "not shown" is a failure to
demonstrate, never a demonstrated equality. This is the third rule this project has had, and
the two burials are worth remembering. The first compared the margin against the control's
range and called a method that hit the optimum on every seed "inside noise", punishing it for
the control's variance. The second required the method's median to beat the control's luckiest
seed, which reads as strict but passes a truly-equal method one time in twelve at five seeds,
gets harder as seeds are added because a minimum only falls, and threw away the pairing the
shared panel had already bought.

The measured results live in README.md's two tables and are not restated here. The shape of
them: all three searches now beat the control on every seed of both examples, and the ERD
searches also beat the human's accepted layout, which says the objective misses what the
human optimizes, semantic grouping, aligned rows, room to grow, and never that the tool
out-draws a person.

## The objective

Score by penetration, not by a count. A connector passing through a table is scored by the
length of the overlap; a crossing count is flat under small moves, so the search has nothing to
follow and walks at random on the plateau. Where a count is unavoidable, add a continuous
nearness term beside it.

Tier the weights by orders of magnitude rather than tuning them. In the ERD example: penetration
and crossings at 100, connector length at 1.

Then check what the tiers weigh at the answer, because a weight is not a magnitude. This
objective was documented as one where "length only ever breaks ties", and that is true of the
human's layout and false of every layout a search returns: the searches drive penetration to
zero, and what is left is a few dozen crossings at 100 against sixty connectors of a few
hundred units each, so length is 81 to 90 percent of everything that varies and the answer is
the one length prefers. The decomposition is cheap to print and nobody printed it for a year.

Score the medium the reader sees. The ERD edges are routed orthogonally (L and Z shapes, the
middle segment sliding across the channel) before anything is measured, because that is what
the tool draws, and the router is calibrated against the one certain fact: the human's layout
achieved 0 crossings and 0 penetration on screen. The run prints the router's shortfall (27
and 323 at present, with border anchoring, per-edge attachment slots, and sequential routing
aware of every connector already placed), and no score comparison against the human means
more than that line allows. Closing the remainder, more bends and a grid router, is the open
problem the router owns.

## The noisy objective, and the verdict it manufactured

The third burial, and the worst of them, because it was in the verdict rather than beside it.
`keep` stores the smallest fitness OBSERVED. On a deterministic objective that is the value of
the returned point. On a noisy one it is the minimum of however many draws the search took near
that point, which is biased low, and the bias is not shared equally: on a sphere in ten
variables at noise sigma 20 and budget 4000, climb takes 559 of its evaluations within half a
unit of the point it returns and anneal 317, where random takes 1. The method that resamples
hardest collects the most luck, and resampling is what distinguishes the methods.

Measured on that sphere, 9 seeds, judged as the library judged it before 0.11.0 and then on the
noiseless value of the same returned points:

    method    reported            on what was actually delivered
    climb     9/9, p = 0.0020     7/9, p = 0.0898   not shown
    anneal    9/9, p = 0.0020     5/9, p = 0.5000   not shown
    ga        9/9, p = 0.0020     9/9, p = 0.0020   better

So the library whose reason for existing is to say when a search did no better than sampling
said "better" at p = 0.002 for two methods that had not. It also reported a median of -37 for a
function whose minimum is 0, and said nothing about that either.

`cjitter_tuning.verify` is the fix: n fresh evaluations of the RETURNED point, spent after the
search and not against the budget, whose running mean becomes `result.verified`, with
`result.inflation` the gap. compare judges on verified when it is set. At verify 30 the three
verdicts above come back to 0.0898, 0.5000 and 0.0020, matching the truth exactly.

Three things the implementation had to get right, each learned by a test failing. The
verification evaluations must not go through `score()`, whose budget guard would abort. The
mean must be a RUNNING mean, because summing k copies of a value and dividing by k does not
return that value (0.1 added 25 times is not 2.5) and the promise that the check is inert on a
deterministic fitness has to be exact. And the field belongs in the tuning, not the budget:
`cjitter_tuning` must come from `cjitter_tuning_default`, where a budget is routinely filled
field by field, so a field added to the budget is uninitialised garbage in every caller that
already exists. The cli suite caught that one within a minute of the first build.

This is the prerequisite for QUESTIONS.md item 1 next door. A sweep for the noise level at
which search stops beating random, run through an estimator whose bias is created by that same
noise, would have measured its own artifact.

## The repair, and why it is the thing to check first

A hard constraint in `repair` is hard only if the repair enforces it, and `example/erd`'s did
not for a year. It pushed each new table out of its neighbours in one ordered walk of four
passes, so a table repaired early could be shoved back by one repaired later with nothing
looking again, and a clamp back onto the canvas could re-seat a table inside a neighbour. It
returned infeasible layouts as best on four of five seeds, tables through each other by up to
121 units; the centroid heuristic's own layout overlapped by 91; 965 of 1000 random layouts
came out of it still overlapping, every residual one a new table inside a FROZEN table, which
is the case the pairwise push cannot solve because the frozen side will not yield.

What made it costly rather than cosmetic: overlap has no term in the objective, so a stacked
layout is free, and stacked tables have short connectors, which is what the objective's
surviving term rewards. The search was not tolerating the bug, it was mining it. A repair that
fails silently does not soften a hard constraint; it pays for violating it.

The fix is three things and the third is the one that mattered: sweep the whole layout until a
sweep moves nothing rather than walking it once, push against the resultant of all overlaps
rather than snapping out of each neighbour in turn (which let the last neighbour win and set
tables oscillating between two frozen boxes), and seat anything still stuck by walking outward
on a coarse grid to the nearest free position. NODE_GAP is 12 units because 12 is the tightest
clearance in the maintainer's own accepted layout, whose next three are 14, 14 and 17.

The lesson generalises past this example: when a constraint lives in a repair, write the check
that the RETURNED answers satisfy it, and run it. Nothing in ut or cliut had that check, which
is why a visible defect survived every gate, four reviews and a paper.

## What is open, in the order to take it

1. ~~**Unit suite and CI.**~~ Done: `make check` is ut + cliut, the sanitizers run both suites,
   and CI covers two platforms, six compiler/flag pairs and the byte-for-byte reproducibility
   job. Writing the exactness checks found the one path that could overspend the budget:
   climb's restart could score twice in an iteration and spend budget+1. Fixed, with a probed
   witness pinned as a regression check; tests/tests.c owns the witness and the instructions
   for re-deriving it when a trajectory change moves it. Later review rounds removed every
   libm call from the trajectories except fabs and sqrt (cjitter.h states the discipline),
   and a four-critic pass fixed an uninitialized best on degenerate fitnesses, the missing
   box refusals, a fixed GA mutation scale that had manufactured the first "no better"
   verdict, and the tuning API's zero-means-default trap.
2. ~~**The real pair on the example.**~~ Done. `example/erd` now runs the real schema's latest
   migration: 34 frozen tables at the human's coordinates, 10 added tables placed by the search,
   the human's own placement scored as a reference. The data is committed anonymized under
   `example/erd/data/` (see PROVENANCE.md there; the mapping back to real names is deliberately
   not stored anywhere). Parsing detail that cost an hour: figure positions are attribute-order
   `type="real" key="left"`, a FK's child table is its `owner` link, and an old document can
   hold several diagrams. `currentDiagram` can point at a stale one, so trust the diagram
   whose FK links resolve.

   **The full benchmark still exists in `~/kul`:** 17 revisions of `doc/DataModel/ERD.mwb`,
   16 consecutive before/after pairs, each a real migration with a human-accepted layout on
   both sides. One pair is now the example; the measurement over all 16 (freeze, place, score
   against the human and the centroid) is what a **`.mwb` reader in C** or the committed
   extraction pipeline run over each pair makes possible. 16 pairs is enough to say whether
   the search beats the heuristic in general, which is the only claim worth making. Caveat
   learned from the one shipped pair: under this objective the searches beat the human, which
   means the objective is missing what the human optimizes. Treat the human score as a
   reference, not a target to beat.
3. **The block, and what it re-opens.** `cjitter_tuning.block` is how many variables one
   proposal moves, in blocks that tile the vector and cycle. The default is n, the whole
   vector, so no shipped trajectory moved and the pinned witness still holds; the tests cover
   the refusal, the whole-vector equivalence at block >= n, the cursor advancing, and the short
   last block when n is not a multiple. It exists because the founding mechanism was missing
   from the library named after it: the 2001 system nudged one label at a time, and all four
   methods here moved the whole vector at once.

   Measured with `cjitter_compare_tuned` at the shipped budgets, block 2 against block n:

       labels    climb 12.8 -> 0, anneal 128 -> 0, ga 1.38 -> 0. All three reach exactly 0
                 on all 7 seeds, range 0: the clean layout, which nothing reached before.
       erd routed  climb 46134 -> 32293 (range 18662 -> 178), anneal 50692 -> 32579,
                 ga 40119 -> 39571 (range 4555 -> 23872).
       erd straight  climb 116506 -> 69142, anneal 104588 -> 70633, ga 72902 -> 80240.

   Two things to read there. The single-point searches gain most, and their spread over seeds
   collapses, which matters more than the median: climb's routed range of 178 says it finds
   the same answer from every seed. And the ga does not gain, and on the erd loses, because
   only its mutation is blocked while crossover still blends every coordinate; that is a weak
   variant, not a bug, and the honest thing is to report it rather than exempt the ga.

   How the examples took it: as an option, not a switch. `labels` gains a fourth argument and
   `erd` a `--block N`, both defaulting to the tuning default, so every number pinned in
   tests/cli.sh and quoted in README.md is the number it always was and no re-measurement was
   needed. The film is the exception and the reason the block exists: `erd_movie` sets block 2
   outright, because at the default a proposal displaces all ten tables and the reader watches
   the migration teleport 47 times instead of tables finding their neighbours. Rebuilt it is
   124 improvements and 32293 against 53061.

   `auto` stays climb, and the reason is now written down where it was missing. The migration
   benchmark in ~/articles/cjitter ranks climb the budget-efficient method (it separates from
   the control on seven of eight instances; the ga does not separate at all), and that is what
   the default follows. README.md's two example panels rank ga first at the default block,
   which is the disagreement the old header sentence hid by saying "the shipped benchmarks"
   without naming one. cjitter.h now names both.

   The paper carries the rest as Section 4.6, exploratory: the whole pre-registered sweep
   re-run on both arms, the default arm reproducing all 640 frozen per-seed values exactly,
   and the ga's refutation verdict moving from p = 0.109 to 0.0156 by flipping exactly the
   pair the leave-one-out appendix had already named as its hinge. Only five of the eight
   pairs can differ at all (three add one table, where block 2 IS the whole vector), and at
   n = 5 the exact two-sided floor is 0.0625, so nothing there reaches 0.05. Do not quote the
   example's 30% as though the benchmark showed it: on the benchmark the margins are 0.01 to
   8.5 percent. Note also that the benchmark's pair 16 is NOT the shipped example, though both
   are k = 10 on the same schema: the benchmark freezes the previous revision's coordinates
   (displacement 1754) and the example the current ones, so their scores differ by an order of
   magnitude and are not comparable.

4. **The router**, until it reproduces the human layout's 0 crossings and 0 penetration. The
   current one (two bends, border anchors at per-edge attachment slots, sequential and aware
   of every connector already placed) measures 27 and 323 on that layout, and the run prints
   the number so nobody mistakes the floor for the human's. What remains: more bends; a grid
   router.
   Every score comparison against the human sharpens exactly as fast as this number falls.
5. **An anchor term**, which turns the incremental case into the general one. Let the frozen tables
   move, penalised by squared displacement from their old positions, and one weight then
   interpolates between "nothing moves" and "full redraw". It also makes the search easier, by
   giving a rugged objective a basin around a known-good answer.
6. **Benchmark against graphviz** before believing anything about full redraws. `neato` and `dot`
   are thirty years of graph drawing and are the control for that case, exactly as uniform sampling
   is the control here.

7. **`example/noise/`, and the join to `~/articles/bpnn/resolution.tex`.** This is QUESTIONS.md
   item 1 next door, and `verify` was the prerequisite: a sweep for the noise level at which
   search stops beating random, run through an estimator biased by that same noise, measures its
   own artifact. Parameterise the family by the single dimensionless ratio **r = sigma_W /
   sigma_B**, not by a raw sigma, because that is exactly what resolution.tex measured across 92
   cells of NAS-Bench-101 and 201, together with the resolvability floor 2.77*sigma_W at one
   training run. Then the sweep answers directly whether ANY cheap stochastic search could have
   separated architectures at the noise those benchmarks actually carry, and the two papers
   compose without a conversion step. Two of the author's own lines meeting in one figure, no GPU.
   Note also `~/smbpann/validation/paper2/scratch_jsig_*.out`: an injected-noise sweep already
   half-run on a fixed landscape, without the control arm at every noise level and without an
   exact test, which is precisely the half this example would add.

8. **Absorb `pairstat.c`.** `cjitter_compare` has one statistic, a 15-line sign test. The exact
   engine already exists in this author's own `~/smbpann/validation/pairstat.c`: 606 lines of
   C99, no dependencies, exact Wilcoxon signed-rank by dynamic programming, exact sign test,
   exact McNemar, Hodges-Lehmann with a distribution-free rank CI, Holm over a declared family,
   TOST, an MDE, and a `--selftest` that passes 16 of 16 against hand-computable cases. The
   layout paper reports Wilcoxon, Holm, Hodges-Lehmann and MDE computed OUTSIDE the library, in
   Python; the library cannot produce the statistics its own paper prints. The MDE matters most:
   it is what makes "not shown" bound rather than assert, which is what this file already claims
   the verdict does.

9. **`B*` as an entry point.** The separation budget, the smallest budget at which a method
   sweeps the control on the seed panel, is already defined, already used in the paper, and lives
   in `articles/cjitter/data/analysis.py`. It is the quantity an engineer actually has a question
   about. Making it a library call is what turns QUESTIONS.md item 2, a field guide by separation
   budget across problem families, from a project into a loop.

10. **A caller-supplied RNG stream.** Concrete blocker, not theory. `~/smbpann` accepts a change
    only when it reproduces archived per-seed output bit for bit, and its `validation/paper2/ga.h`
    is a compile-time template rather than function pointers *because* a shared search owns the
    order in which random numbers are drawn. cjitter seeds its own `Rng` from `budget.seed`, so
    it cannot be dropped into any smbpann probe without breaking that oracle. The probes are
    where it would earn its keep: `emerge_relax.c:183` is literally a one-dimensional real search
    done by a nine-point fixed grid, and `PROTOCOL.md` records that one unswept constant,
    `g_padd = 0.006`, "was solely responsible for the headline in both probes."

11. **An objective-indifference check.** The failure that recurs across every project in this
    line, and the one nothing catches. `~/articles/smbpann2/tiling.tex` is retracted because its
    energy term "was exactly indifferent to placement": an operator that copied nothing scored
    26%, one that destroyed the target spacing scored 59%. This repository produced two of the
    same class in one week, both recorded above: "length only ever breaks ties" was false at
    every layout a search returns, and the repair that owned non-overlap did not enforce it. The
    check is mechanical and cheap: perturb or destroy the property the objective claims to
    reward, and assert the score responds. All three would have failed it on day one.

## What not to do

Do not reopen the architecture-emergence line in bpnn's predecessor on the strength of this
library. That direction closed because its claims were prior art, its numbers were wrong and two
findings were selection artifacts; a better optimiser fixes none of that. The question this library
*can* answer cheaply is the methodological one: at what objective noise level does any search beat
matched-budget random? Synthetic objective, known optimum, injected noise, sweep it. If nothing
survives above the noise level that architecture search operates at, that settles the affordability
question without a GPU.
