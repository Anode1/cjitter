"""The profile: which criteria hold a hand layout, and which hold a tool's layout of the same
graph. Runs station's directional test over the corpora for the declared energies and
prints the median q per cell (1 = every node held, 0 = none).

    python3 profile.py [--d 0.02] [--dirs 16] [--station ../../station] [--md]
"""
import csv, io, os, statistics as st, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TERMS = ['crossings', 'overlap', 'length', 'stress', 'orthogonality', 'alignment', 'node-edge']
CORPORA = [('WikiPathways', 'hs'), ('Reactome', 'sbgn'), ('BPMN', 'bpmn')]
# name, weights C,O,L,S,R,A,N, alignment definition
ENERGIES = [
    ('crossings alone',            '1,0,0,0,0,0,0', 'a1'),
    ('overlap alone',              '0,1,0,0,0,0,0', 'a1'),
    ('length alone',               '0,0,1,0,0,0,0', 'a1'),
    ('stress alone',               '0,0,0,1,0,0,0', 'a1'),
    ('orthogonality alone',        '0,0,0,0,1,0,0', 'a1'),
    ('alignment A1 alone',         '0,0,0,0,0,1,0', 'a1'),
    ('alignment A3 alone',         '0,0,0,0,0,1,0', 'a3'),
    ('gridiness alone',            '0,0,0,0,0,1,0', 'grid'),
    ('node-edge alone',            '0,0,0,0,0,0,1', 'a1'),
    ('C+O',                        '1,1,0,0,0,0,0', 'a1'),
    ('C+O+L (1,1,1)',              '1,1,1,0,0,0,0', 'a1'),
    ('C+O+S',                      '1,1,0,1,0,0,0', 'a1'),
    ('C+O+R',                      '1,1,0,0,1,0,0', 'a1'),
    ('C+O+A1',                     '1,1,0,0,0,1,0', 'a1'),
    ('C+O+A3',                     '1,1,0,0,0,1,0', 'a3'),
    ('C+O+grid',                   '1,1,0,0,0,1,0', 'grid'),
    ('C+O+N',                      '1,1,0,0,0,0,1', 'a1'),
]

def run(station, corpus, weights, align, d, dirs):
    o = subprocess.run([station, 'direct', '--corpus', corpus, '--weights', weights,
                        '--align', align, '--d', d, '--dirs', dirs],
                       capture_output=True, text=True, check=True).stdout
    rows = list(csv.DictReader(io.StringIO(o)))
    term = [t for t, x in zip(TERMS, weights.split(',')) if x != '0']
    value = st.median(float(r[term[0]]) for r in rows) if len(term) == 1 else None
    return st.median(float(r['q']) for r in rows), value, len(rows)

if __name__ == '__main__':
    d, dirs, station, md = '0.02', '16', os.path.join(HERE, '..', '..', 'station'), False
    a = sys.argv[1:]
    while a:
        if a[0] == '--d': d = a[1]; a = a[2:]
        elif a[0] == '--dirs': dirs = a[1]; a = a[2:]
        elif a[0] == '--station': station = a[1]; a = a[2:]
        elif a[0] == '--md': md = True; a = a[1:]
        else: sys.exit('unknown argument ' + a[0])
    counts = {}
    head = ['energy'] + ['%s hand' % c for c, _ in CORPORA] + ['%s neato' % c for c, _ in CORPORA]
    table = []
    for name, w, al in ENERGIES:
        row = [name]
        for kind in ('', '_neato'):
            for cname, stem in CORPORA:
                q, v, k = run(station, os.path.join(HERE, 'data', stem + kind + '.txt'), w, al, d, dirs)
                counts[cname + kind] = k
                # For a term that is a count or a step (crossings, gridiness) a node is held
                # wherever the term is flat, so beside q the median term value says how far
                # the criterion is from satisfied.
                row.append('%.2f' % q if v is None else '%.2f (%.3f)' % (q, v))
        table.append(row)
    if md:
        print('| ' + ' | '.join(head) + ' |')
        print('|' + '---|' * len(head))
        for r in table: print('| ' + ' | '.join(r) + ' |')
    else:
        print('%-24s' % 'energy' + ''.join('%14s' % h for h in head[1:]))
        for r in table: print('%-24s' % r[0] + ''.join('%14s' % v for v in r[1:]))
    print('graphs: ' + ', '.join('%s %d' % kv for kv in sorted(counts.items())))
    print('d = %s, %s directions, median q over graphs; 1 = every node held. In brackets, for a\n'
          'single term: the median value of that term at the layout, 0 = satisfied.' % (d, dirs))
