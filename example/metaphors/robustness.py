"""The revision's robustness annexes, every number the paper's Section 4 and 5 cite.

Run from the working directory holding fb.npz, fb_names.json, uniform_control.csv,
dt_fb.csv and Processed_data.zip. Prints, in order:
  A. top-budget verdict tables at 2.5% per direction (both controls) and the flip
     counts against the pre-registered 5% rule;
  B. the 282-implementation family (dt_fb.csv's own inventory) at the top budget;
  C. cluster-level sensitivity, the sign test on per-(function, instance) medians;
  D. per-function shown-worse counts, 10D top budget;
  E. the same audit on the release's own anytime AOCC values;
  F. paired Wilcoxon (normal approximation) as a magnitude-aware check;
  G. effect-size facts: median of medians, exact zeros, implementations at or
     behind the control, extremes;
  H. the outcome-neutral baselines' win-rate range.
"""
import csv
import io
import json
import zipfile
from math import erf, lgamma, log, exp, sqrt

import numpy as np

def sign_p(w, n):
    if n < 1:
        return 1.0
    ln2 = n * log(2.0)
    return min(sum(exp(lgamma(n + 1) - lgamma(k + 1) - lgamma(n - k + 1) - ln2)
                   for k in range(max(w, 0), n + 1)), 1.0)

def holm(p):
    o = np.argsort(p, kind="stable")
    adj = np.empty_like(p)
    prev = 0.0
    k = len(p)
    for r, i in enumerate(o):
        a = min((k - r) * p[i], 1.0)
        prev = max(prev, a)
        adj[i] = prev
    return adj

D = np.load("fb.npz")
names = json.load(open("fb_names.json"))
libs, algs = names["libs"], [tuple(a) for a in names["algs"]]
BF, DIMS = names["budget_factors"], names["dims"]
ctl = np.full((7, 4, 25, 11, 6), np.nan)
with open("uniform_control.csv") as f:
    for r in csv.DictReader(f):
        ctl[BF.index(int(r["budget_factor"])), DIMS.index(int(r["dim"])),
            int(r["fid"]), int(r["iid"]), int(r["run"])] = float(r["fx"])
alg, bf, dim, fid, iid, run, fx = (D[k] for k in ("alg", "bf", "dim", "fid", "iid", "run", "fx"))
uref = ctl[bf, dim, fid, iid, run]
met = [i for i, (li, a) in enumerate(algs) if libs[li] != "Baselines"]
rs_id = next(i for i, (li, a) in enumerate(algs) if libs[li] == "Baselines" and a == "RandomSearch")

rtab = np.full((4, 25, 11, 6), np.nan)
m = (alg == rs_id) & (bf == 6)
rtab[dim[m], fid[m], iid[m], run[m]] = fx[m]
rref_top = rtab[dim, fid, iid, run]

def counts(ref, sel, family):
    """per implementation in FAMILY: wins, losses over pairs SEL against REF"""
    ok = sel & np.isfinite(ref)
    w = fx < ref
    l = fx > ref
    out = {}
    for i in family:
        mm = ok & (alg == i)
        out[i] = (int(w[mm].sum()), int(l[mm].sum()))
    return out

def table(cnt, family, a):
    pb = np.array([sign_p(cnt[i][0], sum(cnt[i])) for i in family])
    pw = np.array([sign_p(cnt[i][1], sum(cnt[i])) for i in family])
    ab, aw = holm(pb), holm(pw)
    b = int((ab <= a).sum())
    w = int((aw <= a).sum())
    return b, len(family) - b - w, w, ab, aw

print("A. top budget, alpha 2.5% per direction (directional FWER 5% per cell):")
for name, ref in (("uniform", uref), ("their RandomSearch", rref_top)):
    for d in range(4):
        cnt = counts(ref, (bf == 6) & (dim == d), met)
        b, ns, w, _, _ = table(cnt, met, 0.025)
        print(f"  {name:18s} D={DIMS[d]:>2}: better {b} not-shown {ns} worse {w}")

