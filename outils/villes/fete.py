"""La fête des lanternes (étape « 20 heures », 2026-10-20) : la nuit, après Ignarok, Lison attend sur la place de la
Vallée. Cinq lanternes de fête se sont envolées et brillent quelque part dans la vallée, mais seulement la nuit
(coffres avec « si » : quête et condition « nuit ») ; rapportées de nuit, elles s'envolent toutes ensemble et, chaque
nuit ensuite, des lanternes montent dans le ciel de la Vallée (champ « fete » de la carte). Quête « lanternes ». Une
seule fois (refuse si la quête existe). Vérifie que rien de déjà accessible ne devient inaccessible.

    python outils/villes/fete.py
"""
import json
import os
import sys
from collections import deque

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util as pu  # noqa: E402

D = carte.R
QT = [json.loads(l.strip().rstrip(',')) for l in open(D + 'quetes.json', encoding='utf-8') if l.strip().startswith('{')]
if any(q['id'] == 'lanternes' for q in QT):
    sys.exit('Déjà fait.')

m = carte.load('vallee')
before = pu.reachable(m)


def far_cell(m, extra):
    """La case d'herbe accessible la plus éloignée des passages, portes, habitants et des cases `extra`."""
    ok, occ = pu.reachable(m), pu.occupied(m)
    src = [(w['x'], w['y']) for w in m.get('passages', [])] + [(b['x'] + b['l'] // 2, b['y'] + b['h']) for b in m.get('batiments', [])]
    src += [(n['x'], n['y']) for n in m.get('habitants', [])] + list(extra)
    dist, q = {}, deque()
    for s in src:  # une case déjà prise (coffre) n'est plus accessible : on part de ses voisines
        for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
            c = (s[0] + dx, s[1] + dy)
            if c in ok and c not in dist:
                dist[c] = 0
                q.append(c)
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c = (x + dx, y + dy)
            if c in ok and c not in dist:
                dist[c] = dist[(x, y)] + 1
                q.append(c)
    R = m['tuiles']
    far = list(extra) + [(o['x'], o['y']) for o in m['coffres'] if 'si' in o]  # et loin des autres trésors cachés
    for c, d in sorted(dist.items(), key=lambda e: -e[1]):
        if c in occ or R[c[1]][c[0]] not in '.,':
            continue
        if any(abs(c[0] - e[0]) + abs(c[1] - e[1]) < 12 for e in far):  # bien séparées, à l'écran aussi
            continue
        m['coffres'].append({"x": c[0], "y": c[1]})
        lost = ok - pu.reachable(m) - {c}
        m['coffres'].pop()
        if not lost:
            return c
    raise SystemExit('aucune place')


QUEST_NIGHT = {"drapeau": "quete:lanternes", "nuit": True}
spots = []
for i in range(5):
    c = far_cell(m, spots)
    spots.append(c)
    m['coffres'].append({"x": c[0], "y": c[1], "objet": "lanterne_fete", "quantite": 1, "si": QUEST_NIGHT})
p = pu.near_reach(m, 14, 12, ring=1)
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "villageoise", "direction": "bas", "evenement": "lison",
                       "si": {"drapeau": "boss2", "nuit": True}})
m['fete'] = {"drapeau": "fete_lanternes"}
lost = before - pu.reachable(m) - set((o['x'], o['y']) for o in m['coffres'] + m['habitants'])
if lost:
    sys.exit('vallee : des cases deviennent inaccessibles %s' % sorted(lost)[:5])
carte.save('vallee', m)


def say(t): return {"action": "dire", "texte": t}


EV = {'lison': {"pages": [
    {"si": {"drapeau": "fete_lanternes"}, "actions": [say("Lison : Regarde ! Elles montent encore, chaque nuit. La vallée n'a jamais été aussi belle.")]},
    {"si": {"drapeau": "quete:lanternes", "objet_min": {"lanterne_fete": 5}}, "actions": [
        {"action": "retirer", "objet": "lanterne_fete", "quantite": 5},
        say("Lison : Les cinq ! Tu les as toutes retrouvées ! Vite, allumons-les…"),
        say("Une à une, les lanternes s'élèvent au-dessus de la place. Les habitants sortent pour les regarder monter vers les étoiles."),
        {"action": "drapeau", "nom": "fete_lanternes"},
        say("Lison : Merci. C'est la première fête depuis que la brume est partie. Garde celle-ci : c'est la mienne, la plus vieille."),
        {"action": "donner", "objet": "lanterne_lison", "quantite": 1}, {"action": "quete", "id": "lanternes", "fin": True}]},
    {"si": {"drapeau": "quete:lanternes"}, "actions": [
        say("Lison : Les lanternes ne brillent que la nuit. Cherche dans les coins de la vallée, loin des maisons.")]},
    {"actions": [
        say("Lison : Bonsoir ! Je prépare la fête des lanternes, la première depuis que la brume s'est levée."),
        say("Lison : Mais le vent a emporté mes cinq lanternes de fête ! Elles brillent quelque part dans la vallée, la nuit…"),
        {"action": "question", "texte": "Chercher les cinq lanternes de Lison ?",
         "oui": [{"action": "quete", "id": "lanternes"}, say("Lison : Merci ! Elles ne se voient que la nuit. Reviens me voir quand tu les auras, de nuit aussi.")],
         "non": [say("Lison : Tant pis… La fête attendra encore un peu.")]}]}]}}
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
json.load(open(D + 'evenements.json', encoding='utf-8'))
Q = {"id": "lanternes", "nom": "La fête des lanternes", "lieu": "Vallée de Brumeval",
     "etapes": [{"texte": "Retrouver les cinq lanternes de Lison, dans les coins de la Vallée. Elles ne brillent que la nuit."},
                {"si": {"objet_min": {"lanterne_fete": 5}}, "texte": "Rapporter les cinq lanternes à Lison, sur la place de la Vallée, la nuit."}],
     "fin_texte": "Chaque nuit, des lanternes montent dans le ciel de la Vallée."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('lanterne_veilleur') + 1
OBJ[k:k] = [
    {"id": "lanterne_fete", "nom": "Lanterne de fête", "icone": "ic_lanterne", "prix": 0, "important": True,
     "description": "Une lanterne de papier rouge, emportée par le vent. Lison en a besoin pour la fête."},
    {"id": "lanterne_lison", "nom": "Lanterne de Lison", "icone": "ic_lanterne_or", "prix": 3000, "equipement": "accessoire",
     "description": "La plus vieille lanterne de la fête. Sa lumière rassure les créatures de la nuit.",
     "bonus": {"pv": 30, "magie": 5, "resistance": 5, "esquive": 3}}]
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
print('fête des lanternes : Lison en', p, ', lanternes en', spots)
