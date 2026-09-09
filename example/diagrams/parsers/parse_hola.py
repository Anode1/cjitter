"""The HOLA formative drawings, from the study's published SVGs to the corpus format.

    python3 parse_hola.py ../data/hola.txt [--cache DIR]

Kieffer, Dwyer, Marriott and Wybrow ran a formative study for HOLA (TVCG 22(1), 2016) in
which 17 participants each laid out the same 8 small abstract graphs in the study's own
orthogonal editor, and published the 136 drawings as SVG at

    https://data.graphlayout.net/HOLA/formative/

That page carries no licence, so this repository redistributes none of it: data/hola.txt
is generated, not committed, and the numbers in the paper are reproduced by running this
script rather than by reading a copy of someone else's data. With --cache the SVGs are
kept in DIR so a rerun costs no requests; the cache is not committed either. The output
was checked against the corpus the paper measured, from a cold fetch: the same file.

Nodes. Inside <g id="nodes"> each node is a <g> whose transform="translate(x,y)" is the
centre. It holds two rects, a wide halo carrying display="none" and the drawn box; the
drawn box gives the width and height, which vary per node as the study drew them, 30x30
for a plain vertex and text-sized boxes such as 50x30 elsewhere. Document order is the
corpus order.

Edges. Inside <g id="edges"> each edge is a <g> holding several paths that share one
geometry: a wide background path, a transparent highlight, the drawn route in
stroke:black, and per-segment hit targets. The drawn route is read, or where its stroke
is none, as on one edge of 78d30-5.svg, the first path in the group with the same shape.
The route is walked through M, L, absolute H and V, and the relative arcs the editor puts
at corners, and only its two ends are kept: each lies exactly on the border of one node's
box, which is how the endpoint is matched to the node. Bends are dropped and the edge is
emitted as a chord (U with no route points), because the study fixed each graph and let
participants move nodes and bends, so the bends are the router's and not the drawing's.
Every edge is undirected here.

Graph identity. The file name is <participant>-<task>.svg and the task number is the
presentation order, randomised per participant, so it names no graph. Each drawing is
matched by isomorphism against the eight reference drawings theGraphs/h1.svg to h8.svg on
the same page, and the corpus id is <participant>_g<k> for the reference it matches: g1 K4 (4
nodes, 6 edges), g2 Dog (9, 10), g3 Binary Tree (7, 6), g4 Elephant (12, 12), g5 Snail
(13, 14), g6 Robot (7, 6), g7 Montana (12, 13), g8 Potted Plant (10, 10). Each of the 136
matches exactly one reference and each participant covers all eight, which is the check
that the matching is right. Graphs are emitted by participant, then by g number.

The outer translate,scale,translate on the SVG is uniform within a drawing and is ignored;
the reader rescales every layout to the unit square anyway.
"""
import sys, os, re, math, urllib.request
from collections import defaultdict

BASE = 'https://data.graphlayout.net/HOLA/formative'
REFS = ['theGraphs/h%d.svg' % k for k in range(1, 9)]


def fetch(name, cache):
    """The SVG text of NAME, from CACHE if it is there, else from the study's page."""
    if cache:
        p = os.path.join(cache, os.path.basename(name))
        if os.path.exists(p):
            return open(p, encoding='utf-8').read()
    url = '%s/%s' % (BASE, name)
    with urllib.request.urlopen(url, timeout=60) as r:
        s = r.read().decode('utf-8')
    if cache:
        os.makedirs(cache, exist_ok=True)
        open(os.path.join(cache, os.path.basename(name)), 'w', encoding='utf-8').write(s)
    return s


def group(s, gid):
    """The text of the top-level <g id="GID"> element, brace matching on <g> and </g>."""
    m = re.search(r'<g id="%s"[^>]*>' % re.escape(gid), s)
    if not m:
        return ''
    i, depth, j = m.end(), 1, m.end()
    for t in re.finditer(r'<g\b[^>]*?(/?)>|</g>', s[i:]):
        depth += -1 if t.group(0) == '</g>' else (0 if t.group(1) else 1)
        if depth == 0:
            j = i + t.start()
            break
    return s[i:j]


