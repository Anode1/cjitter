"""The stored routes: per corpus, the share of edges that carry an interior waypoint, and
of those, the share axis-aligned at every segment (consecutive route points sharing an
x or a y to 1e-6). Reads the corpus files, whose E lines carry the route points.
Run from this directory: python3 routes.py > routes.md
"""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
print('| corpus | edges | with a waypoint | of those, axis-aligned at every segment |\n|---|---|---|---|')
for stem in ['hs', 'sbgn', 'bpmnr', 'bpmn']:
    m = wp = ortho = 0
    for line in open(os.path.join(HERE, '..', 'data', stem + '.txt')):
        if not line.startswith('E '): continue
        p = line.split(); m += 1
        pts = [(float(p[i]), float(p[i + 1])) for i in range(3, len(p) - 1, 2)]
        if len(pts) <= 2: continue
        wp += 1
        if all(abs(a[0] - b[0]) < 1e-6 or abs(a[1] - b[1]) < 1e-6 for a, b in zip(pts, pts[1:])): ortho += 1
    print('| %s | %d | %.3f | %.3f |' % (stem, m, wp / m, ortho / wp if wp else 0))
