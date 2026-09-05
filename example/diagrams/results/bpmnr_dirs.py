"""Median q by direction count on the random BPMN 300, the direction-count block of
robustness.md for that sample. Run from this directory: python3 bpmnr_dirs.py > bpmnr_dirs.md
"""
import csv, io, os, statistics as st, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
STATION = os.path.join(HERE, '..', '..', '..', 'station')
W = {'length': '0,0,1,0,0,0,0,0', 'stress': '0,0,0,1,0,0,0,0', 'alignment_A1': '0,0,0,0,0,1,0,0'}
print('| corpus | term | 8 | 16 | 32 | 64 |\n| --- | --- | --- | --- | --- | --- |')
for layout in ('hand', 'neato'):
    f = os.path.join(HERE, '..', 'data', 'bpmnr' + ('' if layout == 'hand' else '_neato') + '.txt')
    for t, w in W.items():
        row = []
        for d in (8, 16, 32, 64):
            o = subprocess.run([STATION, 'direct', '--corpus', f, '--weights', w, '--align', 'a1', '--dirs', str(d)],
                               capture_output=True, text=True, check=True).stdout
            row.append('%.2f' % st.median(float(r['q']) for r in csv.DictReader(io.StringIO(o))))
        print('| bpmnr %s | %s | %s |' % (layout, t, ' | '.join(row)))
