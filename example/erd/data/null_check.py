"""How much geometric signal the maintainer's ten placed tables carry.

    python3 null_check.py [draws] [seed]

The example scores a search against uniform random placement of the ten tables a
migration added, with the other 34 frozen. That comparison only means something if the
maintainer's own placement is itself distinguishable from a random draw. This measures
whether it is: it redraws the ten uniformly in the canvas box `draws` times and reports
where the maintainer sits in the resulting null, one criterion at a time.

P is the fraction of random draws scoring at least as well as the maintainer, so 0.5 is
no signal at all and a small P is signal. Tolerances are in canvas units.
"""
import json, os, sys

TOL, TIE_TOL = 5.0, 1.0
BASE = os.path.dirname(os.path.abspath(__file__))

def load():
    d = json.load(open(os.path.join(BASE, 'graph_anon.json')))
    return d['cur']['figs'], d['cur']['edges'], set(d['added'])

def lines(figs, names):
    """Every x and y a box contributes: its two borders and its centre."""
    xs, ys = [], []
    for n in names:
        x, y, w, h = figs[n]
        xs += [x, x + w / 2.0, x + w]
        ys += [y, y + h / 2.0, y + h]
    return xs, ys

def axes_on_line(place, figs, frozen_xs, frozen_ys, tol):
    """Of the 2 axes each added table has, how many sit on a frozen line."""
    hit = 0
    for n, (x, y, w, h) in place.items():
        cx, cy = x + w / 2.0, y + h / 2.0
        if any(abs(cx - v) <= tol or abs(x - v) <= tol or abs(x + w - v) <= tol for v in frozen_xs):
            hit += 1
        if any(abs(cy - v) <= tol or abs(y - v) <= tol or abs(y + h - v) <= tol for v in frozen_ys):
            hit += 1
    return hit

def mutual(place, tol):
    """Pairs among the ten sharing a centre axis."""
    ns, c = sorted(place), 0
    for i in range(len(ns)):
        xi, yi, wi, hi = place[ns[i]]
        for j in range(i + 1, len(ns)):
            xj, yj, wj, hj = place[ns[j]]
            if abs((xi + wi / 2.0) - (xj + wj / 2.0)) <= tol or \
               abs((yi + hi / 2.0) - (yj + hj / 2.0)) <= tol:
                c += 1
    return c

def neighbour(place, figs, nbr, tol):
    """Added tables sharing a centre axis with a foreign-key neighbour."""
    c = 0
    for n, (x, y, w, h) in place.items():
        cx, cy = x + w / 2.0, y + h / 2.0
        for m in nbr.get(n, ()):
            if m in place:
                xm, ym, wm, hm = place[m]
            elif m in figs:
                xm, ym, wm, hm = figs[m]
            else:
                continue
            if abs(cx - (xm + wm / 2.0)) <= tol or abs(cy - (ym + hm / 2.0)) <= tol:
                c += 1
                break
    return c

def spread(place):
    ps = [(x + w / 2.0, y + h / 2.0) for x, y, w, h in place.values()]
    mx = sum(p[0] for p in ps) / len(ps)
    my = sum(p[1] for p in ps) / len(ps)
    return (sum((p[0] - mx) ** 2 + (p[1] - my) ** 2 for p in ps) / len(ps)) ** 0.5

def overlap(place, others):
    total = 0.0
    boxes = list(place.values())
    for i, (x, y, w, h) in enumerate(boxes):
        for (a, b, c, d) in boxes[i + 1:] + others:
            ox = min(x + w, a + c) - max(x, a)
            oy = min(y + h, b + d) - max(y, b)
            if ox > 0 and oy > 0:
                total += ox * oy
    return total

def main():
    draws = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    figs, edges, added = load()
    frozen = {n: v for n, v in figs.items() if n not in added}
    human = {n: v for n, v in figs.items() if n in added}
    others = list(frozen.values())
    fxs, fys = lines(figs, frozen)
    nbr = {}
    for a, b in edges:
        nbr.setdefault(a, set()).add(b)
        nbr.setdefault(b, set()).add(a)

    xs = [v[0] for v in figs.values()]; ys = [v[1] for v in figs.values()]
    x0, x1 = min(xs), max(v[0] + v[2] for v in figs.values())
    y0, y1 = min(ys), max(v[1] + v[3] for v in figs.values())

    crit = [
        ('aligned to frozen lines, tol %g' % TOL,
         lambda p: axes_on_line(p, figs, fxs, fys, TOL), 'hi'),
        ('exact ties, tol %g' % TIE_TOL,
         lambda p: axes_on_line(p, figs, fxs, fys, TIE_TOL), 'hi'),
        ('mutual alignment among the ten, tol %g' % TOL,
         lambda p: mutual(p, TOL), 'hi'),
        ('alignment to FK neighbours, tol %g' % TOL,
         lambda p: neighbour(p, figs, nbr, TOL), 'hi'),
        ('rms spread from own centroid', spread, 'either'),
        ('box overlap area', lambda p: overlap(p, others), 'lo'),
    ]

    s = seed & 0xFFFFFFFF
    def rnd():
        nonlocal s
        s ^= (s << 13) & 0xFFFFFFFF; s ^= s >> 17; s ^= (s << 5) & 0xFFFFFFFF
        return s / 4294967296.0

    hv = [f(human) for _, f, _ in crit]
    samples = [[] for _ in crit]
    for _ in range(draws):
        p = {}
        for n in human:
            _, _, w, h = figs[n]
            p[n] = [x0 + rnd() * max(1.0, (x1 - x0 - w)), y0 + rnd() * max(1.0, (y1 - y0 - h)), w, h]
        for i, (_, f, _) in enumerate(crit):
            samples[i].append(f(p))

    print('canvas %.0f x %.0f, %d frozen tables, %d placed, %d draws, seed %d'
          % (x1 - x0, y1 - y0, len(frozen), len(human), draws, seed))
    print('%-38s %12s %12s %8s' % ('criterion', 'maintainer', 'random med', 'P'))
    for i, (name, _, dirn) in enumerate(crit):
        v, col = hv[i], sorted(samples[i])
        med = col[len(col) // 2]
        if dirn == 'hi':
            p = sum(1 for c in col if c >= v) / float(len(col))
        elif dirn == 'lo':
            p = sum(1 for c in col if c <= v) / float(len(col))
        else:
            p = sum(1 for c in col if c <= v) / float(len(col))
        print('%-38s %12.4g %12.4g %8.3f' % (name, v, med, p))

main()
