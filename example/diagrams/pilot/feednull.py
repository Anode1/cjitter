import json,glob,os,subprocess,sys,statistics as st
budget=int(sys.argv[1]); lo=int(sys.argv[2]); hi=int(sys.argv[3]); d=sys.argv[4]
mode=sys.argv[5]; cap=sys.argv[6]
rows=[]
for fn in sorted(glob.glob(os.path.join(d,'*.json'))):
    g=json.load(open(fn)); n,m=g['n'],g['m']
    if not(lo<=n<=hi) or m<1: continue
    xy=g['xy']; wh=g['wh']
    xs=[p[0] for p in xy]; ys=[p[1] for p in xy]
    s=max(max(xs)-min(xs),max(ys)-min(ys))
    if s<=0: continue
    inp=["%d %d"%(n,m)]
    for (x,y),(w,h) in zip(xy,wh):
        inp.append("%.6f %.6f %.6f %.6f"%((x-min(xs))/s,(y-min(ys))/s,w/s,h/s))
    for a,b in g['e']: inp.append("%d %d"%(a,b))
    o=subprocess.run(['./stat2',str(budget),'12345',mode,cap],
                     input="\n".join(inp),capture_output=True,text=True)
    if o.returncode: continue
    v=o.stdout.split()
    if len(v)<8: continue
    rows.append([int(v[0]),int(v[1])]+[float(t) for t in v[2:]])
def red(a,b): return 0.0 if a<=0 else 100.0*(a-b)/a
A=[red(r[2],r[3]) for r in rows]                      # from the human
B=[red(r[4],r[5]) for r in rows]                      # from a converged local optimum
C=[red(r[6],r[7]) for r in rows]                      # from that optimum, perturbed by cap
print("cap=%s  graphs=%d  budget=%d"%(cap,len(rows),budget))
print("  A  removed descending from the HUMAN layout        : median %.2f%%"%st.median(A))
print("  B  removed descending from a CONVERGED OPTIMUM     : median %.2f%%   <- procedural slack"%st.median(B))
print("  C  removed from that optimum PERTURBED by the cap  : median %.2f%%"%st.median(C))
print("  A > B on %d/%d graphs;  median A-B = %.2f points"%(
      sum(1 for a,b in zip(A,B) if a>b), len(rows), st.median([a-b for a,b in zip(A,B)])))
