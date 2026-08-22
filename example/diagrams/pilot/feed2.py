import json,glob,os,subprocess,sys,statistics as st
budget=int(sys.argv[1]); lo=int(sys.argv[2]); hi=int(sys.argv[3]); d=sys.argv[4]; mode=sys.argv[5]
rows=[]
for fn in sorted(glob.glob(os.path.join(d,'*.json'))):
    g=json.load(open(fn)); n,m=g['n'],g['m']
    if not(lo<=n<=hi): continue
    xy=g['xy']; wh=g['wh']
    xs=[p[0] for p in xy]; ys=[p[1] for p in xy]
    s=max(max(xs)-min(xs),max(ys)-min(ys))
    if s<=0: continue
    inp=["%d %d"%(n,m)]
    for (x,y),(w,h) in zip(xy,wh):
        inp.append("%.6f %.6f %.6f %.6f"%((x-min(xs))/s,(y-min(ys))/s,w/s,h/s))
    for a,b in g['e']: inp.append("%d %d"%(a,b))
    o=subprocess.run(['./stat',str(budget),'12345',mode],input="\n".join(inp),capture_output=True,text=True)
    if o.returncode: continue
    v=o.stdout.split(); rows.append([int(v[0]),int(v[1])]+[float(t) for t in v[2:]])
def med(f): return st.median([f(r) for r in rows])
def red(a,b): return 0.0 if a<=0 else 100.0*(a-b)/a
print("mode=%s graphs=%d  median L=%.4f"%(mode,len(rows),med(lambda r:r[13])))
print(" total E: human %.4f -> descended %.4f   median reduction %.1f%%"%(
      med(lambda r:r[2]),med(lambda r:r[3]),med(lambda r:red(r[2],r[3]))))
print(" crossings/edge : human %.4f -> %.4f"%(med(lambda r:r[7]),med(lambda r:r[10])))
print(" length term    : human %.4f -> %.4f"%(med(lambda r:r[8]),med(lambda r:r[11])))
print(" overlap term   : human %.4f -> %.4f"%(med(lambda r:r[9]),med(lambda r:r[12])))
print(" share of human E in each term: cross %.2f  length %.2f  overlap %.2f"%(
      med(lambda r:r[7]/r[2]),med(lambda r:r[8]/r[2]),med(lambda r:r[9]/r[2])))
print(" descent from human beats descent from random: %d/%d"%(sum(1 for r in rows if r[3]<r[5]),len(rows)))
print(" human already better than matched-budget uniform control: %d/%d"%(sum(1 for r in rows if r[2]<r[6]),len(rows)))
for a,b in [(15,24),(25,49),(50,99)]:
    s2=[red(r[2],r[3]) for r in rows if a<=r[0]<=b]
    if s2: print("   n %3d-%-3d (%3d): median reduction %.1f%%"%(a,b,len(s2),st.median(s2)))
