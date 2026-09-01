"""A Graphviz dot file with positions to the corpus format, so station can measure it.

    python3 dot2corpus.py drawing.gv corpus.txt
    ../../station direct --corpus corpus.txt --weights 0,0,0,0,0,1,0,0 --align a1

Reads node statements carrying pos="x,y" (points), width and height in inches when
present (0 otherwise), and -- or -> edges; -> is written directed. Multiple graphs in
one file become multiple corpus entries. Edge routes are not read; every edge is the
chord, the sensitivity of the paper. This is the adapter the recommendation needs: any
tool that can emit dot with coordinates can report the fraction of boxes an energy
holds at its layout.
"""
import re, sys

def unwrap(text):
    """Graphviz wraps attribute lists across lines; join them so one statement is one line."""
    out, depth = [], 0
    for ch in text:
        if ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
        out.append(' ' if ch == '\n' and depth > 0 else ch)
    return ''.join(out)

def parse(text):
    text = unwrap(text)
    graphs, idx, xy, wh, edges, directed = [], {}, {}, {}, [], []
    def flush():
        if xy:
            graphs.append((dict(idx), dict(xy), dict(wh), list(edges), list(directed)))
        idx.clear(); xy.clear(); wh.clear(); edges.clear(); directed.clear()
    for line in text.splitlines():
        if re.match(r'\s*(strict\s+)?(di)?graph\b', line):
            flush()
            continue
        m = re.match(r'\s*"?([\w.:-]+)"?\s*\[(.*)\]', line)
        if m and 'pos=' in m.group(2) and '--' not in line and '->' not in line:
            attrs = m.group(2)
            p = re.search(r'pos="([-\d.e]+),([-\d.e]+)', attrs)
            if not p:
                continue
            n = idx.setdefault(m.group(1), len(idx))
            xy[n] = (float(p.group(1)), float(p.group(2)))
            w = re.search(r'width="?([\d.]+)', attrs)
            h = re.search(r'height="?([\d.]+)', attrs)
            wh[n] = (72 * float(w.group(1)) if w else 0.0,
                     72 * float(h.group(1)) if h else 0.0)
            continue
        m = re.match(r'\s*"?([\w.:-]+)"?\s*(--|->)\s*"?([\w.:-]+)"?', line)
        if m and m.group(1) in idx and m.group(3) in idx:
            edges.append((idx[m.group(1)], idx[m.group(3)]))
            directed.append(m.group(2) == '->')
    flush()
    return graphs

src, dst = sys.argv[1], sys.argv[2]
graphs = parse(open(src, errors='replace').read())
with open(dst, 'w') as f:
    f.write("# %d graphs from %s via dot2corpus.py, chord edges\n" % (len(graphs), src))
    for k, (idx, xy, wh, edges, directed) in enumerate(graphs):
        seen, dedup = set(), []
        for (a, b), d in zip(edges, directed):
            key = (min(a, b), max(a, b))
            if a == b or key in seen:
                continue
            seen.add(key)
            dedup.append((a, b, d))
        f.write("G %s-%d %d %d\n" % (src.rsplit('/', 1)[-1], k, len(xy), len(dedup)))
        for i in range(len(xy)):
            f.write("V %f %f %f %f\n" % (xy[i][0], xy[i][1], wh[i][0], wh[i][1]))
        for a, b, d in dedup:
            f.write("%s %d %d\n" % ("E" if d else "U", a, b))
print("%s: %d graphs" % (dst, len(graphs)))
