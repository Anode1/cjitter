"""Emit a TikZ picture of one graph from a station corpus file, scaled to a given width."""
import sys

def read(path, gid):
    hdr, V, E = None, [], []
    cur = None
    for line in open(path):
        f = line.split()
        if line.startswith("G "):
            if cur == gid: break
            cur = f[1]; V, E = [], []
        elif cur == gid and line.startswith("V "):
            V.append([float(x) for x in f[1:5]])
        elif cur == gid and (line.startswith("E ") or line.startswith("U ")):
            E.append((int(f[1]), int(f[2]), [float(x) for x in f[3:]], line[0]))
    return V, E

def emit(V, E, width, label, hi=()):
    xs = [v[0] for v in V]; ys = [v[1] for v in V]
    ws = [v[2] for v in V]; hs = [v[3] for v in V]
    lox = min(x - w/2 for x, w in zip(xs, ws)); hix = max(x + w/2 for x, w in zip(xs, ws))
    loy = min(y - h/2 for y, h in zip(ys, hs)); hiy = max(y + h/2 for y, h in zip(ys, hs))
    s = width / max(hix - lox, hiy - loy)
    X = lambda v: (v - lox) * s
    Y = lambda v: (hiy - v) * s          # flip: screen y grows downward
    out = [r"\begin{tikzpicture}[x=1cm,y=1cm,line width=0.3pt]"]
    for (a, b, r, kind) in E:
        pts = [(X(r[i]), Y(r[i+1])) for i in range(0, len(r) - 1, 2)] if len(r) >= 4 else \
              [(X(V[a][0]), Y(V[a][1])), (X(V[b][0]), Y(V[b][1]))]
        out.append(r"\draw[gray!70] " + " -- ".join(f"({x:.3f},{y:.3f})" for x, y in pts) + ";")
    for i, (x, y, w, h) in enumerate(V):
        st = "fill=black!12,draw=black!55" if i not in hi else "fill=orange!35,draw=orange!80!black,line width=0.7pt"
        out.append(f"\\draw[{st}] ({X(x)-w*s/2:.3f},{Y(y)-h*s/2:.3f}) rectangle "
                   f"({X(x)+w*s/2:.3f},{Y(y)+h*s/2:.3f});")
    if label:
        out.append(f"\\node[anchor=north,font=\\scriptsize] at ({width/2:.2f},-0.12) {{{label}}};")
    out.append(r"\end{tikzpicture}")
    return "\n".join(out)

if __name__ == "__main__":
    path, gid, width, label = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
    V, E = read(path, gid)
    if not V: sys.exit(f"graph {gid} not found in {path}")
    print(emit(V, E, width, label))