def nodes(s):
    """(centre x, centre y, width, height) per node, in document order."""
    out = []
    blk = group(s, 'nodes')
    for m in re.finditer(r'<g id="\d+"[^>]*transform="translate\(\s*([-\d.]+)\s*[, ]\s*([-\d.]+)\s*\)"[^>]*>(.*?)</g>',
                         blk, re.S):
        x, y, body = float(m.group(1)), float(m.group(2)), m.group(3)
        drawn = [r for r in re.finditer(r'<rect([^>]*)>', body) if 'display="none"' not in r.group(1)]
        if not drawn:
            raise ValueError('node with no drawn box')
        a = drawn[0].group(1)
        w = float(re.search(r'width="([-\d.]+)"', a).group(1))
        h = float(re.search(r'height="([-\d.]+)"', a).group(1))
        out.append((x, y, w, h))
    return out


NUM = r'[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?'


def walk(d):
    """The first and last point of the path data D, through M, L, H, V and relative arcs."""
    toks = re.findall(r'[MmLlHhVvAaZz]|%s' % NUM, d)
    x = y = 0.0
    first = None
    i = 0
    while i < len(toks):
        c = toks[i]
        if c in 'Mm':
            dx, dy = float(toks[i + 1]), float(toks[i + 2])
            x, y = (x + dx, y + dy) if c == 'm' else (dx, dy)
            i += 3
        elif c in 'Ll':
            dx, dy = float(toks[i + 1]), float(toks[i + 2])
            x, y = (x + dx, y + dy) if c == 'l' else (dx, dy)
            i += 3
        elif c in 'Hh':
            v = float(toks[i + 1])
            x = x + v if c == 'h' else v
            i += 2
        elif c in 'Vv':
            v = float(toks[i + 1])
            y = y + v if c == 'v' else v
            i += 2
        elif c in 'Aa':
            dx, dy = float(toks[i + 6]), float(toks[i + 7])
            x, y = (x + dx, y + dy) if c == 'a' else (dx, dy)
            i += 8
        else:
            i += 1
        if first is None:
            first = (x, y)
    return first, (x, y)


def routes(s):
    """The two ends of each edge's drawn route, in document order."""
    out = []
    blk = group(s, 'edges')
    for m in re.finditer(r'<g id="(\d+)"[^>]*>', blk):
        i = m.end()
        depth, j = 1, len(blk)
        for t in re.finditer(r'<g\b[^>]*?(/?)>|</g>', blk[i:]):
            depth += -1 if t.group(0) == '</g>' else (0 if t.group(1) else 1)
            if depth == 0:
                j = i + t.start()
                break
        body = blk[i:j]
        paths = re.findall(r'<path[^>]*?style="([^"]*)"[^>]*?d="([^"]+)"', body)
        drawn = [d for st, d in paths if 'stroke:black' in st.replace(' ', '')]
        if not drawn:
            # 78d30-5.svg draws one route with stroke:none; its halo has the same shape.
            drawn = [d for st, d in paths if d]
        if not drawn:
            raise ValueError('edge with no path')
        out.append(walk(drawn[0]))
    return out


def on_box(p, v, tol=1e-6):
    """True when P lies on the border of node V's box."""
    x, y, w, h = v
    dx, dy = abs(p[0] - x) - w / 2.0, abs(p[1] - y) - h / 2.0
    return dx <= tol and dy <= tol and max(dx, dy) >= -tol


def attach(p, vs):
    """The node whose box P lies on, else the nearest box if none is exact."""
    hit = [i for i, v in enumerate(vs) if on_box(p, v)]
    if len(hit) == 1:
        return hit[0]
    def gap(v):
        x, y, w, h = v
        return max(abs(p[0] - x) - w / 2.0, abs(p[1] - y) - h / 2.0)
    if hit:
        return min(hit, key=lambda i: gap(vs[i]))
    return min(range(len(vs)), key=lambda i: gap(vs[i]))


def read(s):
    """One drawing: its node boxes and its undirected edges as index pairs."""
    vs = nodes(s)
    es = []
    for a, b in routes(s):
        i, j = attach(a, vs), attach(b, vs)
        if i == j:
            raise ValueError('edge with both ends on one node')
        es.append((i, j))
    return vs, es


