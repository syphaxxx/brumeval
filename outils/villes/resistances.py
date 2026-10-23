"""Résistances des créatures (rééquilibrage du 2026-10-23, « obliger le joueur à adapter son équipe ») : comme dans
Clair Obscur : Expedition 33, toutes les créatures ne se battent plus de la même façon. Une équipe qui ne frappe que
d'une seule manière (physique, ou un seul type) doit changer ses combattants et sa formation selon l'adversaire.

- Carapaces : Roche ou Métal, et Défense bien au-dessus de la Résistance : physique ×0,6, magique ×1,2.
- Esprits : type Esprit, ou Ombre avec la Résistance bien au-dessus de la Défense : physique ×0,55, magique ×1,15.
- Peaux d'enchantement : Résistance bien au-dessus de la Défense (sans être un esprit) : magique ×0,6, physique ×1,15.
- Élémentaires : quelques formes évoluées absorbent leur propre élément (résistance -2, soit ×0,5 × -2 = -1 : le coup
  les soigne de tous ses dégâts).

Seulement les créatures sauvages (celles des zones des cartes, de jour ou de nuit) : ni les humains, ni les boss
(réglés un par un : compétence equilibrage), ni une résistance réglée à la main sur un autre type.
Relancer le script remet ces règles (les valeurs écrites par lui sont recalculées).

    python outils/villes/resistances.py
"""
import glob
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
SAUVAGES = set()
for f in glob.glob(D + 'cartes/*.json'):
    for z in json.load(open(f, encoding='utf-8')).get('zones', []):
        SAUVAGES.update(z.get('creatures', []) + z.get('nuit', []))
ABSORBE = {'fournezard', 'torrentin', 'fulgurin', 'givrenard', 'ronceflore', 'serpentox', 'orageon', 'banquisot', 'cendrelys', 'lanternelle'}
compte = {'carapace': 0, 'esprit': 0, 'enchantement': 0, 'absorbe': 0}
for e in ESP:
    if e.get('humain') or e['id'] not in SAUVAGES:
        continue
    types = e['types'] if 'types' in e else [e['type']]
    b = e['base']
    d, r = b['defense'], b.get('resistance', (b['defense'] + b['magie']) // 2)
    res = {}
    if any(t in ('roche', 'metal') for t in types) and d >= r + 14:
        res = {'physique': 0.6, 'magique': 1.2}
        compte['carapace'] += 1
    elif 'esprit' in types or ('ombre' in types and r >= d + 10):
        res = {'physique': 0.55, 'magique': 1.15}
        compte['esprit'] += 1
    elif r >= d + 14:
        res = {'magique': 0.6, 'physique': 1.15}
        compte['enchantement'] += 1
    if e['id'] in ABSORBE:
        res[types[0]] = -2
        compte['absorbe'] += 1
    old = e.get('resistances')
    if old and not set(old) <= {'physique', 'magique', *types}:  # réglée à la main (un autre type) : on n'y touche pas
        continue
    if res:
        e['resistances'] = res
    elif old is not None and set(old) <= {'physique', 'magique', *types}:
        del e['resistances']
carte.save('especes', ESP, sub='')
print(compte)
