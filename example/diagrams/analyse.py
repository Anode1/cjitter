"""Paired statistics for a diagram measurement: two CSV files over the same diagrams, one row
each, a column of numbers in [0, 1]. Reports the shift from B to A and whether it is
distinguishable from zero.

    python3 analyse.py A.csv B.csv [--column q] [--alternative greater|less|two-sided] [--md]
    python3 analyse.py --family LABEL A1.csv B1.csv LABEL A2.csv B2.csv ...
    python3 analyse.py --selftest

The files pair on the id column and may be in any order; an id in one file and not the other
is an error naming it. d_i = A_i - B_i, so --alternative greater asks whether A is above B.
--family adds the Holm-adjusted signed-rank p over the pairs listed. Standard library only.

zeros: dropped, from both tests and from the shift estimate, which is Wilcoxon's convention
and R's wilcox.test, not Pratt's (Pratt ranks the zeros and drops them from the statistic
afterwards). The table gives n, the pairs, beside nz, the pairs that differ.

ties: equal |d| take midranks. The signed-rank p is then the exact conditional distribution
on the observed midranks: all 2^nz sign assignments, counted by dynamic programming over the
rank sums, doubled so that midranks are integers. R declines an exact p once |d| ties and
approximates instead; this does not.

nz > 50: the normal approximation, continuity correction 1/2 and the variance lowered by
sum(t^3 - t) / 48 over the tie groups, which is wilcox.test(exact = FALSE, correct = TRUE).

the interval: the Hodges-Lehmann estimate is the median of the nz(nz+1)/2 Walsh averages
(d_i + d_j)/2, i <= j. The 95% interval is the pair of Walsh order statistics at the critical
rank of the signed-rank null, that null being exact for nz <= 50 and, above it, the normal
quantile of the critical rank. Above 50 R root-finds on the approximated statistic for both,
so R's reported estimate is not the Walsh median there and its endpoints can sit an order
statistic or two off these. It is two-sided whatever --alternative
says. It is exactly distribution-free only when no |d| ties.
"""
import csv, math, os, statistics, sys
from fractions import Fraction
from statistics import NormalDist

ALTS = ('greater', 'less', 'two-sided')
ALPHA_2 = Fraction(1, 40)          # 0.025, half of a 95% interval's 0.05

def die(msg):
    sys.exit('analyse.py: ' + msg)

# ------------------------------------------------------------------- the null distributions

def midranks(xs):
    """Midranks of xs doubled, so a tied midrank is an integer, and the tie group sizes."""
    order = sorted(range(len(xs)), key=lambda k: xs[k])
    r2, groups, i = [0] * len(xs), [], 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and xs[order[j + 1]] == xs[order[i]]: j += 1
        for k in range(i, j + 1): r2[order[k]] = (i + 1) + (j + 1)   # 2 * mean of ranks i+1..j+1
        groups.append(j - i + 1)
        i = j + 1
    return r2, groups

def subset_sums(ws):
    """Counts of the 2^len(ws) subsets of ws by sum: one exact pass per weight."""
    c = [1] + [0] * sum(ws)
    for w in ws:
        for k in range(len(c) - 1, w - 1, -1): c[k] += c[k - w]
    return c

def signrank_exact(r2, v2, alt):
    """P of the signed-rank statistic at or beyond v2, over the observed doubled ranks r2."""
    c = subset_sums(r2)
    tot, ge, le = 1 << len(r2), sum(c[v2:]), sum(c[:v2 + 1])
    if alt == 'greater': return ge / tot
    if alt == 'less': return le / tot
    return min(1.0, 2 * min(ge, le) / tot)

def signrank_normal(v, n, groups, alt):
    z = v - n * (n + 1) / 4
    sd = math.sqrt(n * (n + 1) * (2 * n + 1) / 24 - sum(t ** 3 - t for t in groups) / 48)
    corr = 0.5 if alt == 'greater' else -0.5 if alt == 'less' else math.copysign(0.5, z) if z else 0.0
    z, nd = (z - corr) / sd, NormalDist()
    if alt == 'greater': return nd.cdf(-z)
    if alt == 'less': return nd.cdf(z)
    return min(1.0, 2 * min(nd.cdf(z), nd.cdf(-z)))