def iso(a, b):
    """True when the edge lists A and B are isomorphic; backtracking, these graphs are small."""
    na = 1 + max(max(e) for e in a) if a else 0
    nb = 1 + max(max(e) for e in b) if b else 0
    if na != nb or len(a) != len(b):
        return False
    adja = [set() for _ in range(na)]
    adjb = [set() for _ in range(nb)]
    for i, j in a:
        adja[i].add(j); adja[j].add(i)
    for i, j in b:
        adjb[i].add(j); adjb[j].add(i)
    if sorted(map(len, adja)) != sorted(map(len, adjb)):
        return False
    order = sorted(range(na), key=lambda v: -len(adja[v]))
    m, used = {}, [False] * nb

    def go(k):
        if k == len(order):
            return True
        v = order[k]
        for w in range(nb):
            if used[w] or len(adja[v]) != len(adjb[w]):
                continue
            if all((m[u] in adjb[w]) for u in adja[v] if u in m):
                if sum(1 for u in adja[v] if u in m) == sum(1 for u in adjb[w] if u in m.values()):
                    m[v] = w; used[w] = True
                    if go(k + 1):
                        return True
                    del m[v]; used[w] = False
        return False

    return go(0)


def fmt(v):
    """A coordinate as the corpus writes it: an integer where it is one."""
    return '%d' % round(v) if abs(v - round(v)) < 1e-9 else ('%.6g' % v)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else '../data/hola.txt'
    cache = None
    if '--cache' in sys.argv:
        cache = sys.argv[sys.argv.index('--cache') + 1]

    with urllib.request.urlopen(BASE + '/', timeout=60) as r:
        index = r.read().decode('utf-8', 'replace')
    names = sorted(set(re.findall(r'layouts/([0-9a-f]{5}-[1-8]\.svg)', index)))
    if len(names) != 136:
        raise SystemExit('expected 136 layout SVGs, the page lists %d' % len(names))

    refs = []
    for h in REFS:
        _, es = read(fetch(h, cache))
        refs.append(es)

    by_person = defaultdict(dict)
    for name in names:
        person = name.split('-')[0]
        vs, es = read(fetch('layouts/' + name, cache))
        hit = [k for k, r in enumerate(refs, 1) if iso(es, r)]
        if len(hit) != 1:
            raise SystemExit('%s matches %d reference graphs' % (name, len(hit)))
        by_person[person][hit[0]] = (vs, es)

    for person, gs in by_person.items():
        if sorted(gs) != list(range(1, 9)):
            raise SystemExit('%s covers %s, not all eight graphs' % (person, sorted(gs)))

    with open(out, 'w', encoding='utf-8') as f:
        f.write('# HOLA formative study: 17 participants x 8 abstract graphs, drawn by hand\n'
                '# in the study\'s orthogonal layout editor. Kieffer, Dwyer, Marriott, Wybrow,\n'
                '# "HOLA: Human-like Orthogonal Network Layout", TVCG 2016.\n'
                '# Source: %s (SVGs under\n'
                '# %s/layouts/<participant>-<task>.svg),\n'
                '# written by parsers/parse_hola.py. Node position = the node group\'s\n'
                '# translate; node box = the drawn rect\'s width/height; edges = route-path\n'
                '# endpoints matched to node boxes, emitted as chords. Graph number g1..g8 is\n'
                '# the reference graph (h<k>.svg) the drawing is isomorphic to; the task number\n'
                '# in the file name was presentation order. All 136 drawings parsed, none\n'
                '# excluded.\n' % (BASE, BASE))
        for person in sorted(by_person):
            for k in range(1, 9):
                vs, es = by_person[person][k]
                f.write('G %s_g%d %d %d\n' % (person, k, len(vs), len(es)))
                for x, y, w, h in vs:
                    f.write('V %s %s %s %s\n' % (fmt(x), fmt(y), fmt(w), fmt(h)))
                for i, j in es:
                    f.write('U %d %d\n' % (i, j))
    n = sum(len(g) for g in by_person.values())
    print('%s: %d drawings, %d participants' % (out, n, len(by_person)), file=sys.stderr)


if __name__ == '__main__':
    main()
