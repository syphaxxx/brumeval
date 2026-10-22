"""Placer un habitant ou un coffre sur une carte existante sans rien boucher (scripts de outils/villes/)."""
import json
import os
W = set('.,=FBagbkcinzpq')
ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'data', 'cartes')
def load(mid): return json.load(open(os.path.join(ROOT, mid + '.json'), encoding='utf-8'))
def occupied(m):
    occ=set()
    for k in ('habitants','coffres','panneaux'):
        for o in m.get(k,[]): occ.add((o['x'],o['y']))
    for o in m.get('passages',[]):
        for dx in (-1,0,1):
            for dy in (-1,0,1): occ.add((o['x']+dx,o['y']+dy))
    for b in m.get('boss',[]):
        for dx in range(-1,3):
            for dy in range(-1,3): occ.add((b['x']+dx,b['y']+dy))
    for b in m.get('batiments',[]):  # le bâtiment et la rangée devant lui (porte)
        for xx in range(b['x']-1, b['x']+b['l']+1):
            for yy in range(b['y'], b['y']+b['h']+1): occ.add((xx,yy))
    return occ
def near(m, x, y, rad=8, ring=1):
    """Case praticable libre la plus proche de (x, y), entourée de cases praticables (ne bouche rien)."""
    R=m['tuiles']; H=len(R); Wd=len(R[0]); occ=occupied(m)
    best=None
    for yy in range(max(1,y-rad),min(H-1,y+rad+1)):
        for xx in range(max(1,x-rad),min(Wd-1,x+rad+1)):
            if (xx,yy) in occ or R[yy][xx] not in W: continue
            ok=all(R[yy+dy][xx+dx] in W and (xx+dx,yy+dy) not in occ for dx in range(-ring,ring+1) for dy in range(-ring,ring+1) if (dx,dy)!=(0,0))
            if not ok: continue
            d=abs(xx-x)+abs(yy-y)
            if best is None or d<best[0]: best=(d,xx,yy)
    return (best[1],best[2]) if best else None

def reachable(m):
    """Cases praticables atteignables depuis les passages de la carte (sans traverser habitants ni coffres)."""
    from collections import deque
    R=m['tuiles']; H=len(R); Wd=len(R[0])
    block=set((o['x'],o['y']) for k in ('habitants','coffres','panneaux') for o in m.get(k,[]))
    for b in m.get('batiments',[]):
        for xx in range(b['x'], b['x']+b['l']):
            for yy in range(b['y'], b['y']+b['h']): block.add((xx,yy))
    for b in m.get('boss',[]):
        for dx in (0,1):
            for dy in (0,1): block.add((b['x']+dx,b['y']+dy))
    seen=set(); q=deque()
    for w in m.get('passages',[]):
        q.append((w['x'],w['y'])); seen.add((w['x'],w['y']))
    while q:
        x,y=q.popleft()
        for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
            nx,ny=x+dx,y+dy
            if 0<=nx<Wd and 0<=ny<H and (nx,ny) not in seen and R[ny][nx] in W and (nx,ny) not in block:
                seen.add((nx,ny)); q.append((nx,ny))
    return seen

def near_reach(m, x, y, ring=0):
    """Comme near, mais seulement une case atteignable à pied (et dont un voisin l'est aussi)."""
    ok=reachable(m); occ=occupied(m); R=m['tuiles']
    best=None
    for (xx,yy) in ok:
        if (xx,yy) in occ: continue
        if ring and not all((xx+dx,yy+dy) in ok for dx in (-1,0,1) for dy in (-1,0,1) if (dx,dy)!=(0,0)): continue
        d=abs(xx-x)+abs(yy-y)
        if best is None or d<best[0]: best=(d,xx,yy)
    return (best[1],best[2]) if best else None
