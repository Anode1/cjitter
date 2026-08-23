"""GPML (WikiPathways) to graph JSON with edge direction and drawn waypoints.

Node and edge rules are those of pilot/parse_gpml.py: DataNode glyphs with a GraphId are
nodes, an interaction endpoint that names an Anchor resolves to an endpoint of the anchor's
host interaction, the largest connected component is kept, files outside 10..400 nodes are
dropped. Node order is sorted GraphId, edge order is sorted index pair, both as in the pilot.

Direction: the drawn order is Point[0] to Point[-1]; an Interaction whose last Point carries
an ArrowHead is directed, GraphicalLine and arrowless Interaction are not.

Waypoints: the Point elements between the first and the last. On 57.4% of the 4750 Elbow,
Curved and Segmented interactions the file stores nothing but the two endpoints and PathVisio
computes the bends at draw time, so those routes carry their ConnectorType as the label and
whatever points the file does store. An endpoint on an Anchor gives route "anchor": the last segment
to the resolved node is not drawn in the file, and the anchor position, interpolated by arc
length along the host interaction's stored polyline at the Anchor Position fraction, is
added as the waypoint on that side.

"ends" carries the two Point coordinates the file draws on the node boxes; the six contract
fields ignore them, and the side that ends on an Anchor is null there, its drawn end being
the anchor position already in wp.
"""
import sys, os, glob, json, math, xml.etree.ElementTree as ET
from collections import defaultdict

ARROWLESS = {'', 'Line', 'None', 'none'}

def strip(t):
    return t.split('}')[-1]

def at_fraction(pts, f):
    """Point at fraction f of the arc length of a polyline."""
    if len(pts) < 2:
        return None
    seg = [math.dist(pts[i], pts[i + 1]) for i in range(len(pts) - 1)]
    total = sum(seg)
    if total <= 0:
        return list(pts[0])
    want = max(0.0, min(1.0, f)) * total
    for i, s in enumerate(seg):
        if want <= s or i == len(seg) - 1:
            t = want / s if s > 0 else 0.0
            (x0, y0), (x1, y1) = pts[i], pts[i + 1]
            return [x0 + t * (x1 - x0), y0 + t * (y1 - y0)]
        want -= s
    return list(pts[-1])

def parse(fn):
    try:
        root = ET.parse(fn).getroot()
    except Exception:
        return None
    nodes = {}
    for el in root.iter():
        if strip(el.tag) == 'DataNode':
            gid = el.get('GraphId')
            g = [c for c in el if strip(c.tag) == 'Graphics']
            if not gid or not g:
                continue
            g = g[0]
            try:
                nodes[gid] = (float(g.get('CenterX')), float(g.get('CenterY')),
                              float(g.get('Width', '0')), float(g.get('Height', '0')))
            except (TypeError, ValueError):
                pass
    anchor_owner, anchor_pos, inters = {}, {}, []
    for el in root.iter():
        tag = strip(el.tag)
        if tag not in ('Interaction', 'GraphicalLine'):
            continue
        g = [c for c in el if strip(c.tag) == 'Graphics']
        if not g:
            continue
        pts = [c for c in g[0] if strip(c.tag) == 'Point']
        idx = len(inters)
        for a in [c for c in g[0] if strip(c.tag) == 'Anchor']:
            if a.get('GraphId'):
                anchor_owner[a.get('GraphId')] = idx
                try:
                    anchor_pos[a.get('GraphId')] = float(a.get('Position', '0.5'))
                except ValueError:
                    anchor_pos[a.get('GraphId')] = 0.5
        xy = []
        for p in pts:
            try:
                xy.append((float(p.get('X')), float(p.get('Y'))))
            except (TypeError, ValueError):
                xy.append(None)
        ok = None not in xy and len(pts) >= 2
        inters.append(dict(tag=tag, conn=g[0].get('ConnectorType') or 'Straight',
                           a=pts[0].get('GraphRef') if len(pts) >= 2 else None,
                           b=pts[-1].get('GraphRef') if len(pts) >= 2 else None,
                           arrow=(pts[-1].get('ArrowHead') or '') if len(pts) >= 2 else '',
                           xy=xy if ok else []))

    def resolve(ref, depth=0):
        if ref is None or depth > 4:
            return None
        if ref in nodes:
            return ref
        if ref in anchor_owner:
            h = inters[anchor_owner[ref]]
            return resolve(h['a'], depth + 1) or resolve(h['b'], depth + 1)
        return None

    def anchor_xy(ref):
        h = inters[anchor_owner[ref]]
        return at_fraction(h['xy'], anchor_pos.get(ref, 0.5))

    return nodes, inters, resolve, anchor_xy, anchor_owner

