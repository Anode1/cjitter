"""Fit the aesthetic energy's weights so that human layouts are as near-stationary as the
model allows, by random search over log-uniform weights (Bergstra and Bengio). Fitted on a
training half of the corpus, reported on a held-out half, so the number quoted is not the
number optimised."""
import json, glob, os, random, subprocess, statistics as st, sys

CAP = sys.argv[1] if len(sys.argv) > 1 else '0.02'
BUDGET = sys.argv[2] if len(sys.argv) > 2 else '4000'
DRAWS = int(sys.argv[3]) if len(sys.argv) > 3 else 60
NG = int(sys.argv[4]) if len(sys.argv) > 4 else 80

def load(d, lo, hi):
    out = []
    for fn in sorted(glob.glob(os.path.join(d, '*.json'))):
        g = json.load(open(fn))
        if not (lo <= g['n'] <= hi) or g['m'] < 1: continue
        xs = [p[0] for p in g['xy']]; ys = [p[1] for p in g['xy']]
        s = max(max(xs)-min(xs), max(ys)-min(ys))
        if s <= 0: continue
        lines = ["%d %d" % (g['n'], g['m'])]
        for (x, y), (w, h) in zip(g['xy'], g['wh']):
            lines.append("%.6f %.6f %.6f %.6f" % ((x-min(xs))/s, (y-min(ys))/s, w/s, h/s))
        for a, b in g['e']: lines.append("%d %d" % (a, b))
        out.append("\n".join(lines))
    return out

G = load('pr_hs', 15, 40)
random.seed(7); random.shuffle(G); G = G[:2*NG]
train, test = G[:NG], G[NG:]

def residual(graphs, wc, wl, wo):
    inp = "%d\n" % len(graphs) + "\n".join(graphs)
    o = subprocess.run(['./stat3', BUDGET, '12345', CAP, str(wc), str(wl), str(wo)],
                       input=inp, capture_output=True, text=True)
    r = []
    for line in o.stdout.strip().split("\n"):
        v = line.split()
        if len(v) < 6: continue
        e, ed = float(v[1]), float(v[2])
        if e > 0: r.append(100.0*(e-ed)/e)
    return st.median(r) if r else 100.0

base = residual(test, 1, 1, 1)
print("weights (1,1,1), asserted        : held-out residual %.2f%%" % base)
best = None
for i in range(DRAWS):
    wc = 10**random.uniform(-2, 2); wl = 10**random.uniform(-2, 2); wo = 10**random.uniform(-2, 2)
    s = wc+wl+wo; wc, wl, wo = 3*wc/s, 3*wl/s, 3*wo/s
    r = residual(train, wc, wl, wo)
    if best is None or r < best[0]: best = (r, wc, wl, wo)
print("best on TRAIN: residual %.2f%%  weights cross %.3f length %.3f overlap %.3f"
      % best)
held = residual(test, best[1], best[2], best[3])
print("same weights on HELD-OUT half    : residual %.2f%%" % held)
print("so fitting the weights moves the residual %.2f -> %.2f points" % (base, held))
