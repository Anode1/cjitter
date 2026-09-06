"""The chance level of the gridiness value: the median over diagrams of the share of
boxes in an alignment of three or more (1 minus the gridiness term, tolerance 0.005) on
nulls.py's uniform random layouts, on align_controls.py's grid-snapped random layouts
(pitch 1, seed 1), and on the hand layouts for comparison.
Run from this directory: python3 gridnull.py > gridnull.md
"""
import csv, io, os, statistics as st, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE); sys.argv = ['x']
nsrc = open(os.path.join(HERE, 'nulls.py')).read().split('tmp = tempfile.mkdtemp')[0]
nn = {'__file__': os.path.join(HERE, 'nulls.py')}; exec(nsrc, nn)
from align_controls import transform
STATION, DATA = nn['STATION'], nn['DATA']
def share(path):
    o = subprocess.run([STATION, 'direct', '--corpus', path, '--weights', '0,0,0,0,0,1,0,0', '--align', 'grid'], capture_output=True, text=True, check=True).stdout
    return 1 - st.median(float(r['E']) for r in csv.DictReader(io.StringIO(o)))
tmp = tempfile.mkdtemp(prefix='gridnull_')
print('| corpus | hand | uniform random | grid-snapped random |\n|---|---|---|---|')
for c in ['hs', 'sbgn', 'bpmnr']:
    rnd = os.path.join(tmp, c + '_random.txt'); nn['write_null'](os.path.join(DATA, c + '_chords.txt'), rnd, 'random')
    snap = os.path.join(tmp, c + '_snapped.txt'); transform(os.path.join(DATA, c + '.txt'), snap, 'snapped', 1.0, 1)
    print('| %s | %.2f | %.2f | %.2f |' % (c, share(os.path.join(DATA, c + '.txt')), share(rnd), share(snap)))