def qsignrank(n, p):
    """The least q with P(W <= q) >= p under the untied signed-rank null on n ranks."""
    if n <= 50:
        s, tot = 0, 1 << n
        for q, k in enumerate(subset_sums(list(range(1, n + 1)))):
            s += k
            if Fraction(s, tot) >= p: return q
        return n * (n + 1) // 2
    mu = n * (n + 1) / 4
    sd = math.sqrt(n * (n + 1) * (2 * n + 1) / 24)
    return max(0, math.ceil(mu + NormalDist().inv_cdf(float(p)) * sd - 0.5))

# ------------------------------------------------------------------- the four statistics

def wilcoxon(d, alt):
    """The signed-rank p and its statistic V, zeros dropped."""
    nz = [x for x in d if x != 0]
    n = len(nz)
    if n == 0: return 1.0, 0.0
    r2, groups = midranks([abs(x) for x in nz])
    v2 = sum(r for r, x in zip(r2, nz) if x > 0)
    if n <= 50: return signrank_exact(r2, v2, alt), v2 / 2
    return signrank_normal(v2 / 2, n, groups, alt), v2 / 2

def sign_test(d, alt):
    """The exact binomial p on the count of positive differences, zeros dropped."""
    nz = [x for x in d if x != 0]
    m = len(nz)
    if m == 0: return 1.0
    k, tot = sum(1 for x in nz if x > 0), 1 << m
    ge = sum(math.comb(m, i) for i in range(k, m + 1))
    le = sum(math.comb(m, i) for i in range(0, k + 1))
    if alt == 'greater': return ge / tot
    if alt == 'less': return le / tot
    return min(1.0, 2 * min(ge, le) / tot)

def walsh(d):
    return sorted((d[i] + d[j]) / 2 for i in range(len(d)) for j in range(i, len(d)))

