"""Write the text corpora station reads, from the parsers' JSON in ../parsed/<corpus>/.

    python3 make_corpus.py ../parsed/hs hs.txt [--tool neato hs_neato.txt]... [--chords]
                           [--bpmn] [--limit K] [--exclude WP5037 WP3391]

Inclusion: largest connected component of 15 to 40 nodes, at least one edge. The component
is taken HERE, whatever the JSON says: the SBGN and BPMN parsers of the pilot wrote whole
diagrams and called them components, and 126 of 180 Reactome and 68 of 161 BPMN graphs in
the pilot's size band were unions of fragments. --bpmn keeps only models whose every edge
is a BPMN flow or association (the Academic Initiative archive is two fifths EPC, Petri net
and UML models, whose flows the node rules turn into degree-2 nodes); --limit K keeps the
first K included models in file-name order. Raw coordinates are written as they are; station
rescales. An edge is written `E a b` in its drawn direction or `U a b` when the format gives
it none, followed by its drawn route: the attachment point on a's box (the centre where the
format stores none), the interior waypoints, the attachment point on b's box; --chords drops
the route, the sensitivity. Per corpus the fraction of edges with waypoints, by route label,
and directed is printed, which the paper reports.

--tool writes the same graphs with the same box sizes laid out by a graphviz tool, the
control a hand layout is read against; `station check` confirms the graphs match. Tools:
neato (stress, -Gstart=1 so the file reproduces, boxes left where they overlap), prism (neato
followed by its overlap removal, -Goverlap=false), dot (layered, directed edges drawn as
such). ELK is elk_layout.py. A control carries no waypoints.
"""
import json, glob, os, subprocess, sys
from collections import Counter

def lcc(g):
    """The largest connected component, relabelled 0..k-1; edges deduplicated as undirected
    pairs, the first listing of a pair keeping its direction and route."""
    adj = [set() for _ in range(g['n'])]
    for a, b in g['e']:
        if a != b: adj[a].add(b); adj[b].add(a)
    seen, best = set(), []
    for s in range(g['n']):
        if s in seen: continue
        stack, comp = [s], []
        seen.add(s)
        while stack:
            u = stack.pop(); comp.append(u)
            for v in adj[u]:
                if v not in seen: seen.add(v); stack.append(v)
        if len(comp) > len(best): best = comp
    ids = sorted(best); ix = {v: i for i, v in enumerate(ids)}
    directed = g.get('directed') or [False] * len(g['e'])
    wp = g.get('wp') or [[] for _ in g['e']]
    route = g.get('route') or ['chord'] * len(g['e'])
    ends = g.get('ends') or [None] * len(g['e'])
    edges, taken = [], set()
    for k, (a, b) in enumerate(g['e']):
        if a == b or a not in ix or b not in ix: continue
        key = (min(a, b), max(a, b))
        if key in taken: continue
        taken.add(key)
        pts = []
        if wp[k] or (ends[k] and any(ends[k])):
            e0 = (ends[k] or [None, None])[0] or g['xy'][a]
            e1 = (ends[k] or [None, None])[1] or g['xy'][b]
            pts = [e0] + list(wp[k]) + [e1]
        edges.append((ix[a], ix[b], directed[k], pts, route[k]))
    edges.sort(key=lambda e: (min(e[0], e[1]), max(e[0], e[1])))
    return {'n': len(ids), 'm': len(edges), 'xy': [g['xy'][v] for v in ids],
            'wh': [g['wh'][v] for v in ids], 'edges': edges, 'file': g['file'],
            'whole_n': g['n']}

BPMN_KINDS = {'SequenceFlow', 'MessageFlow', 'Association_Unidirectional', 'Association_Undirected',
              'Association_Bidirectional', 'DataAssociation', 'ConversationLink'}

