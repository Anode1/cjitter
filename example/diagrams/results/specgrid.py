#!/usr/bin/env python3
"""The specification grid: verdicts for length, stress and A1 over radius x directions
x L-rule x routes, hand and neato, all corpora. Writes specgrid.csv rows as they finish.
"""
import csv, io, os, statistics, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

H = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data')
STATION = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'station')
TERMS = {'length': '0,0,1,0,0,0,0,0', 'stress': '0,0,0,1,0,0,0,0',
         'A1': '0,0,0,0,0,1,0,0'}

jobs = []
for c in ('hs', 'sbgn', 'bpmn'):
    for layout, fname, routes in (('hand', c + '.txt', 'stored'),
                                  ('hand', c + '_chords.txt', 'chords'),
                                  ('neato', c + '_neato.txt', 'stored')):
        for term, w in TERMS.items():
            for d in ('0.005', '0.01', '0.02', '0.05'):
                for dirs in ('8', '16', '32', '64'):
                    lrefs = ('fit', 'median', 'rsqrt') if term != 'A1' else ('fit',)
                    for lref in lrefs:
                        jobs.append((c, layout, routes, fname, term, w, d, dirs, lref))

def run(j):
    c, layout, routes, fname, term, w, d, dirs, lref = j
    o = subprocess.run([STATION, 'direct', '--corpus', os.path.join(DATA, fname),
                        '--weights', w, '--align', 'a1', '--d', d, '--dirs', dirs,
                        '--L', lref], capture_output=True, text=True, check=True).stdout
    med = statistics.median(float(r['q']) for r in csv.DictReader(io.StringIO(o)))
    return [c, layout, routes, term, d, dirs, lref, '%.3f' % med]

with open(os.path.join(H, 'specgrid.csv'), 'w', buffering=1) as f:
    wtr = csv.writer(f)
    wtr.writerow(['corpus', 'layout', 'routes', 'term', 'd', 'dirs', 'L', 'median_q'])
    with ThreadPoolExecutor(max_workers=2) as ex:
        for row in ex.map(run, jobs):
            wtr.writerow(row)
            print(*row, flush=True)