print("\nA2. flips 5% -> 2.5% over all confirmatory cells:")
for name, tabref in (("uniform", "verdicts_uniform.tsv"), ("rs", "verdicts_rs.tsv")):
    flips = 0
    try:
        for r in csv.DictReader(open(tabref), delimiter="\t"):
            if r["lib"] == "Baselines" or int(r["budget_factor"]) == 10:
                continue
            hb, hw = float(r["holm_better"]), float(r["holm_worse"])
            v5 = "b" if hb <= 0.05 else ("w" if hw <= 0.05 else "n")
            v25 = "b" if hb <= 0.025 else ("w" if hw <= 0.025 else "n")
            flips += v5 != v25
        print(f"  vs {name}: {flips}")
    except FileNotFoundError:
        print(f"  vs {name}: verdict tsv not present, skipped")

print("\nB. the 282 named by dt_fb.csv, uniform control, top budget, 5%:")
official = set()
for r in csv.DictReader(open("dt_fb.csv")):
    if r["lib"] != "Baselines":
        official.add((r["lib"], r["algname"]))
fam282 = [i for i in met if (libs[algs[i][0]], algs[i][1]) in official]
print(f"  family size {len(fam282)}; extras: "
      f"{sorted((libs[algs[i][0]], algs[i][1]) for i in met if i not in fam282)}")
for d in range(4):
    cnt = counts(uref, (bf == 6) & (dim == d), fam282)
    b, ns, w, _, _ = table(cnt, fam282, 0.05)
    print(f"  D={DIMS[d]:>2}: better {b} not-shown {ns} worse {w} (not shown better {len(fam282)-b})")

print("\nC. cluster-level (median paired difference per (function, instance)), uniform, top budget, 5%:")
diff = fx - uref
for d in range(4):
    m = (bf == 6) & (dim == d) & np.isfinite(uref)
    a_, key, df = alg[m], fid[m].astype(int) * 100 + iid[m].astype(int), diff[m]
    pb, pw = [], []
    for i in met:
        mm = a_ == i
        dfi, ki = df[mm], key[mm]
        w = l = 0
        for k in np.unique(ki):
            md = np.median(dfi[ki == k])
            if md < 0:
                w += 1
            elif md > 0:
                l += 1
        pb.append(sign_p(w, w + l))
        pw.append(sign_p(l, w + l))
    ab, aw = holm(np.array(pb)), holm(np.array(pw))
    print(f"  D={DIMS[d]:>2}: better {(ab<=0.05).sum()} not-shown "
          f"{((ab>0.05)&(aw>0.05)).sum()} worse {(aw<=0.05).sum()}")

print("\nD. per-function shown-worse counts, D=10, top budget, uniform, 5%:")
m = (bf == 6) & (dim == 2) & np.isfinite(uref)
a_, f_, df = alg[m], fid[m], diff[m]
per = []
for F in range(1, 25):
    mf = f_ == F
    pb, pw = [], []
    for i in met:
        mm = mf & (a_ == i)
        w = int((df[mm] < 0).sum())
        l = int((df[mm] > 0).sum())
        pb.append(sign_p(w, w + l))
        pw.append(sign_p(l, w + l))
    ab, aw = holm(np.array(pb)), holm(np.array(pw))
    per.append((F, int((aw <= 0.05).sum())))
print(" ", per)
print(f"  range {min(w for _, w in per)} to {max(w for _, w in per)}, "
      f"max at f{max(per, key=lambda t: t[1])[0]}")

print("\nE. AOCC (the release's anytime measure) vs their RandomSearch, 5%:")
zf = zipfile.ZipFile("Processed_data.zip")
adata = {}
for name in sorted(zf.namelist()):
    if not name.startswith("AUC_"):
        continue
    lib = name[4:-4]
    with zf.open(name) as f:
        for row in csv.DictReader(io.TextIOWrapper(f)):
            arr = adata.setdefault((lib, row["algname"]), np.full((4, 25, 11, 6), np.nan))
            arr[DIMS.index(int(row["dim"])), int(row["fid"]),
                int(row["iid"]), int(row["run"])] = float(row["auc"])
ars = adata[("Baselines", "RandomSearch")]
amet = [k for k in adata if k[0] != "Baselines"]
for d in range(4):
    pb, pw = [], []
    for k in amet:
        a_, b_ = adata[k][d], ars[d]
        mm = np.isfinite(a_) & np.isfinite(b_)
        w = int((a_[mm] > b_[mm]).sum())
        l = int((a_[mm] < b_[mm]).sum())
        pb.append(sign_p(w, w + l))
        pw.append(sign_p(l, w + l))
    ab, aw = holm(np.array(pb)), holm(np.array(pw))
    print(f"  D={DIMS[d]:>2}: n={len(amet)} better {(ab<=0.05).sum()} not-shown "
          f"{((ab>0.05)&(aw>0.05)).sum()} worse {(aw<=0.05).sum()}")

