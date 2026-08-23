"""The weight fit: which non-negative weights on the terms make a hand layout nearest to a
local minimum, by inverse optimisation on the directional differences station prints.

    ./station direct --corpus data/hs.txt --weights 1,1,1,1,1,1,1,1 --diffs > hs.diffs
    python3 fit.py hs.diffs [--terms C,O,L,S,R,A,N,F] [--folds 5] [--seed 1] [--md]
    python3 fit.py hs.diffs --sweep L --base C,O        q as the weight on one term grows

Node i is held under weights w exactly when sum_k w_k D_ivk >= 0 for every direction v, D the
change of term k when node i moves along v. The fit is the linear program

    minimise  sum_iv s_iv / nodes   over w on the simplex of the chosen terms, s >= 0,
    subject to  sum_k w_k D_ivk + s_iv >= 0,

whose value, the residual, is 0 only if some weighting holds every node. Because the residual
depends on the terms' scales and q does not, both are reported: q(w) is the fraction of nodes
held under w, with ties (sum >= -1e-12) held, computed from the same differences without
running station again. Cross-validation is 5x2: five seeded random halvings of the diagrams,
a fit on each half evaluated on the other, the ten held-out q values averaged; the fit on every
diagram is reported beside it. Needs scipy (HiGHS); the repository's .venv has it.
"""
import csv, random, sys
import numpy as np
from scipy.optimize import linprog

TERMS = 'COLSRANF'
NAMES = ['crossings', 'overlap', 'length', 'stress', 'orthogonality', 'alignment', 'node-edge', 'flow']

def read(path):
    """ids (one per row), and D as an array [rows, 8]; rows are (diagram, node, direction)."""
    ids, node, D = [], [], []
    with open(path) as f:
        for r in csv.DictReader(f):
            ids.append(r['id']); node.append(int(r['node']))
            D.append([float(r[n]) for n in NAMES])
    return np.array(ids), np.array(node), np.array(D)

def held_fraction(D, ids, node, w):
    """Fraction of (diagram, node) pairs whose every direction satisfies D.w >= -1e-12."""
    s = D @ w
    key = np.char.add(np.char.add(ids, ':'), node.astype(str))
    order = np.argsort(key, kind='stable')
    key, s = key[order], s[order]
    starts = np.r_[0, np.flatnonzero(key[1:] != key[:-1]) + 1]
    held = np.logical_and.reduceat(s >= -1e-12, starts)
    return held.mean()

def fit(D, cols):
    """LP over the simplex on the columns COLS. Returns w (length 8, zeros elsewhere), residual."""
    A = D[:, cols]
    rows, k = A.shape
    # variables: w (k), s (rows); minimise sum s; -A w - s <= 0; sum w = 1; bounds >= 0
    c = np.r_[np.zeros(k), np.ones(rows)]
    from scipy.sparse import hstack, identity, csr_matrix
    A_ub = hstack([csr_matrix(-A), -identity(rows)])
    res = linprog(c, A_ub=A_ub, b_ub=np.zeros(rows), A_eq=np.r_[np.ones(k), np.zeros(rows)][None, :],
                  b_eq=[1.0], bounds=[(0, None)] * (k + rows), method='highs')
    if not res.success: sys.exit('LP failed: ' + res.message)
    w = np.zeros(len(TERMS)); w[cols] = res.x[:k]
    return w, res.fun

def nodes_in(ids, node):
    return len(set(zip(ids.tolist(), node.tolist())))

if __name__ == '__main__':
    a = sys.argv[1:]
    path, terms, folds, seed, md, sweep, base = None, 'COLSRANF', 5, 1, False, None, 'C,O'
    while a:
        if a[0] == '--terms': terms = a[1].replace(',', ''); a = a[2:]
        elif a[0] == '--folds': folds = int(a[1]); a = a[2:]
        elif a[0] == '--seed': seed = int(a[1]); a = a[2:]
        elif a[0] == '--sweep': sweep = a[1]; a = a[2:]
        elif a[0] == '--base': base = a[1]; a = a[2:]
        elif a[0] == '--md': md = True; a = a[1:]
        elif path is None: path = a[0]; a = a[1:]
        else: sys.exit('unknown argument ' + a[0])
    if path is None: sys.exit(__doc__)
    ids, node, D = read(path)
    N = nodes_in(ids, node)
    if sweep:
        # q when weight t goes on one term and the rest, equally, on the base terms
        cols = [TERMS.index(c) for c in base.replace(',', '')]
        k = TERMS.index(sweep)
        print('weight on %s (base %s equal)  q' % (NAMES[k], base))
        for t in [0, 0.001, 0.01, 0.05, 0.1, 0.2, 0.5, 1.0]:
            w = np.zeros(len(TERMS)); w[cols] = (1 - t) / len(cols); w[k] = t
            print('%6.3f  %.3f' % (t, held_fraction(D, ids, node, w)))
        sys.exit(0)
    cols = [TERMS.index(c) for c in terms]
    # the fit on everything
    w_all, r_all = fit(D, cols)
    q_all = held_fraction(D, ids, node, w_all)
    # 5x2 cross-validation over diagrams
    diagrams = sorted(set(ids.tolist()))
    rng = random.Random(seed)
    q_out, r_out, ws = [], [], []
    for rep in range(folds):
        d = diagrams[:]; rng.shuffle(d)
        half = set(d[:len(d) // 2])
        mask = np.array([i in half for i in ids])
        for train in (mask, ~mask):
            w, r = fit(D[train], cols)
            ws.append(w)
            q_out.append(held_fraction(D[~train], ids[~train], node[~train], w))
            r_out.append(r / nodes_in(ids[~train], node[~train]))
    head = ['terms', 'residual / node', 'q fitted', 'q held-out (5x2)'] + [NAMES[c] for c in cols]
    row = [terms, '%.4f' % (r_all / N), '%.3f' % q_all, '%.3f [%.3f, %.3f]' % (np.mean(q_out), min(q_out), max(q_out))] \
        + ['%.3f' % w_all[c] for c in cols]
    if md:
        print('| ' + ' | '.join(head) + ' |'); print('|' + '---|' * len(head)); print('| ' + ' | '.join(row) + ' |')
    else:
        for h, v in zip(head, row): print('%-20s %s' % (h, v))
    print('diagrams %d, nodes %d, rows %d' % (len(diagrams), N, len(ids)))
