"""SBGN-ML to graph JSON: human-curated Reactome pathway layouts.
Glyphs that are containers (compartment, submap) or annotations are not nodes; arcs whose
endpoints are both real glyphs are edges. Ports are resolved to their owning glyph."""
import re, json, os, sys, glob
SKIP = {'compartment', 'submap', 'annotation', 'tag'}
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
    es = set()
    for m in re.finditer(r'<arc\b([^>]*)>', t):
        s = re.search(r'source="([^"]+)"', m.group(1))
        d = re.search(r'target="([^"]+)"', m.group(1))
        if not s or not d:
            continue
        a, b = owner.get(s.group(1), s.group(1)), owner.get(d.group(1), d.group(1))
        if a in idx and b in idx and a != b:
            es.add((min(idx[a], idx[b]), max(idx[a], idx[b])))
    ks = list(nodes)
    return dict(file=os.path.basename(path), n=len(ks), m=len(es),
                xy=[[nodes[k][0], nodes[k][1]] for k in ks],
                wh=[[nodes[k][2], nodes[k][3]] for k in ks],
                e=sorted(es))
if __name__ == '__main__':
    out = sys.argv[2]; os.makedirs(out, exist_ok=True)
    ok = 0
    for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.sbgn'))):
        try:
            g = parse(f)
        except Exception:
            continue
        if g['n'] >= 10 and g['m'] >= 1:
            json.dump(g, open(os.path.join(out, g['file'] + '.json'), 'w'))
            ok += 1
    print('parsed', ok)
