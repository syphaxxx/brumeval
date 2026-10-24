"""Attaques sur toute l'équipe des créatures sauvages (rééquilibrage du 2026-10-24) : une meute de trois créatures qui
frappent chacune toute l'équipe écrasait une équipe de six, même avec 4 à 14 niveaux d'avance (70 défaites du joueur
automatique dans l'Abîme et le Vieux Phare, en trois parties ; la Braisaure, le Pétrolin et le Rocaube lançaient Séisme
ou Magma à chaque tour). Même règle que pour les apprentissages tardifs : une créature sauvage n'apprend jamais
d'attaque sur tous les ennemis. Elle reçoit à la place une attaque de zone (ligne, colonne ou croix) d'un de ses types
qu'elle ne connaît pas encore (la formation de l'équipe compte alors) ; s'il n'en reste aucune, la ligne est retirée.

Seulement les créatures sauvages (celles des zones des cartes, de jour ou de nuit) : les boss gardent leurs attaques sur
toute l'équipe (réglés un par un : compétence equilibrage) ; un boss fait d'une espèce sauvage (primes) suit l'espèce.
Relancer le script ne change plus rien une fois les données corrigées.

    python outils/villes/meutes.py
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
TECH = {t['id']: t for t in json.load(open(D + 'techniques.json', encoding='utf-8'))}
SAUVAGES = set()
for f in glob.glob(D + 'cartes/*.json'):
    for z in json.load(open(f, encoding='utf-8')).get('zones', []):
        SAUVAGES.update(z.get('creatures', []) + z.get('nuit', []))


def types(e):
    return e['types'] if 'types' in e else [e['type']]


def sur_tous(mid):
    t = TECH[mid]
    return t.get('cible') == 'tous_ennemis' and t.get('puissance', 0) > 0


# Attaques de zone « ordinaires » : connues d'au moins une créature sauvage (pas celles des héros ni des boss)
connues = {mid for e in ESP if e['id'] in SAUVAGES for _, mid in e['apprend']}
ZONE = sorted((t for t in TECH.values() if t.get('zone') in ('ligne', 'colonne', 'croix') and t.get('puissance', 0) > 0
               and t['id'] in connues), key=lambda t: (-t['puissance'], t.get('cout', 0)))

changes = 0
for e in ESP:
    if e.get('humain') or e['id'] not in SAUVAGES or not any(sur_tous(m) for _, m in e['apprend']):
        continue
    avant = [m for _, m in e['apprend']]
    nouveau, pris = [], set(avant)
    for lvl, mid in e['apprend']:
        if not sur_tous(mid):
            nouveau.append([lvl, mid])
            continue
        choix = next((t['id'] for t in ZONE if t['type'] in types(e) and t['id'] not in pris), None)
        print('%-12s N.%-3d %-18s → %s' % (e['id'], lvl, mid, choix or '(retirée)'))
        if choix:
            nouveau.append([lvl, choix])
            pris.add(choix)
        changes += 1
    e['apprend'] = nouveau
    if sum(TECH[m].get('puissance', 0) > 0 for _, m in nouveau) < 2:
        print('   attention : %s n\'a plus que %d attaque(s)' % (e['id'], sum(TECH[m].get('puissance', 0) > 0 for _, m in nouveau)))
carte.save('especes', ESP, sub='')
print(changes, 'attaques sur toute l\'équipe remplacées ou retirées')
