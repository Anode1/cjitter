"""The descent table of the paper from desc/<corpus>_<energy>.csv (station descend, 15
seeds, cap 0.02): per corpus and energy the median fraction of the cap used and of the
energy removed by the capped climber and by uniform sampling at the same budget, the
share of diagrams where the climber ends lower on at least 12 of 15 seeds, the median q
where the uncapped climber stops, and the share of diagrams whose converged reference
stops below 0.9. Run from this directory: python3 desc_summary.py > desc.md
"""
import csv, os, statistics as st
HERE = os.path.dirname(os.path.abspath(__file__))
print('| corpus | energy | cap climb | cap random | removed climb | removed random | >=12/15 | q converged | reference < 0.9 |')
print('|---|---|---|---|---|---|---|---|---|')
for c in ['hs', 'sbgn', 'bpmnr', 'bpmn']:
    for e in ['COL', 'COA1', 'S', 'COF']:
        p = os.path.join(HERE, 'desc', '%s_%s.csv' % (c, e))
        if not os.path.exists(p): continue
        r = list(csv.DictReader(open(p)))
        m = lambda k: st.median(float(x[k]) for x in r)
        print('| %s | %s | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f |' % (c, e, m('cap_climb'), m('cap_random'), m('rho_climb'), m('rho_random'),
              sum(int(x['wins']) >= 12 for x in r) / len(r), m('q_converged'), sum(float(x['q_converged']) < 0.9 for x in r) / len(r)))
