"""Réglage de l'acte 4 après le joueur automatique (2026-10-20) : l'Ombre du Premier Gardien le battait en moins d'une
minute même avec 17 niveaux d'avance (Éclipse sur toute l'équipe, Foudre noire, Charge héroïque, deux Spectréchos
soigneurs) : techniques plus mesurées, un seul soigneur, pas de renfort de départ. Le Phénix d'écho (4 défaites) :
moins de PV et pas de renfort de départ. Une seule fois (refuse si l'Ombre n'a plus « charge_heroique »).

    python outils/villes/acte4_reglage.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
og = next(e for e in ESP if e['id'] == 'ombre_gardien')
if not any(m == 'charge_heroique' for _, m in og['apprend']):
    sys.exit('Déjà fait.')
og['apprend'] = [[1, "lame_ombre"], [1, "prisme"], [1, "nova"], [1, "cauchemar"]]  # 3e mesure : rien qui frappe toute l'équipe
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
EV = json.load(open(D + 'evenements.json', encoding='utf-8'))


def fight(ev):
    for a in ev['actions']:
        if a['action'] == 'question':
            return a['oui'][0]


f = fight(EV['sanctuaire_echos_boss'])
f['ennemis'][0] = {"espece": "fossiloup", "niveau": 90}   # 3e mesure : plus d'Échoplumes (Ouragan sur toute l'équipe)
f['ennemis'][2] = {"espece": "petrolin", "niveau": 90}
f['renforts'] = []
f['ennemis'][1]['phases'][1]['renforts'] = [{"espece": "spectrecho", "niveau": 90}]
p = fight(EV['phenix_echo'])
p['ennemis'][1]['pv'] = 4.5
p['ennemis'][1]['niveau'] = 87                                      # 2e mesure : encore 4 défaites
p['ennemis'][1]['phases'][0] = {"pv": 0.6, "message": "Le Phénix bat des ailes : l'air du cratère devient brûlant !", "bonus": {"magie": 1}}
p['renforts'] = []
carte.rewrite_events(['sanctuaire_echos_boss', 'phenix_echo'], EV)
# 4e mesure (simulation N.92 : 60 %, Ouragan sur toute l'équipe à chaque tour) : Phénix sans Ouragan
ph = next(e for e in ESP if e['id'] == 'phenix_echo')
ph['apprend'] = [[1, "pyro"], [1, "lame_air"], [1, "brasier"], [1, "fleche_vent"], [1, "soins"]]
open(D + 'especes.json', 'w', encoding='utf-8', newline='
').write(carte.pretty(ESP) + '
')
# 5e mesure (faite à la main dans evenements.json) : le cratère a souvent la météo Canicule (feu +25 %) ; le Phénix est
# escorté d'un Pétrolin et d'un Fossiloup (N.86) au lieu de deux créatures de feu, son renfort aussi.
print("Acte 4 réglé : Ombre du Premier Gardien et Phénix d'écho")
