"""Équipement des actes 3 et 4 (étape « 20 heures », 2026-10-20) : les boutiques de Havre-d'Aube et de Port-Écho ne
vendaient que l'équipement de l'acte 2. Deux rangs de plus : lueur (Havre-d'Aube) et écho (Port-Écho), une arme et une
armure pour les combattants et pour les mages ; la forge de Forgeroc les renforce (+1 à +3) comme les autres.
Une seule fois (refuse si la Lame de lueur existe).

    python outils/villes/equipement_tardif.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
if any(o['id'] == 'lame_lueur' for o in OBJ):
    sys.exit('Déjà fait.')
NEW = [
    {"id": "lame_lueur", "nom": "Lame de lueur", "icone": "ic_epee_4", "prix": 3600, "equipement": "arme",
     "description": "Une lame forgée dans le Bois des Lueurs. Elle brille faiblement dans le noir.", "bonus": {"attaque": 18, "critique": 5}},
    {"id": "baton_lueur", "nom": "Bâton de lueur", "icone": "ic_baton_4", "prix": 3600, "equipement": "arme",
     "description": "Un bâton de bois pâle, coiffé d'un éclat turquoise.", "bonus": {"magie": 18, "pm": 16}},
    {"id": "armure_nacre", "nom": "Armure de nacre", "icone": "ic_armure_6", "prix": 3300, "equipement": "armure",
     "description": "Des plaques de nacre cousues sur du cuir. Légère, et plus solide qu'elle n'en a l'air.", "bonus": {"defense": 16, "resistance": 9, "pv": 20}},
    {"id": "robe_lueur", "nom": "Robe de lueur", "icone": "ic_armure_7", "prix": 3100, "equipement": "armure",
     "description": "Une robe tissée de fils qui gardent la lumière du jour.", "bonus": {"resistance": 15, "defense": 6, "pm": 12}},
    {"id": "lame_echo", "nom": "Lame d'écho", "icone": "ic_epee_5", "prix": 6200, "equipement": "arme",
     "description": "Une lame de pierre grise qui chante quand elle fend l'air.", "bonus": {"attaque": 22, "critique": 6, "vitesse": 2}},
    {"id": "baton_echo", "nom": "Bâton d'écho", "icone": "ic_baton_5", "prix": 6200, "equipement": "arme",
     "description": "Un bâton de pierre grise. Les sorts qu'on y murmure reviennent plus forts.", "bonus": {"magie": 22, "pm": 18, "vitesse": 2}},
    {"id": "armure_petrifiee", "nom": "Armure pétrifiée", "icone": "ic_armure_8", "prix": 5600, "equipement": "armure",
     "description": "Taillée dans l'écorce d'un arbre de pierre. Lourde, mais rien ne la traverse.", "bonus": {"defense": 20, "resistance": 11, "pv": 30, "vitesse": -1}},
    {"id": "robe_echo", "nom": "Robe d'écho", "icone": "ic_armure_7", "prix": 5300, "equipement": "armure",
     "description": "Les coquillages cousus dessus chantent quand on marche.", "bonus": {"resistance": 19, "defense": 8, "pm": 14}},
]
k = [o['id'] for o in OBJ].index('robe_orage') + 1
for i, o in enumerate(NEW):
    OBJ.insert(k + i, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
EV = json.load(open(D + 'evenements.json', encoding='utf-8'))
EV['aube_boutique']['actions'][0]['objets'] += ['lame_lueur', 'baton_lueur', 'armure_nacre', 'robe_lueur']
EV['echos_boutique']['actions'][0]['objets'] += ['lame_echo', 'baton_echo', 'armure_petrifiee', 'robe_echo']
carte.rewrite_events(['aube_boutique', 'echos_boutique'], EV)
print(len(NEW), 'équipements')
