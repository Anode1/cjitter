"""The alignment chance line at 64 directions: the uniform random layouts of nulls.py
under A1 alone at 16 and 64 directions, so the hand layouts' direction-count series
(robustness.md, bpmnr_dirs.md) has its comparator at every count.
Run from this directory: python3 null64.py > null64.md
"""
import csv, io, os, statistics as st, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
sys.argv = ['x']
import importlib.util
spec = importlib.util.spec_from_file_location('nullsmod', os.path.join(HERE, 'nulls.py'))
# nulls.py runs its battery at import; take only write_null by exec of its definitions
src = open(os.path.join(HERE, 'nulls.py')).read().split('tmp = tempfile.mkdtemp')[0]
ns = {'__file__': os.path.join(HERE, 'nulls.py')}; exec(src, ns)
STATION = ns['STATION']; DATA = ns['DATA']
tmp = tempfile.mkdtemp(prefix='null64_')
print('| corpus | layout | 16 directions | 64 directions |\n|---|---|---|---|')
for c in ('hs', 'sbgn', 'bpmnr'):
    out = os.path.join(tmp, c + '_random.txt')
    ns['write_null'](os.path.join(DATA, c + '_chords.txt'), out, 'random')
    row = []
    for d in (16, 64):
        o = subprocess.run([STATION, 'direct', '--corpus', out, '--weights', '0,0,0,0,0,1,0,0', '--align', 'a1', '--dirs', str(d)],
                           capture_output=True, text=True, check=True).stdout
        row.append('%.2f' % st.median(float(r['q']) for r in csv.DictReader(io.StringIO(o))))
    print('| %s | random | %s |' % (c, ' | '.join(row)))
