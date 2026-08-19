# The tuning: every constant a field, every field literal

The methods' internal constants (patience, cooling, mutation, crossover) come from
`cjitter_tuning_default`; change the fields you mean to change and pass the struct to
`cjitter_run_tuned` or `cjitter_compare_tuned`. Every field is read literally, so
`ga_mutate = 0` is a real mutation ablation and `ga_crossover = 0` a real crossover
ablation, never a default in disguise. A tuning is part of a result's identity: two runs
compare only if they share one. `c/cjitter.h` states each field's range beside it, and a
field outside its range is refused before anything is evaluated.

Since 0.11.0 the tuning also owns `jitter` (the first move size, as a fraction of each
variable's range) and `pop` (the ga's population): they are part of how a method spends,
so they live with the other constants, and a budget is just evaluations and a seed.

## The block

`block` is how many variables a single proposal moves; the default is all of them. When the
objective is a sum over objects that interact weakly, a proposal that moves everything at
once improves one object and spoils another, and gets rejected for the spoiling. Setting
`block` to the width of one object, 2 for a point in the plane, steps the search through the
problem an object at a time, which is the mechanism this library is named after and can be
worth an order of magnitude in budget. It is not free: where the good moves adjust several
objects together, a narrow block cannot express them. `cjitter.h` says the rest, and
`cjitter_compare_tuned` measures it for your problem the way it measures everything else.

Blocks tile the vector in order and cycle, one per proposal, so climb, anneal and the ga's
mutation all step through the problem a block at a time. Only the ga's mutation is blocked;
its crossover still blends every coordinate, so a narrow block leaves it a weak-mutation GA,
which is a fact to know before reading a blocked comparison. Measured on the shipped
examples: on the label problem `./labels 90 20000 7 2` puts climb, annealing and the GA all
at exactly 0 on all seven seeds, the clean layout none of them reaches at any budget with
whole-vector proposals; the diagram example's numbers are in
[example/erd/README.md](../example/erd/README.md).

One historical note, because it is the library's name. The 2001 label placer this line
descends from moved one label at a time, per-label random unit steps kept when the summed
overlap fell. All four methods here originally moved the whole vector at once; the block put
the founding mechanism back, as a parameter anyone can measure.

## verify: an honest number when the objective is noisy

If your fitness returns a slightly different number each time it is called, a held-out error
from a training run, a simulation, anything sampled, then the smallest value a search
observes is not what the search found. It is the luckiest draw it took, and the search is
the thing that went looking for lucky draws.

That is not a small correction and it is not fair across methods, because how much luck a
method accumulates depends on how much it resamples one place, which is exactly what
distinguishes the methods being compared. Set `verify` in the tuning and the result carries
`verified`, the mean of that many fresh evaluations of the point actually returned, along
with `inflation`, the gap between the two. `cjitter_compare` then judges on `verified` and
prints the inflation beside each method.

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
