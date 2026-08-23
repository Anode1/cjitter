"""Compare parsed/*/ against the pilot JSON of the same name, and report route statistics.

    python3 check_against_pilot.py

Node count, node centres in order, box sizes and the undirected edge set must match the
pilot; the new parsers add direction and waypoints and change nothing else. A new file with
no pilot counterpart is counted apart: the pilot's BPMN run kept 300 of the models that pass
its own filter, the new run keeps every one of them.
"""
import json, os, sys
from collections import Counter

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPORA = [('hs', 'parsed/hs', 'pilot/pr_hs'),
           ('sbgn', 'parsed/sbgn', 'pilot/pr_sbgn'),
           ('bpmn', 'parsed/bpmn', 'pilot/pr_bpmn')]

def diff(new, old):
    out = []
    if new['n'] != old['n']:
        out.append('n %d vs %d' % (new['n'], old['n']))
    else:
        d = max((abs(a[0] - b[0]) + abs(a[1] - b[1]) for a, b in zip(new['xy'], old['xy'])), default=0)
        if d > 1e-6:
            out.append('xy differs by %.3g' % d)
        d = max((abs(a[0] - b[0]) + abs(a[1] - b[1]) for a, b in zip(new['wh'], old['wh'])), default=0)
        if d > 1e-6:
            out.append('wh differs by %.3g' % d)
    a = {(min(x, y), max(x, y)) for x, y in new['e']}
    b = {(min(x, y), max(x, y)) for x, y in old['e']}
    if a != b:
        out.append('edges +%d -%d of %d' % (len(a - b), len(b - a), len(b)))
    if len(new['e']) != len(new['wp']) or len(new['e']) != len(new['directed']) \
       or len(new['e']) != len(new['route']):
        out.append('field lengths disagree')
    return out

for name, nd, od in CORPORA:
    nd, od = os.path.join(BASE, nd), os.path.join(BASE, od)
    files = sorted(os.listdir(nd))
    same = cmpd = extra = 0
    lines = []
    route, ne, nwp, ndir = Counter(), 0, 0, 0
    for f in files:
        g = json.load(open(os.path.join(nd, f)))
        ne += g['m']
        nwp += sum(1 for w in g['wp'] if w)
        ndir += sum(1 for d in g['directed'] if d)
        route.update(g['route'])
        p = os.path.join(od, f)
        if not os.path.exists(p):
            extra += 1
            continue
        cmpd += 1
        d = diff(g, json.load(open(p)))
        if d:
            lines.append('  %s: %s' % (f, '; '.join(d)))
        else:
            same += 1
    print('%s: %d files parsed, %d compared to a pilot file, %d identical, %d differ, '
          '%d with no pilot counterpart' % (name, len(files), cmpd, same, cmpd - same, extra))
    for l in lines:
        print(l)
    print('  edges %d, with an interior waypoint %.3f, directed %.3f' %
          (ne, nwp / ne, ndir / ne))
    print('  route ' + ', '.join('%s %.3f' % (k, v / ne) for k, v in route.most_common()))
