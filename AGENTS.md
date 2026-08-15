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
them: an earlier working title, `gaop`, would have baked in a misattribution, because what
solved the 2001 problem was called a genetic algorithm and was in fact annealing with restarts.

## The contract (read first)

- **`c/cjitter.h`** -- the interface, and the specification. A problem is `n`, a box, a fitness
  function and an optional repair callback. A budget is evaluations, a seed and a first move
  size. A tuning is optional and names the methods' internal constants; a zeroed field is the
  shipped default, so results only compare across runs that share one.
- **`README.md`** -- what the library promises and the measured output of both examples.
- Hard constraints belong in `repair`, never in the fitness. A constraint enforced by construction
  cannot trade itself off against the objective, and no infeasible point can be returned as best.

## Build and test

    make            # both examples
    make check      # ut + cliut -- the commit gate
    make ut         # unit suite, 50 checks: the library's contract, called as functions
    make cliut      # black-box, 37 checks: the built examples through a shell
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
    c/rng           deterministic xorshift, carried from bpnn. NOTE: 32-bit, period 2^32-1.
                    Fine for a run; NOT fine for a study drawing more than ~4e9 numbers. A
                    measurement that exhausted this period understated its own standard error
                    by 2.5x.
    example/labels.c    rectangles in a container, minimum overlap
    example/erd/erd.c   tables added by a migration onto a frozen diagram; --svg draws it
    example/erd/data/   the real schema, anonymized: ERD.mwb, both revisions as PNG, the
                        geometry as JSON, and PROVENANCE.md for what the anonymization changed
    tests/tests.c       unit suite: refusals, the exact budget, determinism, box and repair
    tests/cli.sh        black-box: exit codes, refusal messages, runs byte-identical over reruns

## What is measured, and what it means

`cjitter_compare` calls a method better only when its median over seeds beats the **control's
luckiest seed**. An earlier rule compared the margin against the control's range and called a
method that hit the global optimum on every seed "inside noise"; random search is erratic, so its
range is wide, and that rule punished a method for the control's variance.

Two results so far, both from the shipped examples:

- Labels at 36% coverage: climb 15.4, anneal 121.7, **ga 195.6 against random's 187.0 -- no
  better than uniform sampling at equal cost.**
- ERD, on the real anonymized schema (34 frozen tables, 10 added by the last migration): all
  three beat the control, the centroid heuristic scores 251673, and the searches also beat the
  human's accepted layout (231877, against climb's 109221 median). That result means the human
  optimizes what the objective cannot see, semantic grouping, aligned rows, room to grow. The
  tool is better only at what the objective names.

## The objective

Score by penetration, not by a count. An edge passing through a table is scored by the length of
the segment inside the rectangle; a crossing count is flat under small moves, so the search has
nothing to follow and walks at random on the plateau. Where a count is unavoidable, add a
continuous nearness term beside it.

Tier the weights by orders of magnitude rather than tuning them. In the ERD example: penetration
and crossings at 100, edge length at 1, so length only ever breaks ties.

## What is open, in the order to take it

1. ~~**Unit suite and CI.**~~ Done: `make check` is ut + cliut, the sanitizers run both suites,
   and CI covers two platforms, six compiler/flag pairs and the byte-for-byte reproducibility
   job. Writing the exactness checks found the one path that could overspend the budget:
   climb's restart could score twice in an iteration and spend budget+1. Fixed, with a probed
   witness pinned as a regression check (currently climb seed 200 at 5000 evaluations,
   restarts pinned at 6; re-derive by probing an unguarded build if the trajectory ever moves).
   A later review round removed every libm call from the trajectories except sqrt (Box-Muller
   became a sum of uniforms, anneal's exp and pow became arithmetic), because libms disagree in
   the last ulp and one ulp under an argmax is a different answer.
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
