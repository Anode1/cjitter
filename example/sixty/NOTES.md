# The sixty-draws question

The research attempt behind QUESTIONS.md item 8 in the articles repository. The rule of 59
(n uniform draws reach the top 5% of the sampling distribution with probability 1 - 0.95^n)
is tutorial folklore around Bergstra and Bengio 2012, not a result the paper states, and
the equation it quietly makes -- top 5% of sampling = good enough -- is a claim about
landscapes that our search found no published test of.

## Phases

1. **The pilot** (`sixty.c`, here): two landscapes, the sphere and a rectangle-overlap
   placement. Per landscape: what best-of-n random delivers over a 201-seed panel, the
   top-5% hit share against the arithmetic's prediction (that column validates the
   instrument, it cannot surprise), and the budget at which climb, anneal and the ga match
   random's n = 59 median, the method-wise counterpart of the constant.
2. **The smbpann landscape.** The engine one directory over (a `~/bpnn` checkout) trains
   its small network in about 6 ms, which makes its constants an affordable objective:
   variables are the training constants (rate, momentum, hidden width read as a rounded
   real), fitness is held-out error after a short fixed run, seeded so a point re-scores
   identically. That turns the pilot's question into the one the folklore is actually
   used for: hyperparameter search. Wire it behind a `BPNN=../..` make variable and keep
   the coupling in this directory.
3. **Confirmatory runs** happen only under a signed pre-registration naming the landscape
   family, the panel size, the primary estimand (delivered quality at n = 59 against the
   quantile promise; the matching budgets per method) and the tests, per the house rules.

## Standing rules

The pilot's output is unpinned on purpose: pinning it would freeze exploratory numbers
into the suite. `make sixty` builds it; it is compiled by `make pedantic` like everything
else. Nothing in this directory feeds the layout paper.
