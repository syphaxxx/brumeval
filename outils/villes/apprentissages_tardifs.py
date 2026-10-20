"""Apprentissages tardifs des créatures (étape « 20 heures », 2026-10-20) : la plupart des créatures n'apprenaient plus
rien après le niveau 12 à 38. Chaque créature (ni humaine ni boss) qui n'apprend rien après le niveau 45 reçoit deux
techniques de son type, aux niveaux 45 et 65 : les plus puissantes qu'elle ne connaît pas encore, physiques ou magiques
selon sa meilleure statistique (Attaque ou Magie), sans les Limites, les duos ni les techniques propres aux héros.
Une seule fois (refuse si le Mulotin apprend déjà quelque chose au niveau 45).

    python outils/villes/apprentissages_tardifs.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
TECH = {t['id']: t for t in json.load(open(D + 'techniques.json', encoding='utf-8'))}
if any(l == 45 for l, _ in next(e for e in ESP if e['id'] == 'mulotin')['apprend']):
    sys.exit('Déjà fait.')
HEROS = {m for e in ESP if e.get('humain') for _, m in e['apprend']}  # techniques des héros : elles leur restent
# pas d'attaque sur tous les ennemis : une meute sauvage qui en a plusieurs écrase l'équipe (mesuré : 60 défaites)
CANDIDATS = [t for t in TECH.values() if t.get('puissance', 0) >= 50 and t.get('genre') in ('physique', 'magique') and t.get('cible') != 'tous_ennemis'
             and not t['id'].startswith(('lim_', 'duo_')) and t['id'] not in HEROS]
n = 0
for e in ESP:
    if e.get('humain') or e.get('forme') == 'boss' or max(l for l, _ in e['apprend']) >= 45:
        continue
    types = e['types'] if 'types' in e else [e['type']]
    genre = 'physique' if e['base']['attaque'] >= e['base']['magie'] else 'magique'
    known = {m for _, m in e['apprend']}
    pool = sorted((t for t in CANDIDATS if t['type'] in types and t['id'] not in known),
                  key=lambda t: (t['genre'] != genre, t['type'] != types[0], -t['puissance'], t['id']))
    if len(pool) < 2:  # pas assez dans ses types : une technique normale de son genre
        pool += sorted((t for t in CANDIDATS if t['type'] == 'normal' and t['genre'] == genre and t['id'] not in known and t not in pool),
                       key=lambda t: (-t['puissance'], t['id']))
    for lvl, t in zip((45, 65), pool[:2]):
        e['apprend'].append([lvl, t['id']])
        n += 1
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
print(n, 'apprentissages')
