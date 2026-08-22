"""Does the alignment result survive a change of encoding? Same experiment, three independent
definitions of 'things line up': A1 nearest node sharing a row or a column, A2 rows and columns
priced separately, A3 a smooth all-pairs kernel with no nearest-neighbour argmin."""
import json,glob,os,random,subprocess,statistics as st,sys
CAP='0.02'; BUDGET='4000'; DRAWS=int(sys.argv[1]); NG=int(sys.argv[2])
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
def disp(binary,graphs,w):
    inp="%d\n"%len(graphs)+"\n".join(graphs)
    o=subprocess.run([binary,BUDGET,'12345',CAP]+[str(x) for x in w],
                     input=inp,capture_output=True,text=True)
    r=[100.0*float(v[6]) for v in (l.split() for l in o.stdout.strip().split("\n")) if len(v)>=7]
    return st.median(r) if r else 100.0
def fit(binary,train,active,draws):
    best=None
    for _ in range(draws):
        w=[0.0]*6
        for i in active: w[i]=10**random.uniform(-2,2)
        s=sum(w); w=[3*x/s for x in w]
        d=disp(binary,train,w)
        if best is None or d<best[0]: best=(d,w)
    return best
for corpus in ('pr_hs','pr_sbgn','pr_bpmn'):
    G=load(corpus,15,40); random.seed(7); random.shuffle(G); G=G[:2*NG]
    train,test=G[:NG],G[NG:]
    base=disp('./stat8_1',test,[1,1,1,0,0,0])
    b3=fit('./stat8_1',train,[0,1,2],DRAWS)
    line=[f"{corpus:9s} asserted {base:5.1f}  base3 {disp('./stat8_1',test,b3[1]):5.1f}"]
    for d in (1,2,3):
        b=fit(f'./stat8_{d}',train,[0,1,2,4],DRAWS)
        line.append(f"  A{d} {disp(f'./stat8_{d}',test,b[1]):5.1f}")
    print("".join(line))
