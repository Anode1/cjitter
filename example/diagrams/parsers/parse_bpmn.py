"""BPMN Academic Initiative (Signavio/Oryx JSON) to graph JSON with edge direction and drawn
waypoints.

Node rules are those of pilot/parse_bpmn.py: a shape whose stencil is neither a connector nor
a container is a node, its bounds accumulate down the childShapes tree, and a node's outgoing
list reaches either another node directly or a connector shape that reaches nodes. Node order
is the order the tree walk meets the shapes, edge order is the sorted index pair. Files under
10 nodes or without an edge are dropped.

Direction: the drawn direction is the outgoing direction. SequenceFlow, MessageFlow,
Association_Unidirectional and DataAssociation are directed; Association_Undirected,
Association_Bidirectional and ConversationLink are not.

Waypoints: the dockers of the connector shape between the first and the last. Oryx stores the
first and the last docker as an offset inside the source and the target shape, and the
interior dockers as canvas coordinates. Verified on the 2253 connectors that carry an
interior docker in the 300 files the pilot kept: rebuilding the route as source.upperLeft + docker[0], the interior
dockers as they stand, target.upperLeft + docker[-1] makes 85.0% of the routes exactly
axis-aligned at every segment, against 23.4% when the endpoints are taken at the shape
centres and 0.0% when the interior dockers are read relative to the connector's own bounds;
and every interior docker of every such connector falls inside that connector's own bounds,
which are canvas coordinates. The first docker is the source shape's centre offset on 90% of
them.

The corpus holds EPC, Petri net and organigram models beside BPMN. Their flow elements
(ControlFlow, Relation, Arc) carry stencils outside the connector set, so the pilot rules make
them nodes and the model is read as a bipartite graph. Such an edge is a bare reference with
no drawn polyline of its own: route "chord", directed false, and the stencil pair is kept in
"kind".

"ends" carries the two attachment points the file draws on the node boxes, rebuilt from the
first and the last docker; the six contract fields ignore them, and they are null when the
connector stores no dockers or the edge is a bare reference. With them 85.6% of the routes
that have a waypoint are exactly orthogonal, against 28.8% when the route is closed at the
node centres.
"""
import json, os, sys, glob

EDGE = {'SequenceFlow', 'MessageFlow', 'Association_Undirected', 'Association_Unidirectional',
        'Association_Bidirectional', 'ConversationLink', 'DataAssociation'}
DIRECTED = {'SequenceFlow', 'MessageFlow', 'Association_Unidirectional', 'DataAssociation'}
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
            conns.append((rid, out, s.get('dockers') or [], st))
        elif st not in CONTAINER and rid:
            nodes[rid] = ((x0 + x1) / 2, (y0 + y1) / 2, abs(x1 - x0), abs(y1 - y0), out, st)
        collect(s, x0, y0, nodes, conns)

def ends(src, tgt, dk):
    """The drawn attachment points, the first and last docker read inside their own shape."""
    if not dk:
        return None
    return [[src[0] - src[2] / 2 + dk[0][0], src[1] - src[3] / 2 + dk[0][1]],
            [tgt[0] - tgt[2] / 2 + dk[1][0], tgt[1] - tgt[3] / 2 + dk[1][1]]]

def parse(path):
    d = json.load(open(path, encoding='utf-8', errors='replace'))
    nodes, conns = {}, []
    collect(d, 0, 0, nodes, conns)
    idx = {k: i for i, k in enumerate(nodes)}
    src_of, geom = {}, {}
    for rid, out, dk, st in conns:
        geom[rid] = ([[p.get('x', 0), p.get('y', 0)] for p in dk[1:-1]], st,
                     [[p.get('x', 0), p.get('y', 0)] for p in (dk[:1] + dk[-1:])] if len(dk) >= 2 else None)
        for t in out:
            if t in idx:
                src_of.setdefault(rid, []).append(idx[t])
    seen, nk = {}, list(nodes)
    for k, (x, y, w, h, out, st) in nodes.items():
        a = idx[k]
        for t in out:
            if t in idx:
                b = idx[t]
                if a != b:
                    seen.setdefault((min(a, b), max(a, b)),
                                    (a, b, False, [], 'chord', st + '>' + nodes[t][5], None))
            elif t in src_of:
                wp, cst, dk = geom[t]
                for b in src_of[t]:
                    if a != b:
                        seen.setdefault((min(a, b), max(a, b)),
                                        (a, b, cst in DIRECTED, wp,
                                         'stored' if wp else 'chord', cst,
                                         ends(nodes[k], nodes[nk[b]], dk)))
    ks = sorted(seen)
    return dict(file=os.path.basename(path), n=len(nk), m=len(ks),
                xy=[[nodes[k][0], nodes[k][1]] for k in nk],
                wh=[[nodes[k][2], nodes[k][3]] for k in nk],
                e=[[seen[k][0], seen[k][1]] for k in ks],
                directed=[seen[k][2] for k in ks],
                wp=[seen[k][3] for k in ks],
                route=[seen[k][4] for k in ks],
                ids=nk, kind=[seen[k][5] for k in ks], ends=[seen[k][6] for k in ks])

if __name__ == '__main__':
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    ok = 0
    for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.json'))):
        if f.endswith('.meta.json'):
            continue
        try:
            g = parse(f)
        except Exception:
            continue
        if g['n'] >= 10 and g['m'] >= 1:
            json.dump(g, open(os.path.join(out, g['file'] + '.json'), 'w'), separators=(',', ':'))
            ok += 1
    print('parsed', ok)
