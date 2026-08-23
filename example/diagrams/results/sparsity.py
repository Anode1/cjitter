"""Does sparsity explain the crossing and overlap findings? Per corpus and layout, Spearman's
rho between q and m/n over diagrams, with the normal-approximation p, and the median q in
the sparser and denser half. Reads the station CSVs in cells/.

    python3 sparsity.py CELLS_DIR
"""
import csv, math, os, statistics as st, sys
from statistics import NormalDist

def spearman(x, y):
    def rank(v):
        order = sorted(range(len(v)), key=lambda k: v[k])
        r = [0.0] * len(v); i = 0
        while i < len(order):
            j = i
            while j + 1 < len(order) and v[order[j + 1]] == v[order[i]]: j += 1
            for k in range(i, j + 1): r[order[k]] = (i + j) / 2 + 1
            i = j + 1
        return r
    rx, ry = rank(x), rank(y)
    mx, my = st.mean(rx), st.mean(ry)
    sx = math.sqrt(sum((a - mx) ** 2 for a in rx)); sy = math.sqrt(sum((a - my) ** 2 for a in ry))
    if sx == 0 or sy == 0: return float('nan'), 1.0
    rho = sum((a - mx) * (b - my) for a, b in zip(rx, ry)) / (sx * sy)
    z = rho * math.sqrt(len(x) - 1)                      # large-sample null
    return rho, 2 * (1 - NormalDist().cdf(abs(z)))

if __name__ == '__main__':
    cells = sys.argv[1]
    for stem, cname in [('hs', 'WikiPathways'), ('sbgn', 'Reactome'), ('bpmn', 'BPMN')]:
        for term in ['crossings_alone', 'overlap_alone', 'length_alone', 'stress_alone']:
            for lay in ['hand', 'dot']:
                rows = list(csv.DictReader(open(os.path.join(cells, '%s_%s_%s.csv' % (stem, lay, term)))))
                mn = [int(r['m']) / int(r['n']) for r in rows]
                q = [float(r['q']) for r in rows]
                rho, p = spearman(q, mn)
                med = st.median(mn)
                lo = [b for a, b in zip(mn, q) if a <= med]; hi = [b for a, b in zip(mn, q) if a > med]
                print('%-13s %-16s %-4s rho %+0.2f  p %-8.2g q sparse/dense %.2f / %.2f'
                      % (cname, term, lay, rho, p, st.median(lo), st.median(hi)))
        print()
