"""The profile: which criteria hold a hand layout, and which hold a tool's layout of the same
graph. Runs station's directional test over the corpora for the declared energies and
prints the median q per cell (1 = every node held, 0 = none).

    python3 profile.py [--d 0.02] [--dirs 16] [--L fit|median|rsqrt] [--layouts hand,neato,prism,dot]
                       [--only NAME,NAME,...] [--ci] [--save DIR] [--station ../../station] [--md | --tex]
    python3 profile.py --sweep [--only ...]        the radius sweep, d in 0.005 0.01 0.02 0.05

--tex prints the paper's layout instead: one block per corpus, a column per layout kind,
the interval on the hand column only.

--ci adds a 95% bootstrap interval to every median: 1000 resamples of graphs, seed 1, the
2.5th and 97.5th percentile of the resampled medians. --save DIR keeps every station CSV as
DIR/<corpus stem>_<layout>_<energy>.csv, which analyse.py pairs for the tests.
"""
import csv, io, os, random, statistics as st, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TERMS = ['crossings', 'overlap', 'length', 'stress', 'orthogonality', 'alignment', 'node-edge', 'flow']
CORPORA = [('WikiPathways', 'hs'), ('Reactome', 'sbgn'), ('BPMN', 'bpmn')]
LAYOUTS = ['hand', 'neato', 'prism', 'dot', 'elk']   # hand, then the tool controls; file stem suffixes
# name, weights C,O,L,S,R,A,N, alignment definition
ENERGIES = [
    ('crossings alone',            '1,0,0,0,0,0,0,0', 'a1'),
    ('overlap alone',              '0,1,0,0,0,0,0,0', 'a1'),
    ('length alone',               '0,0,1,0,0,0,0,0', 'a1'),
    ('stress alone',               '0,0,0,1,0,0,0,0', 'a1'),
    ('orthogonality alone',        '0,0,0,0,1,0,0,0', 'a1'),
    ('alignment A1 alone',         '0,0,0,0,0,1,0,0', 'a1'),
    ('alignment A3 alone',         '0,0,0,0,0,1,0,0', 'a3'),
    ('gridiness alone',            '0,0,0,0,0,1,0,0', 'grid'),
    ('node-edge alone',            '0,0,0,0,0,0,1,0', 'a1'),
    ('flow alone',                 '0,0,0,0,0,0,0,1', 'a1'),
    ('C+O',                        '1,1,0,0,0,0,0,0', 'a1'),
    ('C+O+L',                      '1,1,1,0,0,0,0,0', 'a1'),
    ('C+O+S',                      '1,1,0,1,0,0,0,0', 'a1'),
    ('C+O+R',                      '1,1,0,0,1,0,0,0', 'a1'),
    ('C+O+A1',                     '1,1,0,0,0,1,0,0', 'a1'),
    ('C+O+A3',                     '1,1,0,0,0,1,0,0', 'a3'),
    ('C+O+grid',                   '1,1,0,0,0,1,0,0', 'grid'),
    ('C+O+N',                      '1,1,0,0,0,0,1,0', 'a1'),
    ('C+O+F',                      '1,1,0,0,0,0,0,1', 'a1'),
]
SWEEP_D = ['0.005', '0.01', '0.02', '0.05']

def bootstrap(xs, reps=1000, seed=1):
    rng = random.Random(seed)
    meds = sorted(st.median(rng.choices(xs, k=len(xs))) for _ in range(reps))
    return meds[int(0.025 * reps)], meds[int(0.975 * reps) - 1]

def run(station, corpus, weights, align, d, dirs, lref, ci, save=None):
    o = subprocess.run([station, 'direct', '--corpus', corpus, '--weights', weights,
                        '--align', align, '--d', d, '--dirs', dirs, '--L', lref],
                       capture_output=True, text=True, check=True).stdout
    if save: open(save, 'w').write(o)
    rows = list(csv.DictReader(io.StringIO(o)))
    term = [t for t, x in zip(TERMS, weights.split(',')) if x != '0']
    qs = [float(r['q']) for r in rows]
    value = st.median(float(r[term[0]]) for r in rows) if len(term) == 1 else None
    return st.median(qs), (bootstrap(qs) if ci else None), value, len(rows)

def cell(q, iv, v):
    # For a term that is a count or a step (crossings, gridiness) a node is held wherever the
    # term is flat, so beside q the median term value says how far the criterion is from
    # satisfied.
    s = '%.2f' % q
    if iv: s += ' [%.2f, %.2f]' % iv
    if v is not None: s += ' (%.3f)' % v
    return s

