import json,glob,os,subprocess,sys,statistics as st
budget=int(sys.argv[1]); lo=int(sys.argv[2]); hi=int(sys.argv[3]); d=sys.argv[4]
rows=[]
for fn in sorted(glob.glob(os.path.join(d,'*.json'))):
    g=json.load(open(fn))
    n,m=g['n'],g['m']
    if not(lo<=n<=hi): continue
    xy=g['xy']; wh=g['wh']
    xs=[p[0] for p in xy]; ys=[p[1] for p in xy]
    sx=max(xs)-min(xs); sy=max(ys)-min(ys); s=max(sx,sy)
    if s<=0: continue
    inp=["%d %d"%(n,m)]
    for (x,y),(w,h) in zip(xy,wh):
        inp.append("%.6f %.6f %.6f %.6f"%((x-min(xs))/s,(y-min(ys))/s,w/s,h/s))
    for a,b in g['e']: inp.append("%d %d"%(a,b))
    out=subprocess.run(['./stat',str(budget),'12345'],input="\n".join(inp),capture_output=True,text=True)
    if out.returncode: continue
    v=out.stdout.split()
    n_,m_,eh,ehd,er0,erd,ectl=int(v[0]),int(v[1]),*map(float,v[2:])
    rows.append((os.path.basename(fn),n_,m_,eh,ehd,er0,erd,ectl))
def red(a,b): return 0.0 if a<=0 else 100.0*(a-b)/a
print("graphs: %d"%len(rows))
r_h=[red(r[3],r[4]) for r in rows]
print("median %% of the human layout's energy removed by descent: %.1f%%"%st.median(r_h))
print("  quartiles: %.1f / %.1f"%(sorted(r_h)[len(r_h)//4], sorted(r_h)[3*len(r_h)//4]))
print("  fraction of graphs where descent removes >5%%: %.2f"%(sum(1 for x in r_h if x>5)/len(r_h)))
print("  fraction where descent removes >0.5%%: %.2f"%(sum(1 for x in r_h if x>0.5)/len(r_h)))
wins_h=sum(1 for r in rows if r[4]<r[6])
print("descent-from-human beats descent-from-random on %d/%d"%(wins_h,len(rows)))
wins_c=sum(1 for r in rows if r[6]<r[7])
print("descent-from-random beats matched-budget uniform control on %d/%d"%(wins_c,len(rows)))
print("median E(human)=%.4f  E(human,descended)=%.4f  E(random,descended)=%.4f  E(control)=%.4f"%(
    st.median([r[3] for r in rows]),st.median([r[4] for r in rows]),st.median([r[6] for r in rows]),st.median([r[7] for r in rows])))
# scaling with n
for lo2,hi2 in [(15,24),(25,49),(50,99),(100,400)]:
    s=[red(r[3],r[4]) for r in rows if lo2<=r[1]<=hi2]
    if s: print("  n %3d-%-3d (%3d graphs): median reduction %.1f%%"%(lo2,hi2,len(s),st.median(s)))
