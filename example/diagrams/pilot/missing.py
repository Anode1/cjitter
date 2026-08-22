"""Which aesthetic is missing? Add one candidate term at a time to the base three, fit all
weights by random search on a training half, and report the held-out displacement: how far a
capped descent still moves the human layout, as a fraction of the distance it was allowed.
0% means the human layout is stationary under that energy."""
import json, glob, os, random, subprocess, statistics as st, sys
CAP='0.02'; BUDGET='4000'
DRAWS=int(sys.argv[1]) if len(sys.argv)>1 else 60
NG=int(sys.argv[2]) if len(sys.argv)>2 else 50
def load(d,lo,hi):
    out=[]
    for fn in sorted(glob.glob(os.path.join(d,'*.json'))):
        g=json.load(open(fn))
        if not(lo<=g['n']<=hi) or g['m']<1: continue
        xs=[p[0] for p in g['xy']]; ys=[p[1] for p in g['xy']]
        s=max(max(xs)-min(xs),max(ys)-min(ys))
        if s<=0: continue
        L=["%d %d"%(g['n'],g['m'])]
        for (x,y),(w,h) in zip(g['xy'],g['wh']):
            L.append("%.6f %.6f %.6f %.6f"%((x-min(xs))/s,(y-min(ys))/s,w/s,h/s))
        for a,b in g['e']: L.append("%d %d"%(a,b))
        out.append("\n".join(L))
    return out
G=load('pr_hs',15,40); random.seed(7); random.shuffle(G); G=G[:2*NG]
train,test=G[:NG],G[NG:]
def disp(graphs,w):
    inp="%d\n"%len(graphs)+"\n".join(graphs)
    o=subprocess.run(['./stat6',BUDGET,'12345',CAP]+[str(x) for x in w],
                     input=inp,capture_output=True,text=True)
    r=[]
    for line in o.stdout.strip().split("\n"):
        v=line.split()
        if len(v)>=7: r.append(100.0*float(v[6]))
    return st.median(r) if r else 100.0
NAMES=['crossings','length','overlap','orthogonality','alignment','node-edge']
def fit(active,draws):
    best=None
    for _ in range(draws):
        w=[0.0]*6
        for i in active: w[i]=10**random.uniform(-2,2)
        s=sum(w); w=[3*x/s for x in w]
        d=disp(train,w)
        if best is None or d<best[0]: best=(d,w)
    return best
base=[0,1,2]
print("held-out displacement, %% of the cap that the descent still uses (0 = stationary)")
print("  asserted (1,1,1)                      : %.1f%%"%disp(test,[1,1,1,0,0,0]))
b=fit(base,DRAWS); print("  base three, weights fitted            : %.1f%%"%disp(test,b[1]))
for k in (3,4,5):
    b=fit(base+[k],DRAWS)
    print("  base + %-14s fitted        : %.1f%%   weights %s"%(
        NAMES[k], disp(test,b[1]), " ".join("%s=%.2f"%(NAMES[i],b[1][i]) for i in base+[k])))
b=fit([0,1,2,3,4,5],DRAWS*2)
print("  all six terms fitted                  : %.1f%%"%disp(test,b[1]))
print("     weights: "+" ".join("%s=%.2f"%(NAMES[i],b[1][i]) for i in range(6)))
