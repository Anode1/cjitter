"""Build the two alignment controls from a corpus file.

  jitter  every box displaced by up to half a pitch, the declared test of
          PREREGISTRATION-STATIONARITY section 7.
  snapped every box placed uniformly at random in the diagram's own bounding box
          and snapped to the same pitch: a layout with the grid but no author.
Box sizes, node count and bounding box are preserved in both.
"""
import random, sys

def transform(src, dst, mode, pitch, seed):
    rng = random.Random(seed)
    out, block, head = [], [], None

    def flush():
        if head is None:
            return
        xs = [b[0] for b in block]; ys = [b[1] for b in block]
        lox, hix, loy, hiy = min(xs), max(xs), min(ys), max(ys)
        out.append(head)
        for (x, y, w, h) in block:
            if mode == "jitter":
                nx = x + rng.uniform(-pitch / 2.0, pitch / 2.0)
                ny = y + rng.uniform(-pitch / 2.0, pitch / 2.0)
            else:
                nx = round(rng.uniform(lox, hix) / pitch) * pitch
                ny = round(rng.uniform(loy, hiy) / pitch) * pitch
            out.append(f"V {nx:.6f} {ny:.6f} {w:.6f} {h:.6f}")

    for line in open(src):
        line = line.rstrip("\n")
        if line.startswith("G "):
            flush(); block = []; head = line
        elif line.startswith("V "):
            f = line.split()
            block.append(tuple(float(t) for t in f[1:5]))
        elif line.startswith("#"):
            out.append(line)
        else:
            if block and head is not None and (line.startswith("E ") or line.startswith("U ")):
                flush(); head = None; block = []
            out.append(line)
    flush()
    open(dst, "w").write("\n".join(out) + "\n")

if __name__ == "__main__":
    src, dst, mode, pitch, seed = sys.argv[1:6]
    transform(src, dst, mode, float(pitch), int(seed))
