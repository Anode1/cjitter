"""BPMN lanes and the alignment criterion: is a box held under A1 by a row its lane imposed,
or by an alignment the person chose?

    python3 lanes.py RAW_MODELS_DIR NODES_CSV

RAW_MODELS_DIR holds the archive's .json models; NODES_CSV is station's --nodes output for
the hand BPMN corpus under alignment A1 alone (best_dir -1 = held). Every node of the corpus
is given its innermost lane or pool (the deepest Lane, Pool, VerticalLane or VerticalPool
above it in the childShapes tree) and its alignment partner, the other node of the component
nearest along whichever axis is nearer, in the rescaled coordinates station uses. A held box
is then counted by what holds it: a column (its partner shares its x), a row shared with a
box of another lane or of no lane, or a row shared with a box of its own lane; for the last,
the lane's height in box heights says whether the lane left room to do otherwise. Horizontal
lanes constrain y only, so columns and cross-lane rows cannot be the lane's.
"""
import csv, json, os, sys, statistics as st
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'parsers'))
import parse_bpmn

LANES = {'Lane', 'Pool', 'VerticalLane', 'VerticalPool', 'CollapsedPool'}

def containers(shape, ox, oy, stack, out):
    """out[resourceId] = (innermost container id, its height) for every shape, in parse_bpmn's
    tree order and with its coordinate accumulation."""
    for s in shape.get('childShapes', []):
        st_ = (s.get('stencil') or {}).get('id', '')
        b = s.get('bounds') or {}
        ul, lr = b.get('upperLeft') or {}, b.get('lowerRight') or {}
        x0, y0 = ox + ul.get('x', 0), oy + ul.get('y', 0)
        y1 = oy + lr.get('y', 0)
        rid = s.get('resourceId')
        if rid: out[rid] = stack[-1] if stack else (None, None)
        if st_ in LANES:
            stack.append((rid, abs(y1 - y0)))
            containers(s, x0, y0, stack, out)
            stack.pop()
        else:
            containers(s, x0, y0, stack, out)

def lcc_ids(g):
    """The same component make_corpus.lcc takes, as the parser's node indices, sorted."""
    adj = [set() for _ in range(g['n'])]
    for a, b in g['e']:
        if a != b: adj[a].add(b); adj[b].add(a)
    seen, best = set(), []
    for s in range(g['n']):
        if s in seen: continue
        stack, comp = [s], []
        seen.add(s)
        while stack:
            u = stack.pop(); comp.append(u)
            for v in adj[u]:
                if v not in seen: seen.add(v); stack.append(v)
        if len(comp) > len(best): best = comp
    return sorted(best)

def rescaled(xy, wh):
    """station's unit-square rescaling: the bounding box of the boxes fits the unit square."""
    x0 = min(x - w / 2 for (x, y), (w, h) in zip(xy, wh)); x1 = max(x + w / 2 for (x, y), (w, h) in zip(xy, wh))
    y0 = min(y - h / 2 for (x, y), (w, h) in zip(xy, wh)); y1 = max(y + h / 2 for (x, y), (w, h) in zip(xy, wh))
    s = max(x1 - x0, y1 - y0) or 1
    return [((x - x0) / s, (y - y0) / s) for x, y in xy], [(w / s, h / s) for w, h in wh], s

if __name__ == '__main__':
    raw, nodes_csv = sys.argv[1], sys.argv[2]
    held = {}
    for r in csv.DictReader(open(nodes_csv)):
        held.setdefault(r['id'], []).append(int(r['best_dir']) < 0)
    kinds = {'column': 0, 'row, other lane or none': 0, 'row, same lane': 0}
    ratios, free_boxes, n_held, n_all, in_lane = [], 0, 0, 0, 0
    for fid, hv in held.items():
        path = os.path.join(raw, fid)
        g = parse_bpmn.parse(path)
        d = json.load(open(path, encoding='utf-8', errors='replace'))
        cont = {}; containers(d, 0, 0, [], cont)
        ids = lcc_ids(g)
        assert len(ids) == len(hv), (fid, len(ids), len(hv))
        xy, wh, scale = rescaled([g['xy'][i] for i in ids], [g['wh'][i] for i in ids])
        lane = [cont.get(g['ids'][i], (None, None)) for i in ids]
        for k, (x, y) in enumerate(xy):
            n_all += 1
            if lane[k][0] is not None: in_lane += 1
            if not hv[k]: continue
            n_held += 1
            best, axis, partner = None, None, None
            for j, (px, py) in enumerate(xy):
                if j == k: continue
                dx, dy = abs(px - x), abs(py - y)
                m, ax = (dx, 'x') if dx <= dy else (dy, 'y')
                if best is None or m < best: best, axis, partner = m, ax, j
            if axis == 'x':
                kinds['column'] += 1
            elif lane[k][0] is None or lane[k][0] != lane[partner][0]:
                kinds['row, other lane or none'] += 1
            else:
                kinds['row, same lane'] += 1
                ratios.append(lane[k][1] / (wh[k][1] * scale))
    print('boxes %d, in a lane or pool %.2f, held under A1 %d (%.3f)' % (n_all, in_lane / n_all, n_held, n_held / n_all))
    for k, v in kinds.items():
        print('  held by a %-24s %5d  %.3f of held, %.3f of all boxes' % (k, v, v / n_held, v / n_all))
    r = sorted(ratios)
    print('same-lane rows: lane height in box heights, median %.1f, quartiles %.1f and %.1f; under 2: %.3f'
          % (st.median(r), r[len(r) // 4], r[3 * len(r) // 4], sum(1 for x in r if x < 2) / len(r)))
