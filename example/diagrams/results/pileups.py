"""Boundary pileups behind the degenerate bootstrap intervals, from the cells profile.py
saves: the share of hand diagrams at exactly q = 0 under length and stress, and of neato
layouts at exactly q = 1 under stress. Run from this directory: python3 pileups.py > pileups.md
"""
import csv, os
HERE = os.path.dirname(os.path.abspath(__file__))
def frac(name, val):
    with open(os.path.join(HERE, 'cells', name)) as f:
        q = [float(r['q']) for r in csv.DictReader(f)]
    return sum(abs(x - val) < 1e-12 for x in q) / len(q), len(q)
print('| cell | at the boundary |\n| --- | --- |')
for c in ['hs', 'sbgn', 'bpmnr', 'bpmn']:
    for t in ['length', 'stress']:
        fr, n = frac('%s_hand_%s_alone.csv' % (c, t), 0.0); print('| %s hand %s | %.2f of %d diagrams at exactly 0.00 |' % (c, t, fr, n))
    fr, n = frac('%s_neato_stress_alone.csv' % c, 1.0); print('| %s neato stress | %.2f of %d diagrams at exactly 1.00 |' % (c, fr, n))
