"""The stress value beside calib.py's removable fraction: the median stress of neato's
converged layouts after every box is displaced by eps of the width (calib.py's perturb,
its seeds), of the hand layouts, and of nulls.py's uniform random layouts. The removable
fraction is not monotone in eps, so the value is the axis on which the hand layouts are
placed. Run from this directory: python3 calib_values.py > calib_values.md
"""
import csv, io, os, statistics as st, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); sys.argv = ['x']
src = open(os.path.join(HERE, 'calib.py')).read().split('jobs = []')[0]
ns = {'__file__': os.path.join(HERE, 'calib.py')}; exec(src, ns)
perturb, DATA, STATION = ns['perturb'], ns['DATA'], ns['STATION']
nsrc = open(os.path.join(HERE, 'nulls.py')).read().split('tmp = tempfile.mkdtemp')[0]
nn = {'__file__': os.path.join(HERE, 'nulls.py')}; exec(nsrc, nn)
def run(path):
    o = subprocess.run([STATION, 'direct', '--corpus', path, '--weights', '0,0,0,1,0,0,0,0'], capture_output=True, text=True, check=True).stdout
    rows = list(csv.DictReader(io.StringIO(o)))
    return st.median(float(r['E']) for r in rows), st.median(float(r['q']) for r in rows)
def medE(path): return run(path)[0]
def medQ(path): return run(path)[1]
tmp = tempfile.mkdtemp(prefix='calib_values_')
EPS = [0.005, 0.01, 0.02, 0.05, 0.10]
print('| corpus | reading | neato | ' + ' | '.join('eps %g' % e for e in EPS) + ' | hand | random |\n|' + '---|' * (len(EPS) + 5))
for ci, c in enumerate(['hs', 'sbgn', 'bpmnr']):
    files = [os.path.join(DATA, c + '_neato.txt')]
    for ei, eps in enumerate(EPS):
        dst = os.path.join(tmp, '%s_e%g.txt' % (c, eps)); perturb(os.path.join(DATA, c + '_neato.txt'), dst, eps, 100 * ci + ei); files.append(dst)
    files.append(os.path.join(DATA, c + '.txt'))
    rnd = os.path.join(tmp, c + '_random.txt'); nn['write_null'](os.path.join(DATA, c + '_chords.txt'), rnd, 'random'); files.append(rnd)
    res = [run(f) for f in files]
    print('| %s | stress value | %s |' % (c, ' | '.join('%.3f' % e for e, q in res)))
    print('| %s | q at d = 0.02 | %s |' % (c, ' | '.join('%.2f' % q for e, q in res)))
