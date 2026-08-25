"""Is the first 300 by model id a fair draw from the 6,723 in the band?

Reads data/bpmn_population_a1.csv, the per-model hold q over the whole band; its header
names the archive, its sha256, the funnel and the station commands that produced it.
"""
import csv, os, statistics, random

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")
first300 = [l.split()[1] for l in open(os.path.join(D, "bpmn.txt")) if l.startswith("G ")]
pop = list(csv.DictReader(l for l in open(os.path.join(D, "bpmn_population_a1.csv"))
                          if not l.startswith("#")))
print(f"{'term':10} {'committed 300':>13} {'population':>11} "
      f"{'random 300: median [2.5%, 97.5%]':>34} {'percentile':>11}")
for term in ("overlap", "length", "stress", "alignA1", "flow"):
    q = {r["id"]: float(r[term]) for r in pop}
    allq = list(q.values())
    comm = statistics.median([q[i] for i in first300 if i in q])
    pop_med = statistics.median(allq)
    rng = random.Random(20260824)
    draws = sorted(statistics.median(rng.sample(allq, 300)) for _ in range(2000))
    lo, hi = draws[50], draws[1949]
    pct = 100.0 * sum(1 for d in draws if d < comm) / len(draws)
    print(f"{term:10} {comm:13.3f} {pop_med:11.3f}   {statistics.median(draws):8.3f} "
          f"[{lo:.3f}, {hi:.3f}]{'':9} {pct:9.1f}%")
