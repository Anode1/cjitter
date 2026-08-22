"""BPMN (Signavio/Oryx JSON) to graph JSON. Shapes with a stencil that is not a connector are
nodes, positioned by their bounds; connector shapes become edges between the shapes they link.
Bounds are relative to the parent shape, so positions accumulate down the childShapes tree."""
import json, os, sys, glob
EDGE = {'SequenceFlow', 'MessageFlow', 'Association_Undirected', 'Association_Unidirectional',
        'Association_Bidirectional', 'ConversationLink', 'DataAssociation'}
CONTAINER = {'Pool', 'Lane', 'BPMNDiagram', 'CollapsedPool'}
def collect(shape, ox, oy, nodes, conns):
    for s in shape.get('childShapes', []):
        st = (s.get('stencil') or {}).get('id', '')
        b = s.get('bounds') or {}
        ul, lr = b.get('upperLeft') or {}, b.get('lowerRight') or {}
        x0, y0 = ox + ul.get('x', 0), oy + ul.get('y', 0)
        x1, y1 = ox + lr.get('x', 0), oy + lr.get('y', 0)
        rid = s.get('resourceId')
        out = [o.get('resourceId') for o in s.get('outgoing', []) if o.get('resourceId')]
        if st in EDGE:
            conns.append((rid, out))
        elif st not in CONTAINER and rid:
            nodes[rid] = ((x0 + x1) / 2, (y0 + y1) / 2, abs(x1 - x0), abs(y1 - y0), out)
        collect(s, x0, y0, nodes, conns)
def parse(path):
    d = json.load(open(path, encoding='utf-8', errors='replace'))
    nodes, conns = {}, []
    collect(d, 0, 0, nodes, conns)
    idx = {k: i for i, k in enumerate(nodes)}
    es = set()
    # node -> node via a connector shape
    src_of = {}
    for rid, out in conns:
        for t in out:
            if t in idx:
                src_of.setdefault(rid, []).append(idx[t])
    for k, (x, y, w, h, out) in nodes.items():
        for t in out:
            if t in idx:
                a, b = idx[k], idx[t]
                if a != b: es.add((min(a, b), max(a, b)))
            elif t in src_of:
                for b in src_of[t]:
                    a = idx[k]
                    if a != b: es.add((min(a, b), max(a, b)))
    ks = list(nodes)
    return dict(file=os.path.basename(path), n=len(ks), m=len(es),
                xy=[[nodes[k][0], nodes[k][1]] for k in ks],
                wh=[[nodes[k][2], nodes[k][3]] for k in ks], e=sorted(es))
if __name__ == '__main__':
    out = sys.argv[2]; os.makedirs(out, exist_ok=True); ok = 0
    for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.json'))):
        if f.endswith('.meta.json'): continue
        try: g = parse(f)
        except Exception: continue
        if g['n'] >= 10 and g['m'] >= 1:
            json.dump(g, open(os.path.join(out, g['file'] + '.json'), 'w')); ok += 1
    print('parsed', ok)
