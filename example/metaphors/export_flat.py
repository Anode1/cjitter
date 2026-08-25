"""Export fb.npz to the flat text the C driver reads.

fb_flat.txt: one row per run, "alg bf dim fid iid run fx", alg an integer id.
fb_algs.txt: one row per id, "id lib algname baseline_flag".
The driver pairs, counts and tests; this file only changes the container.
"""
import json
import numpy as np

D = np.load("fb.npz")
names = json.load(open("fb_names.json"))
libs = names["libs"]
algs = [tuple(a) for a in names["algs"]]

with open("fb_algs.txt", "w") as f:
    for i, (li, a) in enumerate(algs):
        f.write(f"{i} {libs[li]} {a} {1 if libs[li] == 'Baselines' else 0}\n")

cols = np.column_stack([D["alg"], D["bf"], D["dim"], D["fid"], D["iid"], D["run"]])
fx = D["fx"]
with open("fb_flat.txt", "w") as f:
    for row, v in zip(cols, fx):
        f.write(f"{row[0]} {row[1]} {row[2]} {row[3]} {row[4]} {row[5]} {v:.17g}\n")
print("rows", len(fx))
