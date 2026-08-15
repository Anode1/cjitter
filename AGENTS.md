# AGENTS.md -- how to develop cjitter (for humans and AI agents)

`cjitter` is four stochastic searches over a box of real variables, in **C99**, behind one fitness
interface, with uniform random sampling as a control that runs at the same budget. It is the third
of three: [linearr](https://github.com/Anode1/linearr) fits a line and says when a line is the
wrong shape; [bpnn](https://github.com/Anode1/bpnn) fits a network and says when not to trust it;
this searches and says when the search did no better than luck.

## Why it exists

Two real problems, twenty-five years apart. Label placement in a bounded rectangle (2001,
deployed), and placing tables added by a migration onto an existing MySQL Workbench diagram, which
Workbench does badly and which costs about an hour by hand on a 44-table schema.

The name is jitter, the regulariser from the author's 1997 thesis and the mechanism that made the
2001 layout settle. It names what all four methods share, which is why it does not name one of
them: an earlier working title, `gaop`, would have baked in a misattribution -- what solved the
2001 problem was called a genetic algorithm and was in fact annealing with restarts.

## The contract (read first)

- **`c/cjitter.h`** -- the interface, and the specification. A problem is `n`, a box, a fitness
  function and an optional repair callback. A budget is evaluations, a seed and a first move size.
- **`README.md`** -- what the library promises and the measured output of both examples.
- Hard constraints belong in `repair`, never in the fitness. A constraint enforced by construction
  cannot trade itself off against the objective, and no infeasible point can be returned as best.

## Build and test

    make            # both examples
    make check      # ut + cliut -- the commit gate
    make ut         # unit suite, 47 checks: the library's contract, called as functions
    make cliut      # black-box, 29 checks: the built examples through a shell
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
four ways and compares both examples' output byte for byte -- `-march=native` is in that matrix
because it is the flag that exposes FMA contraction. macOS never sees gcc and the sanitizers
only ever run under the default compiler; do not read more coverage into it than that.

`-ffp-contract=off` is in the flags. gcc contracts `a*b+c` into one FMA by default even under
`-std=c99`, and x86-64 baseline hides it only because SSE2 has no FMA instruction; without the flag
a run stops reproducing the moment anyone builds with `-march=native` or on arm64.

## Module map

    c/cjitter.h     the interface
    c/cjitter.c     the four searches and cjitter_compare
    c/rng           deterministic xorshift, carried from bpnn. NOTE: 32-bit, period 2^32-1.
                    Fine for a run; NOT fine for a study drawing more than ~4e9 numbers. A
                    measurement that exhausted this period understated its own standard error
                    by 2.5x.
    example/labels.c    rectangles in a container, minimum overlap
    example/erd/erd.c   tables added by a migration onto a frozen diagram
    tests/tests.c       unit suite: refusals, the exact budget, determinism, box and repair
    tests/cli.sh        black-box: exit codes, refusal messages, runs byte-identical over reruns

## What is measured, and what it means

`cjitter_compare` calls a method better only when its median over seeds beats the **control's
luckiest seed**. An earlier rule compared the margin against the control's range and called a
method that hit the global optimum on every seed "inside noise"; random search is erratic, so its
range is wide, and that rule punished a method for the control's variance.

Two results so far, both from the shipped examples:

- Labels at 36% coverage: climb 36.96, anneal 114.6, **ga 192.2 against random's 187.0 -- no
  better than uniform sampling at equal cost.**
- ERD: all three beat the control; the centroid heuristic scores 55887 against the search's 10174,
  because a table at its neighbours' centroid lands on the edges between them.

## Writing an objective a search can follow

Score by penetration, not by a count. An edge passing through a table is scored by the length of
the segment inside the rectangle; a crossing count is flat under small moves, so the search has
nothing to follow and walks at random on the plateau. Where a count is unavoidable, add a
continuous nearness term beside it.

Tier the weights by orders of magnitude rather than tuning them. In the ERD example: penetration
and crossings at 100, edge length at 1, so length only ever breaks ties.

## What is open, in the order to take it

1. ~~**Unit suite and CI.**~~ Done: `make check` is ut + cliut, the sanitizers run both suites,
   and CI covers two platforms, six compiler/flag pairs and the byte-for-byte reproducibility
   job. Writing the exactness checks found the one path that could overspend the budget --
   climb's restart could score twice in an iteration and spend budget+1 -- fixed with the
   witness (seed 157 at 5000 evaluations) pinned as a regression check. Both examples' shipped
   outputs were compared byte for byte across the fix: unchanged.
2. **The `.mwb` reader.** `example/erd` builds its graph in code. A real `.mwb` is a zip around
   `document.mwb.xml`; a grep for `key="left" type="real"` finds nothing, so the first task is
   mapping where `db.mysql.Table` objects and the diagram figure positions live and how they link.
   This is a parsing job; the layout is done.

   **The benchmark already exists and nobody had to build it.** `~/kul` has **17 revisions of
   `doc/DataModel/ERD.mwb` in git history** -- 16 consecutive before/after pairs, each one a real
   migration with a layout a human accepted on both sides. Commit subjects name the change
   ("added new asset set metadata table"). So for every pair you have: the previous diagram with
   its coordinates, the new schema, and the placement a person actually chose. That is ground
   truth for an incremental layouter, in quantity, for free.

       git -C ~/kul log --oneline -- doc/DataModel/ERD.mwb
       git -C ~/kul show REV:doc/DataModel/ERD.mwb > prev.mwb

   The measurement that follows: freeze the old tables, place the ones the migration added, and
   score the result against the human's placement -- and against the centroid heuristic, which
   the example already shows losing badly. 16 pairs is enough to say whether the search beats the
   heuristic in general rather than on one diagram, which is the only claim worth making.

   `ERD.png` and `ERD_prev.png` are the current pair rendered, useful for judging by eye.
   Credentials for the live schema are in `~/kul/java/conf/system.properties` if a reverse-
   engineered comparison is wanted, but the git history alone is a richer source and needs no
   database at all.
3. **An anchor term**, which turns the incremental case into the general one. Let the frozen tables
   move, penalised by squared displacement from their old positions, and one weight then
   interpolates between "nothing moves" and "full redraw". It also makes the search easier, by
   giving a rugged objective a basin around a known-good answer.
4. **Benchmark against graphviz** before believing anything about full redraws. `neato` and `dot`
   are thirty years of graph drawing and are the control for that case, exactly as uniform sampling
   is the control here.

## What not to do

Do not reopen the architecture-emergence line in bpnn's predecessor on the strength of this
library. That direction closed because its claims were prior art, its numbers were wrong and two
findings were selection artifacts; a better optimiser fixes none of that. The question this library
*can* answer cheaply is the methodological one: at what objective noise level does any search beat
matched-budget random? Synthetic objective, known optimum, injected noise, sweep it. If nothing
survives above the noise level that architecture search operates at, that settles the affordability
question honestly and without a GPU.
