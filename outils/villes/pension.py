"""La pension de Moulinbourg (étape « 20 heures », 2026-10-20) : Mamie Rose garde deux créatures (src/pension.cpp,
action « pension ») ; un enclos de clôture et deux bêtes devant chez elle. Une seule fois (refuse si l'événement
« pension » existe).

    python outils/villes/pension.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util as pu  # noqa: E402

D = carte.R
EVS = json.load(open(D + 'evenements.json', encoding='utf-8'))
if 'pension' in EVS:
    sys.exit('Déjà fait.')


def say(t): return {"action": "dire", "texte": t}


m = carte.load('plaine')
before = pu.reachable(m)
p = pu.near_reach(m, 10, 13, ring=1)
assert p
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "villageoise", "direction": "bas", "evenement": "pension"})
for dx, look in ((-3, 'vache'), (3, 'poule')):  # des bêtes autour de la pension
    q = pu.near_reach(m, p[0] + dx, p[1] + 1, ring=1)
    if q:
        m['habitants'].append({"x": q[0], "y": q[1], "apparence": look, "direction": "gauche", "dialogue": ["…"], "promenade": 2})
if before - pu.reachable(m) - set((h['x'], h['y']) for h in m['habitants']):
    sys.exit('plaine : des cases deviennent inaccessibles')
carte.save('plaine', m)

EV = {'pension': {"pages": [
    {"si": {"drapeau": "pension_vue"}, "actions": [say("Mamie Rose : Bonjour, mon petit ! Tu viens voir tes protégés ?"), {"action": "pension"}]},
    {"actions": [
        say("Mamie Rose : Bienvenue à la pension de Moulinbourg ! Ici, je garde les créatures de ceux qui voyagent."),
        say("Mamie Rose : Laisse-m'en deux, et elles grandiront à chacun de tes pas. Je leur fais courir les champs, tu sais."),
        say("Mamie Rose : Et si elles ont un type en commun… il arrive qu'on trouve un œuf au matin. Ne me demande pas comment !"),
        {"action": "drapeau", "nom": "pension_vue"}, {"action": "pension"}]}]}}
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
json.load(open(D + 'evenements.json', encoding='utf-8'))
ex = open(D + 'exploits.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in ex else '\n'
e = {"id": "oeuf", "nom": "Un petit nouveau", "description": "Faire éclore un œuf de la pension de Moulinbourg.", "si": {"drapeau": "oeuf_eclos"}}
open(D + 'exploits.json', 'w', encoding='utf-8', newline='').write(ex.rstrip()[:-1].rstrip() + ',' + nl + '  ' + json.dumps(e, ensure_ascii=False) + nl + ']' + nl)
print('pension en', p)
