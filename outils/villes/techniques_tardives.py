"""Techniques tardives des héros (étape « 20 heures », 2026-10-20) : les héros n'apprenaient plus rien après le niveau 27
environ, alors que l'aventure va au-delà du niveau 100. Deux techniques de plus pour chacun, aux niveaux 35 et 60,
dans son rôle (Lior : coups puissants ; Maëlle : soins et lumière ; Brann : rangs et poussées ; Isra : sorts de zone ;
Kael : tir à distance ; Sélène : protection et bond). Une seule fois (refuse si « lame_aurore » existe).

    python outils/villes/techniques_tardives.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
TECH = json.load(open(D + 'techniques.json', encoding='utf-8'))
if any(t['id'] == 'lame_aurore' for t in TECH):
    sys.exit('Déjà fait.')
NEW = {
    'lior': [
        (35, {"id": "lame_aurore", "nom": "Lame d'aurore", "type": "normal", "genre": "physique", "cible": "ennemi", "puissance": 95,
              "precision": 92, "critique": 12, "rythme": "lourde", "description": "La lame s'illumine : un coup qui ne pardonne pas."}),
        (60, {"id": "coupe_gardien", "nom": "Coupe du gardien", "type": "normal", "genre": "physique", "cible": "ennemi", "puissance": 70,
              "zone": "ligne", "effet": {"stat": "defense", "niveaux": -1, "chance": 40}, "rythme": "lourde",
              "description": "Frappe toute une ligne d'ennemis et fend parfois leurs armures."})],
    'maelle': [
        (35, {"id": "aube_soignante", "nom": "Aube soignante", "type": "lumiere", "genre": "soin", "cible": "tous_allies", "puissance": 34,
              "cout": 16, "description": "Une lumière douce comme le matin : soigne toute l'équipe."}),
        (60, {"id": "lumiere_sacree", "nom": "Lumière sacrée", "type": "lumiere", "genre": "magique", "cible": "ennemi", "puissance": 70,
              "zone": "croix", "cout": 16, "rythme": "lourde", "description": "Une colonne de lumière s'abat sur la cible et ses voisins."})],
    'brann': [
        (35, {"id": "marteau_seisme", "nom": "Marteau-séisme", "type": "normal", "genre": "physique", "cible": "ennemi", "puissance": 70,
              "zone": "colonne", "deplace": "pousse", "rythme": "lourde", "description": "Un coup de marteau qui fait trembler tout un rang et le repousse."}),
        (60, {"id": "coup_forge", "nom": "Coup de forge", "type": "feu", "genre": "physique", "cible": "ennemi", "puissance": 100, "precision": 85,
              "critique": 10, "rythme": "lourde", "description": "Le marteau, chauffé à blanc, frappe de toute la force de Forgeroc."})],
    'isra': [
        (35, {"id": "ombre_devorante", "nom": "Ombre dévorante", "type": "ombre", "genre": "magique", "cible": "ennemi", "puissance": 66,
              "zone": "croix", "cout": 14, "effet": {"stat": "magie", "niveaux": -1, "chance": 30},
              "description": "Une ombre qui mord la cible et ses voisins, et affaiblit parfois leur magie."}),
        (60, {"id": "meteore", "nom": "Météore", "type": "feu", "genre": "magique", "cible": "ennemi", "puissance": 82, "zone": "croix",
              "cout": 20, "rythme": "lourde", "description": "Une pierre de feu tombe du ciel au milieu des ennemis."})],
    'kael': [
        (35, {"id": "fleche_percante", "nom": "Flèche perçante", "type": "vent", "genre": "physique", "cible": "ennemi", "puissance": 80,
              "portee": "distance", "precision": 95, "critique": 20, "description": "Une flèche tirée de loin, qui trouve le défaut de l'armure."}),
        (60, {"id": "tempete_fleches", "nom": "Tempête de flèches", "type": "vent", "genre": "physique", "cible": "tous_ennemis", "puissance": 58,
              "portee": "distance", "rythme": "lourde", "description": "Une volée de flèches s'abat sur tous les ennemis."})],
    'selene': [
        (35, {"id": "egide", "nom": "Égide", "type": "metal", "genre": "statut", "cible": "tous_allies", "puissance": 0,
              "effet": {"stat": "resistance", "niveaux": 1}, "description": "Un bouclier de lumière : Résistance +1 pour toute l'équipe."}),
        (60, {"id": "lance_celeste", "nom": "Lance céleste", "type": "metal", "genre": "physique", "cible": "ennemi", "puissance": 100,
              "precision": 92, "rythme": "lourde", "mouvement": "bond", "description": "Sélène bondit par-dessus la cible et la frappe de haut."})],
}
for lst in NEW.values():
    for _, t in lst:
        TECH.append(t)
open(D + 'techniques.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(TECH) + '\n')
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
for e in ESP:
    for lvl, t in NEW.get(e['id'], []):
        e['apprend'].append([lvl, t['id']])
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
print(sum(len(v) for v in NEW.values()), 'techniques')
