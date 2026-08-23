"""Write the text corpora station reads, from the parsers' JSON in ../pilot/pr_*/.

    python3 make_corpus.py ../pilot/pr_hs hs.txt [--tool neato hs_neato.txt]... [--exclude WP5037 WP3391]

Inclusion: largest connected component of 15 to 40 nodes, at least one edge. The component
is taken HERE, whatever the JSON says: the SBGN and BPMN parsers of the pilot wrote whole
diagrams and called them components, and 126 of 180 Reactome and 68 of 161 BPMN graphs in
the pilot's size band were unions of fragments. Raw coordinates are written as they are;
station rescales. --tool writes the same graphs with the same box sizes laid out by a
graphviz tool, the control a hand layout is read against; `station check` confirms the
graphs match. Tools: neato (stress, -Gstart=1 so the file reproduces, boxes left where they
overlap), prism (neato followed by its overlap removal, -Goverlap=false), dot (layered).
"""
import json, glob, os, subprocess, sys

def lcc(g):
    """The largest connected component, relabelled 0..k-1; edges deduplicated."""
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
    e = sorted({(ix[min(a, b)], ix[max(a, b)]) for a, b in g['e'] if a in ix and b in ix and a != b})
    return {'n': len(ids), 'm': len(e), 'xy': [g['xy'][v] for v in ids],
            'wh': [g['wh'][v] for v in ids], 'e': [list(x) for x in e], 'file': g['file'],
            'whole_n': g['n']}

def load(d, lo, hi, exclude):
    out, trimmed = [], 0
    for fn in sorted(glob.glob(os.path.join(d, '*.json'))):
        g = json.load(open(fn))
        if any(x in g['file'] for x in exclude): continue
        g = lcc(g)
        if not (lo <= g['n'] <= hi) or g['m'] < 1: continue
        if g['n'] < g['whole_n']: trimmed += 1
        g['id'] = os.path.basename(fn)[:-5].replace(' ', '_')
        out.append(g)
    print("%s: %d graphs in the band, %d of them the component of a larger diagram" % (d, len(out), trimmed))
    return out

def write(f, g, xy):
    f.write("G %s %d %d\n" % (g['id'], g['n'], g['m']))
    for (x, y), (w, h) in zip(xy, g['wh']):
        f.write("V %.6f %.6f %.6f %.6f\n" % (x, y, w, h))
    for a, b in g['e']:
        f.write("E %d %d\n" % (a, b))

TOOLS = {'neato': ['neato', '-Gstart=1'],
         'prism': ['neato', '-Gstart=1', '-Goverlap=false'],
         'dot':   ['dot']}

def layout(g, tool):
    dot = ["graph G { node [shape=box];"]
    for i, (w, h) in enumerate(g['wh']):
        dot.append(" n%d [width=%.4f,height=%.4f,fixedsize=true];" % (i, w / 72, h / 72))
    for a, b in g['e']:
        dot.append(" n%d -- n%d;" % (a, b))
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
    tools = []; exclude = []
    args = sys.argv[3:]
    while args:
        if args[0] == '--tool' and args[1] in TOOLS: tools.append((args[1], args[2])); args = args[3:]
        elif args[0] == '--exclude': exclude = args[1:]; args = []
        else: sys.exit("unknown argument " + args[0])
    G = load(src, 15, 40, exclude)
    with open(dst, 'w') as f:
        f.write("# %d graphs of 15 to 40 nodes from %s\n" % (len(G), src))
        for g in G: write(f, g, g['xy'])
    print("%s: %d graphs" % (dst, len(G)))
    for tool, out in tools:
        n = 0
        with open(out, 'w') as f:
            f.write("# the graphs of %s laid out by %s, same box sizes\n" % (os.path.basename(dst), " ".join(TOOLS[tool])))
            for g in G:
                xy = layout(g, tool)
                if xy is None: sys.exit("%s: %s gave no layout for %s" % (out, tool, g['id']))
                write(f, g, xy); n += 1
        print("%s: %d graphs" % (out, n))