def load(d, lo, hi, exclude, bpmn, limit):
    out, trimmed = [], 0
    for fn in sorted(glob.glob(os.path.join(d, '*.json'))):
        if limit and len(out) >= limit: break
        g = json.load(open(fn))
        if any(x in g['file'] for x in exclude): continue
        if bpmn and (not g.get('kind') or any(k not in BPMN_KINDS for k in g['kind'])): continue
        g = lcc(g)
        if not (lo <= g['n'] <= hi) or g['m'] < 1: continue
        if g['n'] < g['whole_n']: trimmed += 1
        g['id'] = os.path.basename(fn)[:-5].replace(' ', '_')
        out.append(g)
    print("%s: %d graphs in the band, %d of them the component of a larger diagram" % (d, len(out), trimmed))
    return out

def write(f, g, xy, chords):
    f.write("G %s %d %d\n" % (g['id'], g['n'], g['m']))
    for (x, y), (w, h) in zip(xy, g['wh']):
        f.write("V %.6f %.6f %.6f %.6f\n" % (x, y, w, h))
    for a, b, directed, wp, route in g['edges']:
        f.write("%s %d %d" % ('E' if directed else 'U', a, b))
        if not chords:
            for x, y in wp: f.write(" %.6f %.6f" % (x, y))
        f.write("\n")

def report(G):
    m = sum(g['m'] for g in G)
    routes = Counter(e[4] for g in G for e in g['edges'])
    with_wp = sum(1 for g in G for e in g['edges'] if len(e[3]) > 2)
    directed = sum(1 for g in G for e in g['edges'] if e[2])
    print("edges %d: with waypoints %.3f, directed %.3f, by route %s" % (
        m, with_wp / m, directed / m, ", ".join("%s %.3f" % (k, v / m) for k, v in sorted(routes.items()))))

TOOLS = {'neato': ['neato', '-Gstart=1'],
         'prism': ['neato', '-Gstart=1', '-Goverlap=false'],
         'dot':   ['dot']}

def layout(g, tool):
    dot = ["digraph G { node [shape=box];"]
    for i, (w, h) in enumerate(g['wh']):
        dot.append(" n%d [width=%.4f,height=%.4f,fixedsize=true];" % (i, w / 72, h / 72))
    for a, b, directed, wp, route in g['edges']:
        dot.append(" n%d -> n%d%s;" % (a, b, "" if directed else " [dir=none]"))
    dot.append("}")
    o = subprocess.run(TOOLS[tool] + ['-Tplain'], input="\n".join(dot),
                       capture_output=True, text=True).stdout
    xy = [None] * g['n']
    for l in o.split("\n"):
        v = l.split()
        if v and v[0] == 'node':
            xy[int(v[1][1:])] = (float(v[2]) * 72, float(v[3]) * 72)
    return None if any(p is None for p in xy) else xy

if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    tools, exclude, chords, bpmn, limit, lo, hi = [], [], False, False, 0, 15, 40
    args = sys.argv[3:]
    while args:
        if args[0] == '--tool' and args[1] in TOOLS: tools.append((args[1], args[2])); args = args[3:]
        elif args[0] == '--chords': chords = True; args = args[1:]
        elif args[0] == '--bpmn': bpmn = True; args = args[1:]
        elif args[0] == '--limit': limit = int(args[1]); args = args[2:]
        elif args[0] == '--band': lo, hi = int(args[1]), int(args[2]); args = args[3:]
        elif args[0] == '--exclude': exclude = args[1:]; args = []
        else: sys.exit("unknown argument " + args[0])
    G = load(src, lo, hi, exclude, bpmn, limit)
    report(G)
    with open(dst, 'w') as f:
        f.write("# %d graphs of %d to %d nodes from %s%s\n" % (len(G), lo, hi, src, ", chords only" if chords else ""))
        for g in G: write(f, g, g['xy'], chords)
    print("%s: %d graphs" % (dst, len(G)))
    for tool, out in tools:
        n = 0
        with open(out, 'w') as f:
            f.write("# the graphs of %s laid out by %s, same box sizes\n" % (os.path.basename(dst), " ".join(TOOLS[tool])))
            for g in G:
                xy = layout(g, tool)
                if xy is None: sys.exit("%s: %s gave no layout for %s" % (out, tool, g['id']))
                write(f, g, xy, True); n += 1
        print("%s: %d graphs" % (out, n))
