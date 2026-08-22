import sys, os, glob, json, xml.etree.ElementTree as ET
from collections import defaultdict
def strip(t): return t.split('}')[-1]
def parse(fn):
    try: root=ET.parse(fn).getroot()
    except Exception: return None
    nodes={}
    for el in root.iter():
        if strip(el.tag)=='DataNode':
            gid=el.get('GraphId')
            g=[c for c in el if strip(c.tag)=='Graphics']
            if not gid or not g: continue
            g=g[0]
            try: nodes[gid]=(float(g.get('CenterX')),float(g.get('CenterY')),float(g.get('Width','0')),float(g.get('Height','0')))
            except (TypeError,ValueError): pass
    # anchors: map anchor GraphId -> owning interaction index, resolved later
    anchor_owner={}
    inters=[]
    for el in root.iter():
        if strip(el.tag) in ('Interaction','GraphicalLine'):
            g=[c for c in el if strip(c.tag)=='Graphics']
            if not g: continue
            pts=[c for c in g[0] if strip(c.tag)=='Point']
            ancs=[c for c in g[0] if strip(c.tag)=='Anchor']
            idx=len(inters)
            for a in ancs:
                if a.get('GraphId'): anchor_owner[a.get('GraphId')]=idx
            if len(pts)>=2:
                inters.append((pts[0].get('GraphRef'),pts[-1].get('GraphRef')))
            else:
                inters.append((None,None))
    def resolve(ref,depth=0):
        if ref is None or depth>4: return None
        if ref in nodes: return ref
        if ref in anchor_owner:
            a,b=inters[anchor_owner[ref]]
            return resolve(a,depth+1) or resolve(b,depth+1)
        return None
    edges=set()
    for a,b in inters:
        ra,rb=resolve(a),resolve(b)
        if ra and rb and ra!=rb: edges.add(tuple(sorted((ra,rb))))
    return nodes,sorted(edges)
def lcc(nodes,edges):
    adj=defaultdict(set)
    for a,b in edges: adj[a].add(b); adj[b].add(a)
    seen=set(); best=[]
    for s in adj:
        if s in seen: continue
        st=[s]; comp=[]; seen.add(s)
        while st:
            u=st.pop(); comp.append(u)
            for v in adj[u]:
                if v not in seen: seen.add(v); st.append(v)
        if len(comp)>len(best): best=comp
    return set(best)
if __name__=='__main__':
    d=sys.argv[1]; out=sys.argv[2] if len(sys.argv)>2 else None
    rows=[]; os.makedirs(out,exist_ok=True) if out else None
    for fn in sorted(glob.glob(os.path.join(d,'*.gpml'))):
        r=parse(fn)
        if not r: continue
        nodes,edges=r
        if not edges: continue
        comp=lcc(nodes,edges)
        ce=[e for e in edges if e[0] in comp and e[1] in comp]
        n=len(comp); m=len(ce)
        rows.append((os.path.basename(fn),len(nodes),len(edges),n,m))
        if out and 10<=n<=400:
            ids=sorted(comp); ix={g:i for i,g in enumerate(ids)}
            json.dump({'file':os.path.basename(fn),'n':n,'m':m,
                       'xy':[[nodes[g][0],nodes[g][1]] for g in ids],
                       'wh':[[nodes[g][2],nodes[g][3]] for g in ids],
                       'e':[[ix[a],ix[b]] for a,b in ce]},
                      open(os.path.join(out,os.path.basename(fn)[:-5]+'.json'),'w'))
    import statistics as st
    ns=[r[3] for r in rows]
    print("files with >=1 edge: %d"%len(rows))
    for lo,hi in [(2,9),(10,24),(25,49),(50,99),(100,199),(200,399),(400,10**9)]:
        c=sum(1 for x in ns if lo<=x<=hi); print("  LCC nodes %4d-%-6d : %4d"%(lo,hi,c))
    print("median LCC n=%s  median m=%s"%(st.median(ns),st.median([r[4] for r in rows])))
