"""HOLA's 136 drawings under length, stress and A1 at radii 0.01, 0.02 and 0.05 of the
width: the drawings have 4 to 13 boxes, so d is a smaller fraction of a box than in the
15 to 40 band, and this says whether the distance-term zeros depend on that.
Run from this directory: python3 hola_radius.py > hola_radius.md
"""
import csv, io, os, statistics as st, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
STATION = os.path.join(HERE, '..', '..', '..', 'station'); HOLA = os.path.join(HERE, '..', 'data', 'hola.txt')
W = {'length': '0,0,1,0,0,0,0,0', 'stress': '0,0,0,1,0,0,0,0', 'alignment A1': '0,0,0,0,0,1,0,0'}
print('| term | d = 0.01 | 0.02 | 0.05 |\n|---|---|---|---|')
for t, w in W.items():
    row = []
    for d in ('0.01', '0.02', '0.05'):
        o = subprocess.run([STATION, 'direct', '--corpus', HOLA, '--weights', w, '--align', 'a1', '--d', d], capture_output=True, text=True, check=True).stdout
        row.append('%.2f' % st.median(float(r['q']) for r in csv.DictReader(io.StringIO(o))))
    print('| %s | %s |' % (t, ' | '.join(row)))
