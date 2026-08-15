"""Renders ERD.png and ERD_prev.png from graph_anon.json. These images are this renderer's
work, never a Workbench export -- an export would carry the real names, which is the reason the
committed pair can exist at all. Same drawing language as the erd example's --svg.

    python3 render.py && rsvg-convert -w 1800 ERD_anon.svg -o ERD.png \
                      && rsvg-convert -w 1800 ERD_prev_anon.svg -o ERD_prev.png
"""
import json

g = json.load(open('graph_anon.json'))
added = set(g['added'])


def render(rev, path, mark_added):
    figs, edges = rev['figs'], rev['edges']
    W = max(f[0] + f[2] for f in figs.values()) + 40
    H = max(f[1] + f[3] for f in figs.values()) + 40
    cx = {n: (f[0] + f[2] / 2, f[1] + f[3] / 2) for n, f in figs.items()}
    out = ["<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g' "
           "font-family='sans-serif'>" % (W, H),
           "<rect width='%g' height='%g' fill='#fafafa'/>" % (W, H)]
    for a, b in edges:
        (ax, ay), (bx, by) = cx[a], cx[b]
        nu = mark_added and (a in added or b in added)
        out.append("<line x1='%g' y1='%g' x2='%g' y2='%g' stroke='%s' stroke-width='2'/>"
                   % (ax, ay, bx, by, '#c60' if nu else '#999'))
    for n, f in figs.items():
        nu = mark_added and n in added
        out.append("<rect x='%g' y='%g' width='%g' height='%g' rx='6' fill='%s' "
                   "stroke='%s' stroke-width='2'/>"
                   % (f[0], f[1], f[2], f[3], '#fc3' if nu else '#e8e8e8',
                      '#963' if nu else '#555'))
        out.append("<text x='%g' y='%g' text-anchor='middle' font-size='22' fill='%s'>%s</text>"
                   % (f[0] + f[2] / 2, f[1] + f[3] / 2 + 7,
                      '#630' if nu else '#333', n))
    out.append('</svg>')
    open(path, 'w').write('\n'.join(out))
    print(path, len(figs), 'tables,', len(edges), 'edges, canvas %gx%g' % (W, H))


render(g['cur'], 'ERD_anon.svg', True)
render(g['prev'], 'ERD_prev_anon.svg', False)