print("\nF. paired Wilcoxon signed-rank (normal approximation), uniform, top budget, 5%:")
for d in range(4):
    m = (bf == 6) & (dim == d) & np.isfinite(uref)
    a_, df = alg[m], diff[m]
    pb, pw = [], []
    for i in met:
        x = df[a_ == i]
        x = x[x != 0]
        n = len(x)
        if n == 0:
            pb.append(1.0); pw.append(1.0); continue
        rk = np.argsort(np.argsort(np.abs(x))) + 1.0
        wplus = rk[x > 0].sum()
        mu, sd = n * (n + 1) / 4.0, sqrt(n * (n + 1) * (2 * n + 1) / 24.0)
        z = (wplus - mu) / sd
        # a small W+ means the differences lean negative, the implementation winning,
        # so "better" is the lower tail P(W+ <= observed)
        pb.append(0.5 * (1 + erf(z / sqrt(2))))
        pw.append(0.5 * (1 + erf(-z / sqrt(2))))
    ab, aw = holm(np.array(pb)), holm(np.array(pw))
    print(f"  D={DIMS[d]:>2}: better {(ab<=0.05).sum()} not-shown "
          f"{((ab>0.05)&(aw>0.05)).sum()} worse {(aw<=0.05).sum()}")

print("\nG. effect sizes at the top budget, uniform: per-implementation median error ratio")
for d in range(4):
    m = (bf == 6) & (dim == d) & np.isfinite(uref)
    a_, fx_, u_ = alg[m], fx[m], uref[m]
    meds = []
    for i in met:
        mm = a_ == i
        if mm.any():
            meds.append(np.median(fx_[mm] / np.maximum(u_[mm], 1e-300)))
    meds = np.array(sorted(meds))
    print(f"  D={DIMS[d]:>2}: median of medians {np.median(meds):.3g}; exact zeros "
          f"{(meds==0).sum()}; at or behind control (>=1) {(meds>=1).sum()}; "
          f"worst {meds[-1]:.3g}")

print("\nH. the five outcome-neutral baselines, win rates at the top budget:")
five = [i for i, (li, a) in enumerate(algs)
        if libs[li] == "Baselines" and a in ("DE", "DiagonalCMA", "modcma", "bipop", "lshade")]
for name, ref in (("their RandomSearch", rref_top), ("uniform", uref)):
    lo, hi = 1200, 0
    for d in range(4):
        cnt = counts(ref, (bf == 6) & (dim == d), five)
        for i in five:
            lo = min(lo, cnt[i][0])
            hi = max(hi, cnt[i][0])
    print(f"  vs {name}: wins {lo} to {hi} of 1200")

print("\nI. the robust set (needs verdicts_uniform.tsv beside the data):")
try:
    from collections import defaultdict
    cells = defaultdict(list)
    for r in csv.DictReader(open("verdicts_uniform.tsv"), delimiter="\t"):
        if r["lib"] == "Baselines" or int(r["budget_factor"]) == 10:
            continue
        cells[(r["lib"], r["alg"])].append(r["verdict"])
    allb = sorted(k for k, v in cells.items() if len(v) == 24 and all(x == "better" for x in v))
    nw = sum(1 for v in cells.values() if all(x != "worse" for x in v))
    print(f"  shown better in all 24 cells: {len(allb)} of {len(cells)}; "
          f"never shown worse anywhere: {nw}")
    with open("robust_set.txt", "w") as f:
        for l, a in allb:
            f.write(f"{l}\t{a}\n")
    print("\nJ. per-library at the top budget (4 dimension cells per implementation):")
    st = defaultdict(lambda: [0, 0, 0])
    for r in csv.DictReader(open("verdicts_uniform.tsv"), delimiter="\t"):
        if r["lib"] == "Baselines" or int(r["budget_factor"]) != 10000:
            continue
        i = {"better": 0, "not shown": 1, "worse": 2}.get(r["verdict"])
        if i is not None:
            st[r["lib"]][i] += 1
    for l, (b, ns, w) in sorted(st.items()):
        n = b + ns + w
        print(f"  {l:11s} better {b}/{n} ({100*b/n:.0f}%)  not-shown {ns}  worse {w} ({100*w/n:.0f}%)")
except FileNotFoundError:
    print("  verdicts_uniform.tsv not present, skipped")
