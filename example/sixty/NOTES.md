# The sixty-draws question

The research attempt behind QUESTIONS.md item 8 in the articles repository. The rule of 59
(n uniform draws reach the top 5% of the sampling distribution with probability 1 - 0.95^n)
is tutorial folklore. It is not in Bergstra and Bengio 2012, whose closed form is
1 - 0.99^T against a 1% target; the 60-draws wording traces to Alice Zheng, 2015. The
equation the rule quietly makes, top 5% of sampling equals good enough, is a claim about
landscapes, and our search found no published test of it.

The pre-registration is `~/articles/sixty/PREREGISTRATION.md`. Nothing confirmatory runs
until it is signed.

## Phases

1. **The pilot** (`sixty.c`, here): two landscapes, the sphere and a rectangle-overlap
   placement. Per landscape: what best-of-n random delivers over a 201-seed panel, the
   top-5% hit share against the arithmetic's prediction (that column validates the
   instrument, it cannot surprise), and the budget at which climb, anneal and the ga match
   random's n = 59 median, the method-wise counterpart of the constant.
2. **The tabular family.** NAS-Bench-201 (15,625 architectures, exhaustive, three seeds
   each) and NAS-Bench-101 (423,624), both already extracted to flat text under
   `~/smbpann/validation` with provenance beside them. Exhaustive enumeration makes the
   sampling distribution the table itself, so the top-5% threshold and the good-region
   volume are exact counts and the pilot's 50,000-draw reference is not needed here. The
   three seeds are what make `verify` and `inflation` measurable against a held-out
   replicate. Read it through a flat adapter; no Python enters the study.
3. **The noisy landscape.** The engine one directory over (a `~/bpnn` checkout) trains its
   small network in about 6 ms, which makes its constants an affordable objective:
   variables are the training constants (rate, momentum, hidden width read as a rounded
   real), fitness is held-out error after a short fixed run, a fresh seed per call.
   Wire it behind a `BPNN=../..` make variable and keep the coupling in this directory.
4. **Confirmatory runs** happen only under the signed pre-registration, per the house rules.

## Standing rules

The pilot's output is unpinned on purpose: pinning it would freeze exploratory numbers
into the suite. `make sixty` builds it; it is compiled by `make pedantic` like everything
else.
