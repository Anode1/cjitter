"""Exact coordinate ties in the corpora as measured: the share of boxes sharing an exact x
with another box of the same diagram, an exact y, and either. Uniform rescaling keeps
equal coordinates equal, so the corpus files carry the ties of the raw files.

Run from this directory: python3 ties.py > ties.md
"""
import collections, os
HERE = os.path.dirname(os.path.abspath(__file__))
print('| corpus | boxes | exact x | exact y | either |\n|---|---|---|---|---|')
for stem in ['hs', 'sbgn', 'bpmn', 'bpmnr']:
    n = sx = sy = se = 0
    cur = []
    def flush():
        global n, sx, sy, se
        xs = collections.Counter(x for x, y in cur); ys = collections.Counter(y for x, y in cur)
        for x, y in cur:
            n += 1; ax = xs[x] > 1; ay = ys[y] > 1
            sx += ax; sy += ay; se += (ax or ay)
    for line in open(os.path.join(HERE, '..', 'data', stem + '.txt')):
        if line.startswith('G '): flush(); cur = []
        elif line.startswith('V '):
            p = line.split(); cur.append((p[1], p[2]))
    flush()
    print('| %s | %d | %.2f | %.2f | %.2f |' % (stem, n, sx / n, sy / n, se / n))