def lcc(nodes, edges):
    adj = defaultdict(set)
    for a, b in edges:
        adj[a].add(b); adj[b].add(a)
    seen, best = set(), []
    for s in adj:
        if s in seen:
            continue
        st, comp = [s], []
        seen.add(s)
        while st:
            u = st.pop(); comp.append(u)
            for v in adj[u]:
                if v not in seen:
                    seen.add(v); st.append(v)
        if len(comp) > len(best):
            best = comp
    return set(best)

def graph(fn):
    r = parse(fn)
    if not r:
        return None
    nodes, inters, resolve, anchor_xy, anchor_owner = r
    drawn = []
    for it in inters:
        ra, rb = resolve(it['a']), resolve(it['b'])
        if not ra or not rb or ra == rb:
            continue
        wp = [list(p) for p in it['xy'][1:-1]]
        route = {'Straight': 'stored', 'Elbow': 'elbow', 'Curved': 'curved',
                 'Segmented': 'segmented'}.get(it['conn'], 'stored')
        if route == 'stored' and not wp:
            route = 'chord'
        aa = it['a'] not in nodes and it['a'] in anchor_owner
        ab = it['b'] not in nodes and it['b'] in anchor_owner
        if aa or ab:
            route = 'anchor'
            if ab:
                p = anchor_xy(it['b'])
                if p:
                    wp.append(p)
            if aa:
                p = anchor_xy(it['a'])
                if p:
                    wp.insert(0, p)
        en = [None if aa else list(it['xy'][0]), None if ab else list(it['xy'][-1])] \
            if it['xy'] else None
        drawn.append((ra, rb, it['tag'] == 'Interaction' and it['arrow'] not in ARROWLESS,
                      wp, route, it['conn'], en))
    if not drawn:
        return None
    comp = lcc(nodes, sorted({tuple(sorted((d[0], d[1]))) for d in drawn}))
    ids = sorted(comp)
    ix = {g: i for i, g in enumerate(ids)}
    seen = {}
    for ra, rb, di, wp, route, conn, en in drawn:
        if ra not in ix or rb not in ix:
            continue
        k = (min(ix[ra], ix[rb]), max(ix[ra], ix[rb]))
        seen.setdefault(k, (ix[ra], ix[rb], di, wp, route, conn, en))
    n, m = len(ids), len(seen)
    if not (10 <= n <= 400) or m == 0:
        return None
    ks = sorted(seen)
    return dict(file=os.path.basename(fn), n=n, m=m,
                xy=[[nodes[g][0], nodes[g][1]] for g in ids],
                wh=[[nodes[g][2], nodes[g][3]] for g in ids],
                e=[[seen[k][0], seen[k][1]] for k in ks],
                directed=[seen[k][2] for k in ks],
                wp=[seen[k][3] for k in ks],
                route=[seen[k][4] for k in ks],
                ids=ids, connector=[seen[k][5] for k in ks],
                ends=[seen[k][6] for k in ks])

if __name__ == '__main__':
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    ok = 0
    for fn in sorted(glob.glob(os.path.join(sys.argv[1], '*.gpml'))):
        g = graph(fn)
        if not g:
            continue
        json.dump(g, open(os.path.join(out, os.path.basename(fn)[:-5] + '.json'), 'w'), separators=(',', ':'))
        ok += 1
    print('parsed', ok)
