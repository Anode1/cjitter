"""Renders ERD_routed.png and ERD_prev_routed.png from graph_anon.json. These images are this renderer's
work, never a Workbench export -- an export would carry the real names, which is the reason the
committed pair can exist at all. The connectors are routed by the same rule as erd.c's
route_edge (two L shapes, Z shapes sliding across the channel, least penetration wins, length
breaks ties, earlier shape wins exact ties), so the pictures show orthogonal connectors, the
medium the objective scores, and never a diagonal.

ERD_straight is the same current revision with straight center-to-center edges, the
general-graph representation that diagonal-edge tools draw; the pair of pictures is the
argument for routing, so both are kept.

    python3 render.py && rsvg-convert -w 1800 ERD_anon.svg -o ERD_routed.png \
                      && rsvg-convert -w 1800 ERD_prev_anon.svg -o ERD_prev_routed.png \
                      && rsvg-convert -w 1800 ERD_straight_anon.svg -o ERD_straight.png
"""
import json

g = json.load(open('graph_anon.json'))
added = set(g['added'])

T = [-0.25, 0.15, 0.3, 0.5, 0.7, 0.85, 1.25]


def pen_of(pts, figs, skip):
    """Penetration of an axis-aligned polyline into every table except the endpoints' own,
    open intervals at the borders, exactly as erd.c's apen computes it."""
    p = 0.0
    for n, f in figs.items():
        if n in skip:
            continue
        x0, y0, w, h = f
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            if y1 == y2:
                if y0 < y1 < y0 + h:
                    lo, hi = max(min(x1, x2), x0), min(max(x1, x2), x0 + w)
                    if hi > lo:
                        p += hi - lo
            else:
                if x0 < x1 < x0 + w:
                    lo, hi = max(min(y1, y2), y0), min(max(y1, y2), y0 + h)
                    if hi > lo:
                        p += hi - lo
    return p


def route(a, b, figs, cx, offs):
    (ax, ay), (bx, by) = cx[a], cx[b]
    fa, fb = figs[a], figs[b]
    oa, ob = offs
    ya, xa = ay + oa * fa[3], ax + oa * fa[2]   # this edge's own departure lines
    yb, xb = by + ob * fb[3], bx + ob * fb[2]   # and arrival lines
    cands = [[(ax, ya), (xb, ya), (xb, by)], [(xa, ay), (xa, yb), (bx, yb)]]
    for t in T:
        mx = ax + t * (bx - ax)
        cands.append([(ax, ya), (mx, ya), (mx, yb), (bx, yb)])
    for t in T:
        my = ay + t * (by - ay)
        cands.append([(xa, ay), (xa, my), (xb, my), (xb, by)])
    best, bestcost = None, None
    for pts in cands:
        pen = pen_of(pts, figs, {a, b})
        length = sum(abs(x2 - x1) + abs(y2 - y1)
                     for (x1, y1), (x2, y2) in zip(pts, pts[1:]))
        cost = 100.0 * pen + length
        if bestcost is None or cost < bestcost:
            best, bestcost = pts, cost
        if pen == 0:
            break
    return best


def trim(p0, p1, f):
    """Border position as a fraction of the segment leaving a table's centre."""
    (x0, y0), (x1, y1) = p0, p1
    dx, dy = x1 - x0, y1 - y0
    t = 2.0
    if dx:
        t = min(t, (f[2] / 2) / abs(dx))
    if dy:
        t = min(t, (f[3] / 2) / abs(dy))
    return t


def anchor(pts, fa, fb):
    """Connectors leave a table's border, never its centre; same rule as erd.c's anchor."""
    pts = [p for i, p in enumerate(pts) if i == 0 or p != pts[i - 1]]
    if len(pts) < 2:
        return pts
    if len(pts) == 2:
        ta, tb = trim(pts[0], pts[1], fa), trim(pts[1], pts[0], fb)
        if ta + tb < 1:
            (x0, y0), (x1, y1) = pts
            dx, dy = x1 - x0, y1 - y0
            pts = [(x0 + ta * dx, y0 + ta * dy), (x1 - tb * dx, y1 - tb * dy)]
        return pts
    t = trim(pts[0], pts[1], fa)
    if t <= 1:
        (x0, y0), (x1, y1) = pts[0], pts[1]
        pts[0] = (x0 + t * (x1 - x0), y0 + t * (y1 - y0))
    t = trim(pts[-1], pts[-2], fb)
    if t <= 1:
        (x0, y0), (x1, y1) = pts[-1], pts[-2]
        pts[-1] = (x0 + t * (x1 - x0), y0 + t * (y1 - y0))
    return pts


def render(rev, path, mark_added, straight=False):
    figs, edges = rev['figs'], rev['edges']
    W = max(f[0] + f[2] for f in figs.values()) + 40
    H = max(f[1] + f[3] for f in figs.values()) + 40
    cx = {n: (f[0] + f[2] / 2, f[1] + f[3] / 2) for n, f in figs.items()}
    # each edge leaves its table at its own attachment slot, same rule as erd.c: the j-th of
    # a table's d edges at fraction ((j+1)/(d+1) - 1/2) * 0.8 of the side
    deg, seen = {}, {}
    for a, b in edges:
        deg[a] = deg.get(a, 0) + 1
        deg[b] = deg.get(b, 0) + 1
    slots = []
    for a, b in edges:
        sa, sb = seen.get(a, 0), seen.get(b, 0)
        seen[a], seen[b] = sa + 1, sb + 1
        slots.append((((sa + 1) / (deg[a] + 1) - 0.5) * 0.8,
                      ((sb + 1) / (deg[b] + 1) - 0.5) * 0.8))
    out = ["<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g' "
           "font-family='sans-serif'>" % (W, H),
           "<rect width='%g' height='%g' fill='#fafafa'/>" % (W, H)]
    for (a, b), off in zip(edges, slots):
        nu = mark_added and (a in added or b in added)
        pts = [cx[a], cx[b]] if straight else route(a, b, figs, cx, off)
        pts = anchor(pts, figs[a], figs[b])
        out.append("<polyline points='%s' fill='none' stroke='%s' stroke-width='2'/>"
                   % (' '.join('%g,%g' % p for p in pts), '#c60' if nu else '#999'))
    for n, f in figs.items():
        nu = mark_added and n in added
        out.append("<rect x='%g' y='%g' width='%g' height='%g' rx='6' fill='%s' "
                   "stroke='%s' stroke-width='2'/>"
                   % (f[0], f[1], f[2], f[3], '#fc3' if nu else '#e8e8e8',
                      '#963' if nu else '#555'))
        out.append("<text x='%g' y='%g' text-anchor='middle' font-size='22' fill='%s'>%s</text>"
                   % (f[0] + f[2] / 2, f[1] + f[3] / 2 + 7,
                      '#630' if nu else '#333', n))
    out.append('</svg>')
    open(path, 'w').write('\n'.join(out))
    print(path, len(figs), 'tables,', len(edges), 'edges, canvas %gx%g' % (W, H))


render(g['cur'], 'ERD_anon.svg', True)
render(g['prev'], 'ERD_prev_anon.svg', False)
render(g['cur'], 'ERD_straight_anon.svg', True, straight=True)