def emit_tex(energies, layouts, results):
    """results[(energy, layout, corpus)] = (q, interval, value). Booktabs rows, one block per
    corpus, to paste into the paper; the interval only on the hand column."""
    print('\\begin{tabular}{ll' + 'r' * len(layouts) + '}')
    print('\\toprule')
    print('corpus & energy & ' + ' & '.join('\\texttt{%s}' % l if l != 'hand' else 'hand' for l in layouts) + ' \\\\')
    print('\\midrule')
    for ci, (cname, stem) in enumerate(CORPORA):
        if ci: print('\\midrule')
        for ei, (name, w, al) in enumerate(energies):
            cells = []
            for layout in layouts:
                q, iv, v = results[(name, layout, cname)]
                s = '%.2f' % q
                if iv and layout == 'hand': s += ' [%.2f, %.2f]' % iv
                if v is not None: s += ' (%.3f)' % v
                cells.append(s)
            print('%s & %s & %s \\\\' % (cname if ei == 0 else '', name.replace('+', '$+$'), ' & '.join(cells)))
    print('\\bottomrule')
    print('\\end{tabular}')

def corpus_file(stem, layout):
    return os.path.join(HERE, 'data', stem + ('' if layout == 'hand' else '_' + layout) + '.txt')

def emit(head, table, md):
    if md:
        print('| ' + ' | '.join(head) + ' |')
        print('|' + '---|' * len(head))
        for r in table: print('| ' + ' | '.join(r) + ' |')
    else:
        w = max(len(h) for h in head[1:]) + 2
        print('%-24s' % head[0] + ''.join(('%' + str(w) + 's') % h for h in head[1:]))
        for r in table: print('%-24s' % r[0] + ''.join(('%' + str(w) + 's') % v for v in r[1:]))

if __name__ == '__main__':
    d, dirs, lref, md, ci, sweep, tex, save = '0.02', '16', 'fit', False, False, False, False, None
    station = os.path.join(HERE, '..', '..', 'station')
    layouts, only = LAYOUTS, None
    a = sys.argv[1:]
    while a:
        if a[0] == '--d': d = a[1]; a = a[2:]
        elif a[0] == '--dirs': dirs = a[1]; a = a[2:]
        elif a[0] == '--L': lref = a[1]; a = a[2:]
        elif a[0] == '--layouts': layouts = a[1].split(','); a = a[2:]
        elif a[0] == '--only': only = a[1].split(','); a = a[2:]
        elif a[0] == '--station': station = a[1]; a = a[2:]
        elif a[0] == '--md': md = True; a = a[1:]
        elif a[0] == '--tex': tex = True; a = a[1:]
        elif a[0] == '--ci': ci = True; a = a[1:]
        elif a[0] == '--save': save = a[1]; os.makedirs(save, exist_ok=True); a = a[2:]
        elif a[0] == '--sweep': sweep = True; a = a[1:]
        else: sys.exit('unknown argument ' + a[0])
    energies = [e for e in ENERGIES if not only or e[0] in only]
    if only and len(energies) != len(only): sys.exit('unknown energy in --only')
    counts = {}
    if sweep:
        # One table per layout kind; columns are corpus x d.
        for layout in layouts:
            head = ['energy, %s' % layout] + ['%s d=%s' % (c, dd) for c, _ in CORPORA for dd in SWEEP_D]
            table = []
            for name, w, al in energies:
                row = [name]
                for cname, stem in CORPORA:
                    for dd in SWEEP_D:
                        q, iv, v, k = run(station, corpus_file(stem, layout), w, al, dd, dirs, lref, ci)
                        row.append(cell(q, iv, None))
                table.append(row)
            emit(head, table, md)
            print()
        sys.exit(0)
    head = ['energy'] + ['%s %s' % (c, l) for l in layouts for c, _ in CORPORA]
    table, results = [], {}
    for name, w, al in energies:
        row = [name]
        for layout in layouts:
            for cname, stem in CORPORA:
                out = save and os.path.join(save, '%s_%s_%s.csv' % (stem, layout, name.replace(' ', '_').replace('+', '')))
                q, iv, v, k = run(station, corpus_file(stem, layout), w, al, d, dirs, lref, ci, out)
                counts['%s %s' % (cname, layout)] = k
                results[(name, layout, cname)] = (q, iv, v)
                row.append(cell(q, iv, v))
        table.append(row)
    if tex: emit_tex(energies, layouts, results)
    else: emit(head, table, md)
    print('graphs: ' + ', '.join('%s %d' % kv for kv in sorted(counts.items())))
    print('d = %s, %s directions, L %s, median q over graphs; 1 = every node held. In brackets, for a\n'
          'single term: the median value of that term at the layout, 0 = satisfied.' % (d, dirs, lref))
