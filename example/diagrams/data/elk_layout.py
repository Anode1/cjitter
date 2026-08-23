#!/usr/bin/env python3
"""Lays out a corpus with the Eclipse Layout Kernel's LAYERED algorithm.

Reads the corpus text format, runs ELK layered on every graph with the box sizes
given, and writes the same corpus with new coordinates. Box sizes, graph order,
graph ids and edge lines are carried over unchanged; edge waypoints are dropped,
since the tool's routes are not kept.

ELK itself is JavaScript here: elkjs, driven by a short node program on stdin.
Both paths are arguments, --node and --elk, so nothing is assumed about where
they live.

Options. Only three are set:

    elk.algorithm   layered
    elk.direction   RIGHT
    elk.randomSeed  1

Everything else is ELK's own default, which is the point: the control has to be
the algorithm as a BPMN or KIELER editor would run it, not a tuned variant.
RIGHT is asked for by the measurement. The seed is ELK's own default value,
written out because layer sweep crossing minimisation reads it, and a control
that moves between runs is not a control.

A V line holds the centre of the box; ELK reports the top left corner, so half
the box is added back. Coordinates are raw ELK units, printed with six decimals
like the input.

Usage:
    elk_layout.py --node NODE --elk ELKJS_DIR IN.txt OUT.txt
"""

import argparse
import json
import os
import subprocess
import sys

DRIVER = r"""
const ELK = require(process.argv[1]);
const elk = new ELK();
let buf = '';
process.stdin.setEncoding('utf8');
process.stdin.on('data', d => { buf += d; });
process.stdin.on('end', async () => {
  const graphs = JSON.parse(buf);
  const out = [];
  for (const g of graphs) {
    const r = await elk.layout(g);
    const box = {};
    for (const c of r.children) box[c.id] = [c.x, c.y, c.width, c.height];
    out.push(box);
  }
  process.stdout.write(JSON.stringify(out));
});
"""


def read_corpus(path):
    """Returns a list of (id, nodes, edges); a node is [x, y, w, h], an edge
    (kind, a, b) with kind 'E' or 'U'."""
    graphs = []
    want_v = want_e = 0
    with open(path) as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            t = line.split()
            if t[0] == "G":
                if want_v or want_e:
                    sys.exit("%s:%d: graph %s is short" % (path, lineno, graphs[-1][0]))
                graphs.append((t[1], [], []))
                want_v, want_e = int(t[2]), int(t[3])
            elif t[0] == "V":
                if not want_v:
                    sys.exit("%s:%d: unexpected V" % (path, lineno))
                graphs[-1][1].append([float(x) for x in t[1:5]])
                want_v -= 1
            elif t[0] in ("E", "U"):
                if want_v or not want_e:
                    sys.exit("%s:%d: unexpected %s" % (path, lineno, t[0]))
                graphs[-1][2].append((t[0], int(t[1]), int(t[2])))
                want_e -= 1
            else:
                sys.exit("%s:%d: unknown line %s" % (path, lineno, t[0]))
    if want_v or want_e:
        sys.exit("%s: last graph is short" % path)
    return graphs


def elk_input(graphs):
    """The corpus as ELK graph JSON. A U edge goes to ELK as a directed edge as
    listed: ELK layered has no undirected edge, and the direction it is given is
    the one that decides the layering."""
    out = []
    for gid, nodes, edges in graphs:
        out.append({
            "id": gid,
            "layoutOptions": {
                "elk.algorithm": "layered",
                "elk.direction": "RIGHT",
                "elk.randomSeed": "1",
            },
            "children": [
                {"id": "n%d" % i, "width": w, "height": h}
                for i, (_x, _y, w, h) in enumerate(nodes)
            ],
            "edges": [
                {"id": "e%d" % j, "sources": ["n%d" % a], "targets": ["n%d" % b]}
                for j, (_k, a, b) in enumerate(edges)
            ],
        })
    return out


def run_elk(node, elkdir, graphs):
    p = subprocess.run(
        [node, "-e", DRIVER, elkdir],
        input=json.dumps(elk_input(graphs)),
        stdout=subprocess.PIPE, text=True,
    )
    if p.returncode:
        sys.exit("node exited %d" % p.returncode)
    return json.loads(p.stdout)


def write_corpus(path, header, graphs, boxes):
    with open(path, "w") as f:
        f.write(header + "\n")
        for (gid, nodes, edges), box in zip(graphs, boxes):
            f.write("G %s %d %d\n" % (gid, len(nodes), len(edges)))
            for i, (_x, _y, w, h) in enumerate(nodes):
                bx, by, bw, bh = box["n%d" % i]
                if abs(bw - w) > 1e-6 or abs(bh - h) > 1e-6:
                    sys.exit("%s node %d: ELK resized the box" % (gid, i))
                f.write("V %.6f %.6f %.6f %.6f\n"
                        % (bx + w / 2.0, by + h / 2.0, w, h))
            for kind, a, b in edges:
                f.write("%s %d %d\n" % (kind, a, b))


def elk_version(elkdir):
    with open(os.path.join(elkdir, "package.json")) as f:
        return json.load(f)["version"]


def main():
    ap = argparse.ArgumentParser(description="lay a corpus out with ELK layered")
    ap.add_argument("--node", required=True, help="path to the node binary")
    ap.add_argument("--elk", required=True, help="path to the elkjs module directory")
    ap.add_argument("input")
    ap.add_argument("output")
    a = ap.parse_args()

    graphs = read_corpus(a.input)
    boxes = run_elk(a.node, a.elk, graphs)
    if len(boxes) != len(graphs):
        sys.exit("ELK returned %d of %d graphs" % (len(boxes), len(graphs)))
    header = ("# the graphs of %s laid out by ELK layered %s, same box sizes"
              % (os.path.basename(a.input), elk_version(a.elk)))
    write_corpus(a.output, header, graphs, boxes)


if __name__ == "__main__":
    main()
