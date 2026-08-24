"""Is the first 300 by model id a fair draw from the 6,723 in the band?"""
import csv, statistics, random

first300 = [l.split()[1] for l in open("/home/vas/cjitter/example/diagrams/data/bpmn.txt")
            if l.startswith("G ")]
fset = set(first300)
print(f"{'term':10} {'committed 300':>13} {'population':>11} "
      f"{'random 300: median [2.5%, 97.5%]':>34} {'percentile':>11}")
for term in ("overlap", "length", "stress", "alignA1", "flow"):
    rows = list(csv.DictReader(open(f"/tmp/claude-1000/-home-vas-cjitter/dad3ec16-cfe2-437e-babb-5129d27b044a/scratchpad/pop_{term}.csv")))
    q = {r["id"]: float(r["q"]) for r in rows}
    allq = list(q.values())
    comm = statistics.median([q[i] for i in first300 if i in q])
    pop = statistics.median(allq)
    rng = random.Random(20260824)
    draws = sorted(statistics.median(rng.sample(allq, 300)) for _ in range(2000))
    lo, hi = draws[50], draws[1949]
    pct = 100.0 * sum(1 for d in draws if d < comm) / len(draws)
    print(f"{term:10} {comm:13.3f} {pop:11.3f}   {statistics.median(draws):8.3f} "
          f"[{lo:.3f}, {hi:.3f}]{'':9} {pct:9.1f}%")
