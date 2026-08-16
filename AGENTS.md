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
    make ut         # unit suite, 65 checks: the library's contract, called as functions
    make cliut      # black-box, 48 checks: the built examples through a shell
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
    example/labels.c    rectangles in a container, minimum overlap
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
and crossings at 100, connector length at 1, so length only ever breaks ties.

Score the medium the reader sees. The ERD edges are routed orthogonally (L and Z shapes, the
middle segment sliding across the channel) before anything is measured, because that is what
the tool draws, and the router is calibrated against the one certain fact: the human's layout
achieved 0 crossings and 0 penetration on screen. The run prints the router's shortfall (27
and 323 at present, with border anchoring, per-edge attachment slots, and sequential routing
aware of every connector already placed), and no score comparison against the human means
more than that line allows. Closing the remainder, more bends and a grid router, is the open
problem the router owns.

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
3. **The router**, until it reproduces the human layout's 0 crossings and 0 penetration. The
   current one (two bends, border anchors at per-edge attachment slots, sequential and aware
   of every connector already placed) measures 27 and 323 on that layout, and the run prints
   the number so nobody mistakes the floor for the human's. What remains: more bends; a grid
   router.
   Every score comparison against the human sharpens exactly as fast as this number falls.
4. **An anchor term**, which turns the incremental case into the general one. Let the frozen tables
   move, penalised by squared displacement from their old positions, and one weight then
   interpolates between "nothing moves" and "full redraw". It also makes the search easier, by
   giving a rugged objective a basin around a known-good answer.
5. **Benchmark against graphviz** before believing anything about full redraws. `neato` and `dot`
   are thirty years of graph drawing and are the control for that case, exactly as uniform sampling
   is the control here.

## What not to do

Do not reopen the architecture-emergence line in bpnn's predecessor on the strength of this
library. That direction closed because its claims were prior art, its numbers were wrong and two
findings were selection artifacts; a better optimiser fixes none of that. The question this library
*can* answer cheaply is the methodological one: at what objective noise level does any search beat
matched-budget random? Synthetic objective, known optimum, injected noise, sweep it. If nothing
survives above the noise level that architecture search operates at, that settles the affordability
question without a GPU.
