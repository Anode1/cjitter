"""Condense the Zenodo FBUDGET_*.csv files into one binary table.

Run from the directory holding Processed_data.zip (Zenodo 10.5281/zenodo.10561215);
writes fb.npz and fb_names.json beside it.

Columns kept: lib, algname, budget_factor, dim, fid, iid, run, fx.
Output: fb.npz with integer-coded columns and a names list, plus coverage counts.
"""
import zipfile, io, csv, json
import numpy as np

ZIP = "Processed_data.zip"
OUT = "fb.npz"
BF = [10, 50, 100, 500, 1000, 5000, 10000]
bf_idx = {str(b): i for i, b in enumerate(BF)}

libs, algs = [], []
lib_idx, alg_idx = {}, {}
col_lib, col_alg, col_bf, col_dim, col_fid, col_iid, col_run, col_fx = ([] for _ in range(8))
dims = {"2": 0, "5": 1, "10": 2, "20": 3}

zf = zipfile.ZipFile(ZIP)
for name in sorted(zf.namelist()):
    if not name.startswith("FBUDGET_"):
        continue
    lib = name[len("FBUDGET_"):-len(".csv")]
    if lib not in lib_idx:
        lib_idx[lib] = len(libs); libs.append(lib)
    li = lib_idx[lib]
    n0 = len(col_fx)
    with zf.open(name) as raw:
        rdr = csv.reader(io.TextIOWrapper(raw, newline=""))
        hdr = next(rdr)
        pos = {h: i for i, h in enumerate(hdr)}
        c_fx, c_run, c_bf = pos["fx"], pos["run"], pos["budget_factor"]
        c_alg, c_fid, c_iid, c_dim = pos["algname"], pos["fid"], pos["iid"], pos["dim"]
        for row in rdr:
            a = row[c_alg]
            key = (li, a)
            ai = alg_idx.get(key)
            if ai is None:
                ai = alg_idx[key] = len(algs); algs.append(key)
            col_lib.append(li); col_alg.append(ai)
            col_bf.append(bf_idx[row[c_bf]]); col_dim.append(dims[row[c_dim]])
            col_fid.append(int(row[c_fid])); col_iid.append(int(row[c_iid]))
            col_run.append(int(row[c_run])); col_fx.append(float(row[c_fx]))
    print(f"{lib}: {len(col_fx)-n0} rows, algs so far {len(algs)}", flush=True)

np.savez_compressed(
    OUT,
    lib=np.array(col_lib, np.int8), alg=np.array(col_alg, np.int16),
    bf=np.array(col_bf, np.int8), dim=np.array(col_dim, np.int8),
    fid=np.array(col_fid, np.int8), iid=np.array(col_iid, np.int8),
    run=np.array(col_run, np.int8), fx=np.array(col_fx, np.float64),
)
with open("fb_names.json", "w") as f:
    json.dump({"libs": libs, "algs": algs, "budget_factors": BF,
               "dims": [2, 5, 10, 20]}, f)
print("total rows", len(col_fx), "algs", len(algs))
