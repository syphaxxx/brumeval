"""Intérieurs des nouveaux villages (étape « 20 heures », 2026-10-20) : Moulinbourg, Port d'Écume, Havre-d'Aube, l'île de
Corail et Port-Écho avaient des bâtiments sans intérieur (la porte lançait directement l'événement). Chaque bâtiment
(sauf décor et sanctuaire d'Aube) reçoit une carte intérieure, sur le modèle de ceux de la Vallée et de Forgeroc : le
service (soins, boutique, auberge) passe à un habitant de l'intérieur ; une maison qui n'avait qu'une description la
montre quand on fait le premier pas dedans. Une seule fois (refuse si data/cartes/plaine_soin.json existe).

    python outils/villes/interieurs_nouveaux.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/plaine_soin.json'):
    sys.exit('Déjà fait.')
EV = json.load(open(D + 'evenements.json', encoding='utf-8'))
MODELE = {'soin': ('vallee_soin', 'docteur'), 'boutique': ('vallee_boutique', 'marchande'), 'auberge': ('cendrelune_auberge', 'barman'),
          'maison': ('cendrelune_maison', 'villageoise')}
VILLAGES = ['plaine', 'port', 'aube', 'corail', 'echos']
SAUF = {'epreuves'}  # la porte du sanctuaire d'Aube reste un événement


def kind(genre):
    return 'boutique' if genre.startswith('boutique') else genre


def only_text(ev):
    acts = ev.get('actions') if isinstance(ev, dict) else None
    return acts is not None and 'pages' not in ev and all(a.get('action') == 'dire' for a in acts)


made = []
for mid in VILLAGES:
    V = carte.load(mid)
    used = set()
    for b in V['batiments']:
        k = kind(b['genre'])
        if k not in MODELE or b.get('interieur') or not b.get('evenement') or b['evenement'] in SAUF:
            continue
        iid = '%s_%s' % (mid, k)
        n = 2
        while iid in used:
            iid = '%s_%s%d' % (mid, k, n)
            n += 1
        used.add(iid)
        tpl, look = MODELE[k]
        I = carte.load(tpl)
        I['id'], I['nom'] = iid, b['nom']
        ev = b.pop('evenement')
        b['interieur'] = iid
        dx, dy = b['x'] + b['l'] // 2, b['y'] + b['h']
        I['passages'] = [{"x": 5, "y": 8, "vers": mid, "tx": dx, "ty": dy, "direction": "bas"}]
        I['coffres'], I['panneaux'], I['zones'], I['boss'] = [], [], [], []
        if k == 'maison' and only_text(EV.get(ev, {})):
            I['habitants'] = []
            I['declencheurs'] = [{"x": 5, "y": 6, "l": 1, "h": 1, "evenement": ev}]  # la description, au premier pas
        else:
            I['habitants'] = [{"x": 5, "y": 3 if k != 'maison' else 4, "apparence": look, "evenement": ev, "direction": "bas"}]
            I.pop('declencheurs', None)
        carte.save(iid, I)
        made.append((iid, b['nom'], ev))
    carte.save(mid, V)
for m in made:
    print('%-16s %-30s %s' % m)
print(len(made), 'intérieurs')