def shift(d):
    """The Hodges-Lehmann estimate and its 95% interval, zeros dropped."""
    nz = [x for x in d if x != 0]
    n = len(nz)
    if n == 0: return 0.0, 0.0, 0.0
    w = walsh(nz)
    qu = max(1, qsignrank(n, ALPHA_2))
    return statistics.median(w), w[qu - 1], w[n * (n + 1) // 2 - qu]

def holm(ps):
    """Step-down adjustment, held monotone in the sorted order."""
    order, run, out = sorted(range(len(ps)), key=lambda i: ps[i]), 0.0, [0.0] * len(ps)
    for rank, i in enumerate(order):
        run = max(run, min(1.0, (len(ps) - rank) * ps[i]))
        out[i] = run
    return out

# ------------------------------------------------------------------- the files

def read_column(path, column):
    if not os.path.exists(path): die('cannot open ' + path)
    with open(path, newline='') as f:
        rows = list(csv.DictReader(f))
    if not rows: die(path + ' has no rows')
    if 'id' not in rows[0]: die(path + ' has no id column')
    if column not in rows[0]: die('%s has no column %s' % (path, column))
    order, value = [], {}
    for r in rows:
        i = r['id']
        if i in value: die('%s has id %s twice' % (path, i))
        try: value[i] = float(r[column])
        except (TypeError, ValueError): die('%s: id %s: %s is not a number' % (path, i, r[column]))
        order.append(i)
    return order, value

def paired(pa, pb, column):
    oa, a = read_column(pa, column)
    ob, b = read_column(pb, column)
    for i in oa:
        if i not in b: die('%s has no row for id %s' % (pb, i))
    for i in ob:
        if i not in a: die('%s has no row for id %s' % (pa, i))
    return [a[i] for i in oa], [b[i] for i in oa]

def analyse(label, pa, pb, column, alt):
    a, b = paired(pa, pb, column)
    d = [x - y for x, y in zip(a, b)]
    hl, lo, hi = shift(d)
    pw, v = wilcoxon(d, alt)
    return dict(label=label, n=len(d), nz=sum(1 for x in d if x != 0), v=v,
                ma=statistics.median(a), mb=statistics.median(b), md=statistics.median(d),
                hl=hl, lo=lo, hi=hi, ps=sign_test(d, alt), pw=pw)

# ------------------------------------------------------------------- the table

HEAD = ['pair', 'n', 'nz', 'median A', 'median B', 'median d', 'HL', 'CI lo', 'CI hi',
        'p sign', 'p signrank']

def fp(p):
    return '< 1e-4' if p < 1e-4 else '%.4g' % p

def cells(r, adj):
    row = [r['label'], str(r['n']), str(r['nz'])] + \
          ['%.3f' % r[k] for k in ('ma', 'mb', 'md', 'hl', 'lo', 'hi')] + [fp(r['ps']), fp(r['pw'])]
    return row + [fp(adj)] if adj is not None else row

def emit(head, table, md):
    if md:
        print('| ' + ' | '.join(head) + ' |')
        print('|' + '---|' * len(head))
        for r in table: print('| ' + ' | '.join(r) + ' |')
        return
    w = [max(len(h), *(len(r[i]) for r in table)) for i, h in enumerate(head)] if table else \
        [len(h) for h in head]
    line = lambda r: (r[0].ljust(w[0]) + ''.join('  ' + v.rjust(w[i]) for i, v in enumerate(r) if i)).rstrip()
    print(line(head))
    for r in table: print(line(r))

# ------------------------------------------------------------------- the self-test
# Every pinned value below names where it comes from. The two sources are R 4.4 (wilcox.test,
# binom.test, qsignrank, p.adjust), run on the same data, and hand arithmetic over the 2^n
# sign assignments. The exact signed-rank p under ties has no R reference: R declines it. It
# is checked against enumeration instead.

def naive_midrank(xs, i):
    """A midrank from its definition, sharing no code with midranks()."""
    return sum(1 for x in xs if x < xs[i]) + (sum(1 for x in xs if x == xs[i]) + 1) / 2

def brute_wilcoxon(d, alt):
    """The signed-rank p by walking all 2^n sign assignments. For n <= 12 only."""
    nz = [x for x in d if x != 0]
    n = len(nz)
    if n == 0: return 1.0
    ax = [abs(x) for x in nz]
    r = [naive_midrank(ax, i) for i in range(n)]
    v, ge, le = sum(r[i] for i in range(n) if nz[i] > 0), 0, 0
    for m in range(1 << n):
        s = sum(r[i] for i in range(n) if m >> i & 1)
        if s >= v: ge += 1
        if s <= v: le += 1
    tot = 1 << n
    if alt == 'greater': return ge / tot
    if alt == 'less': return le / tot
    return min(1.0, 2 * min(ge, le) / tot)

def brute_sign(d, alt):
    nz = [x for x in d if x != 0]
    m = len(nz)
    if m == 0: return 1.0
    k, ge, le = sum(1 for x in nz if x > 0), 0, 0
    for bits in range(1 << m):
        s = bin(bits).count('1')
        if s >= k: ge += 1
        if s <= k: le += 1
    tot = 1 << m
    if alt == 'greater': return ge / tot
    if alt == 'less': return le / tot
    return min(1.0, 2 * min(ge, le) / tot)

def selftest():
    import random
    ok = [0, 0]
    def check(name, got, want, tol=0.0):
        good = abs(got - want) <= tol if isinstance(want, float) else got == want
        ok[0 if good else 1] += 1
        if not good: print('  FAIL %s: expected [%r], got [%r]' % (name, want, got))

    # Hollander and Wolfe (1973), p. 29f, the Hamilton depression scores; the same data is the
    # one-sample example on R's ?wilcox.test page. By hand: the differences rank 8, 3, 9, 4, 7,
    # 6, 5, 2, 1 by |d| and the seven positive ones sum to V = 40. The upper tail P(V >= 40) is
    # by symmetry P(V <= 5), and the subsets of 1..9 summing to at most 5 number
    # 1+1+1+2+2+3 = 10, so p = 10/512 = 0.01953125. R agrees: V = 40, p-value = 0.01953125.
    x = [1.83, 0.50, 1.62, 2.48, 1.68, 1.88, 1.55, 3.06, 1.30]
    y = [0.878, 0.647, 0.598, 2.05, 1.06, 1.29, 1.06, 3.14, 1.29]
    d = [a - b for a, b in zip(x, y)]
    p, v = wilcoxon(d, 'greater')
    check('H&W V', v, 40.0)
    check('H&W greater', p, 10 / 512, 1e-15)
    check('H&W two-sided', wilcoxon(d, 'two-sided')[0], 20 / 512, 1e-15)   # R: 0.0390625
    check('H&W less', wilcoxon(d, 'less')[0], 505 / 512, 1e-15)   # R: 0.986328125
    # R: wilcox.test(x, y, paired = TRUE, conf.int = TRUE) gives (pseudo)median 0.46 and
    # 95 percent confidence interval 0.01 0.786.
    hl, lo, hi = shift(d)
    check('H&W HL', hl, 0.46, 1e-12)
    check('H&W CI lo', lo, 0.01, 1e-12)
    check('H&W CI hi', hi, 0.786, 1e-12)
    # Seven of the nine differences are positive. By hand P(X >= 7) = (36 + 9 + 1)/512 =
    # 0.08984375, which is R's binom.test(7, 9, 0.5, alternative = "greater").
    check('H&W sign greater', sign_test(d, 'greater'), 46 / 512, 1e-15)
    check('H&W sign two-sided', sign_test(d, 'two-sided'), 92 / 512, 1e-15)   # R: 0.1796875

    # R: qsignrank(0.025, n).
    for n, q in ((6, 1), (9, 6), (10, 9), (15, 26), (20, 53), (25, 90), (50, 435)):
        check('qsignrank %d' % n, qsignrank(n, ALPHA_2), q)
    # Above 50 the critical rank is the normal quantile. It should sit near the exact one, so
    # compare the two formulas on the range where both are available.
    for n in (30, 40, 50):
        mu = n * (n + 1) / 4
        sd = math.sqrt(n * (n + 1) * (2 * n + 1) / 24)
        approx = max(0, math.ceil(mu + NormalDist().inv_cdf(0.025) * sd - 0.5))
        check('critical rank agrees at n = %d' % n, abs(approx - qsignrank(n, ALPHA_2)) <= 2, True)

    # The dynamic programming against enumeration of all 2^n sign assignments, on differences
    # drawn off a coarse grid so that ties and zeros are common.
    rng = random.Random(7)
    for trial in range(60):
        n = rng.randint(1, 12)
        dd = [rng.randint(-3, 3) / 2 for _ in range(n)]
        for alt in ALTS:
            check('DP vs enumeration %d %s' % (trial, alt), wilcoxon(dd, alt)[0],
                  brute_wilcoxon(dd, alt), 1e-12)
            check('sign vs enumeration %d %s' % (trial, alt), sign_test(dd, alt),
                  brute_sign(dd, alt), 1e-12)
        # A shift of the data shifts the estimate and its interval by the same amount, and
        # mirroring the data mirrors both and swaps the one-sided p values. The shift is over
        # the non-zero differences: adding a constant to a zero difference would keep it.
        if any(v != 0 for v in dd):
            nz = [v for v in dd if v != 0]
            hl, lo, hi = shift(nz)
            hl2, lo2, hi2 = shift([v + 10 for v in nz])
            check('HL shifts %d' % trial, (hl2 - hl, lo2 - lo, hi2 - hi), (10.0, 10.0, 10.0))
            hl3, lo3, hi3 = shift([-v for v in nz])
            check('HL mirrors %d' % trial, (hl3, lo3, hi3), (-hl, -hi, -lo))
            check('alternative mirrors %d' % trial, wilcoxon([-v for v in dd], 'less')[0],
                  wilcoxon(dd, 'greater')[0], 1e-15)
            check('scale leaves the rank p %d' % trial, wilcoxon([3 * v for v in dd], 'greater')[0],
                  wilcoxon(dd, 'greater')[0], 1e-15)

    # The exact p under ties, which R will not compute: three |d| tie at 0.1, two at 0.2 and
    # two at 0.3, and two differences are zero. Enumeration is the only reference.
    u = [0.3, -0.1, 0.1, 0.0, 0.2, 0.2, -0.3, 0.0, 0.4, 0.1]
    for alt in ALTS:
        check('tied exact %s' % alt, wilcoxon(u, alt)[0], brute_wilcoxon(u, alt), 1e-12)
    check('tied V', wilcoxon(u, 'greater')[1], 27.5)     # R reports V = 27.5 on the same data
    # R, on the same u: wilcox.test(u, exact = FALSE, correct = TRUE) gives p = 0.2042192702898.
    r2, groups = midranks([abs(t) for t in u if t != 0])
    check('tied normal', signrank_normal(27.5, 8, groups, 'two-sided'), 0.20421927028984926, 1e-12)

    # The large-sample branch, nz = 59 after one zero drops, with heavy ties. Reproduce the
    # data in R as ((1:60 * 37) %% 41)/40 - 0.4; wilcox.test then gives V = 1203.5 and, at
    # exact = FALSE, correct = TRUE, the three p values pinned here.
    v60 = [((i * 37) % 41) / 40 - 0.4 for i in range(1, 61)]
    check('n = 60 nz', sum(1 for t in v60 if t != 0), 59)
    check('n = 60 V', wilcoxon(v60, 'greater')[1], 1203.5)
    check('n = 60 greater', wilcoxon(v60, 'greater')[0], 0.0081831908561027164, 1e-14)
    check('n = 60 less', wilcoxon(v60, 'less')[0], 0.99198408771298363, 1e-14)
    check('n = 60 two-sided', wilcoxon(v60, 'two-sided')[0], 0.016366381712205433, 1e-14)

    # R: p.adjust(p, "holm").
    check('holm four', [round(t, 12) for t in holm([0.01, 0.04, 0.03, 0.005])],
          [0.03, 0.06, 0.06, 0.02])
    check('holm monotone', [round(t, 12) for t in holm([0.2, 0.02, 0.2])], [0.4, 0.06, 0.4])
    check('holm caps at one', holm([0.6, 0.7])[1], 1.0)

    # Degenerate input: every difference zero.
    check('all zero p', wilcoxon([0.0, 0.0, 0.0], 'two-sided')[0], 1.0)
    check('all zero shift', shift([0.0, 0.0, 0.0]), (0.0, 0.0, 0.0))

    print('analyse: %d passed, %d failed' % (ok[0], ok[1]))
    return 1 if ok[1] else 0

# -------------------------------------------------------------------

if __name__ == '__main__':
    column, alt, md, family, args = 'q', 'two-sided', False, False, []
    a = sys.argv[1:]
    while a:
        if a[0] == '--column': column = a[1]; a = a[2:]
        elif a[0] == '--alternative': alt = a[1]; a = a[2:]
        elif a[0] == '--md': md = True; a = a[1:]
        elif a[0] == '--family': family = True; a = a[1:]
        elif a[0] == '--selftest': sys.exit(selftest())
        else:
            if a[0].startswith('--'): die('unknown argument ' + a[0])
            args.append(a[0]); a = a[1:]
    if alt not in ALTS: die('--alternative must be greater, less or two-sided, not %s' % alt)
    if family:
        if not args or len(args) % 3: die('--family wants LABEL A.csv B.csv per pair')
        rows = [analyse(args[i], args[i + 1], args[i + 2], column, alt) for i in range(0, len(args), 3)]
        adj = holm([r['pw'] for r in rows])
    elif len(args) == 2:
        rows, adj = [analyse(' vs '.join(os.path.basename(p)[:-4] if p.endswith('.csv')
                                         else os.path.basename(p) for p in args),
                             args[0], args[1], column, alt)], None
    else:
        die('wants two CSV files, or --family LABEL A.csv B.csv ...')
    head = HEAD + ['p Holm'] if adj else HEAD
    emit(head, [cells(r, adj[i] if adj else None) for i, r in enumerate(rows)], md)
    print('column %s, alternative %s, d = A - B; 95%% interval, zeros dropped, %s null.'
          % (column, alt, 'exact' if all(r['nz'] <= 50 for r in rows) else 'normal'))
