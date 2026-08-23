"""SBGN-ML (Reactome) to graph JSON with edge direction and drawn waypoints.

Node rules are those of pilot/parse_sbgn.py: glyphs that are containers or annotations are
not nodes, a port resolves to its owning glyph, node order is the order the glyph regex meets
them, edge order is the sorted index pair. Files under 10 nodes or without an edge are dropped.

Direction: every arc is drawn source to target, so directed is true throughout. Consumption
runs entity to process, production runs process to entity.

Waypoints: the <next> points of the arc, in canvas coordinates like start and end. A trailing
<next> that repeats <end> exactly is dropped, being an endpoint and not an interior point;
Reactome writes that duplicate on 98.3% of the 18651 arcs that carry any <next> at all, so
most arcs come out as chords. Arcs between a pair already connected keep the first arc's
route.

"ends" carries the <start> and <end> points, the attachment the file draws on the two glyphs;
the six contract fields ignore them.
"""
import re, json, os, sys, glob

SKIP = {'compartment', 'submap', 'annotation', 'tag'}
PT = re.compile(r'<(start|next|end)\b[^>]*x="([-\d.eE]+)"[^>]*y="([-\d.eE]+)"')
ARC = re.compile(r'<arc\b([^>]*?)/>|<arc\b([^>]*)>(.*?)</arc>', re.S)

def parse(path):
    t = open(path, encoding='utf-8', errors='replace').read()
    nodes, owner = {}, {}
    for m in re.finditer(r'<glyph\b([^>]*)>(.*?)</glyph>', t, re.S):
        at, body = m.group(1), m.group(2)
        gid = re.search(r'id="([^"]+)"', at)
        cls = re.search(r'class="([^"]+)"', at)
        if not gid or not cls or cls.group(1) in SKIP:
            continue
        bb = re.search(r'<bbox\s+w="([-\d.eE]+)"\s+h="([-\d.eE]+)"\s+x="([-\d.eE]+)"\s+y="([-\d.eE]+)"', body)
        if not bb:
            continue
        w, h, x, y = (float(bb.group(i)) for i in (1, 2, 3, 4))
        nodes[gid.group(1)] = (x + w / 2, y + h / 2, w, h)
        for p in re.finditer(r'<port\b[^>]*id="([^"]+)"', body):
            owner[p.group(1)] = gid.group(1)
    idx = {k: i for i, k in enumerate(nodes)}
    seen = {}
    for m in ARC.finditer(t):
        at = m.group(1) if m.group(1) is not None else m.group(2)
        body = m.group(3) or ''
        s = re.search(r'source="([^"]+)"', at)
        d = re.search(r'target="([^"]+)"', at)
        if not s or not d:
            continue
        a, b = owner.get(s.group(1), s.group(1)), owner.get(d.group(1), d.group(1))
        if a not in idx or b not in idx or a == b:
            continue
        pts = {'start': [], 'next': [], 'end': []}
        for q in PT.finditer(body):
            pts[q.group(1)].append([float(q.group(2)), float(q.group(3))])
        wp = list(pts['next'])
        end = pts['end'][-1] if pts['end'] else None
        start = pts['start'][0] if pts['start'] else None
        while wp and end and wp[-1] == end:
            wp.pop()
        while wp and start and wp[0] == start:
            wp.pop(0)
        cls = re.search(r'class="([^"]+)"', at)
        seen.setdefault((min(idx[a], idx[b]), max(idx[a], idx[b])),
                        (idx[a], idx[b], wp, 'stored' if wp else 'chord',
                         cls.group(1) if cls else '',
                         [start, end] if start and end else None))
    ks = sorted(seen)
    nk = list(nodes)
    return dict(file=os.path.basename(path), n=len(nk), m=len(ks),
                xy=[[nodes[k][0], nodes[k][1]] for k in nk],
                wh=[[nodes[k][2], nodes[k][3]] for k in nk],
                e=[[seen[k][0], seen[k][1]] for k in ks],
                directed=[True] * len(ks),
                wp=[seen[k][2] for k in ks],
                route=[seen[k][3] for k in ks],
                ids=nk, arc=[seen[k][4] for k in ks], ends=[seen[k][5] for k in ks])

if __name__ == '__main__':
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    ok = 0
    for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.sbgn'))):
        try:
            g = parse(f)
        except Exception:
            continue
        if g['n'] >= 10 and g['m'] >= 1:
            json.dump(g, open(os.path.join(out, g['file'] + '.json'), 'w'), separators=(',', ':'))
            ok += 1
    print('parsed', ok)
