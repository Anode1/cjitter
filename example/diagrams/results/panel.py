"""The decrease and active columns of the paper's second results table, from the cells
profile.py saves: per term, the median over diagrams of q, of dec (the mean over boxes of
the best decrease a single move finds, as a fraction of the term's value) and the share
of diagrams in which any box has an improving move.

Run from this directory: python3 panel.py > panel.md
"""
import csv, os, statistics as st
HERE = os.path.dirname(os.path.abspath(__file__))
TERMS = ['crossings', 'overlap', 'node-edge', 'length', 'stress', 'orthogonality', 'alignment_A1', 'flow']
print('| corpus | term | q | dec | active |\n|---|---|---|---|---|')
for c in ['hs', 'sbgn', 'bpmnr', 'bpmn']:
    for t in TERMS:
        with open(os.path.join(HERE, 'cells', '%s_hand_%s_alone.csv' % (c, t))) as f:
            rows = list(csv.DictReader(f))
        q = [float(r['q']) for r in rows]; dec = [float(r['dec']) for r in rows]
        print('| %s | %s | %.2f | %.3f | %.0f%% |' % (c, t, st.median(q), st.median(dec), 100 * sum(x < 1 for x in q) / len(q)))
