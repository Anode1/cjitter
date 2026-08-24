import random, sys
def blocks(path):
    hdr, cur, out = None, [], []
    for line in open(path):
        if line.startswith("G "):
            if hdr is not None: out.append((hdr, cur))
            hdr, cur = line, []
        elif line.startswith("#"): pass
        else: cur.append(line)
    if hdr is not None: out.append((hdr, cur))
    return out
src, dst, mode, k, seed = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
b = blocks(src)
b.sort(key=lambda x: x[0].split()[1])
pick = b[:k] if mode == "first" else random.Random(seed).sample(b, k)
with open(dst, "w") as f:
    f.write(f"# {k} graphs, {mode}, seed {seed}\n")
    for h, c in pick:
        f.write(h); f.writelines(c)
