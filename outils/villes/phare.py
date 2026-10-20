"""Le Vieux Phare (étape « 20 heures », 2026-10-20) : un donjon de cinq étages sous les ruines du vieux phare de
Port-Écho, après l'acte 4 (`echos_fin`), au-delà de l'Abîme : créatures de N.90 à N.102, un escalier par étage, et au
sommet le Veilleur du phare (boss recoloré, N.110, trois phases). Quête `phare`, drapeau `phare5_boss`, Lanterne du
veilleur. Une seule fois (refuse si data/cartes/phare1.json existe) ; puis outils/sprites/decoupe.py.

    python outils/villes/phare.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util as pu  # noqa: E402
from cartegen import dungeon, finish  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/phare1.json'):
    sys.exit('Le Vieux Phare existe déjà.')
N = 5
POOL = ['spectrecho', 'echoplume', 'fossiloup', 'braisaure', 'petrolin', 'cendrelys', 'aubespectre', 'loup_aube']
floors = []
for k in range(1, N + 1):
    F, ex, br, oth = dungeon('phare%d' % k, "Le Vieux Phare (étage %d)" % k, 'grotte', 40, 32, 80 + k, '.', 'c', nrooms=9 + k % 2,
                             puzzle=(k == 3), decor='xR')
    F.m['ambiance'] = {"effet": "obscurite" if k % 2 else "lucioles"}
    floors.append((F, ex, br, oth))
# l'entrée : devant les ruines du vieux phare, à Port-Écho
E = carte.load('echos')
before = pu.reachable(E)
gx, gy = pu.near_reach(E, 37, 21, ring=1)
rows = [list(r) for r in E['tuiles']]
rows[gy][gx] = '='
E['tuiles'] = [''.join(r) for r in rows]
E['passages'].append({"x": gx, "y": gy, "vers": "phare1", "tx": floors[0][1], "ty": 30, "direction": "haut", "condition": {"drapeau": "echos_fin"},
                      "message": "Sous les ruines du vieux phare, un escalier descend dans le noir. Une voix murmure : « Pas encore… »"})
lost = before - pu.reachable(E) - {(gx, gy)}
if lost:
    sys.exit('echos : des cases deviennent inaccessibles %s' % sorted(lost)[:5])
for k, (F, ex, br, oth) in enumerate(floors, start=1):
    lv = (88 + 2 * k, 92 + 2 * k, 3, POOL[(k - 1) % 3:(k - 1) % 3 + 5])  # 94-98 au départ : 46 défaites du joueur automatique
    back = ('echos', gx, gy + 1) if k == 1 else ('phare%d' % (k - 1), floors[k - 2][2][0] + floors[k - 2][2][2] // 2, floors[k - 2][2][1] + 2)
    if k < N:  # un escalier dans la salle la plus loin mène à l'étage suivant
        sx, sy = br[0] + br[2] // 2, br[1] + 1
        F.rect(br[0], br[1], br[0] + br[2] - 1, br[1] + br[3] - 1, '.')
        F.put(sx, sy, 'k')
        F.warp(sx, sy, 'phare%d' % (k + 1), floors[k][1], 30, 'haut')
        loot = [[('elixir', 2), 'gemme_vie', ('or', 10000), 'plume'], ['gemme_force', ('elixir', 2), ('or', 12000)],
                ['gemme_esprit', ('or', 14000), ('elixir', 3)], ['gemme_maitre', ('or', 16000), ('elixir', 3), 'plume']][k - 1]
        for (x, y, rw, rh), item in zip(oth, loot):
            F.chest(x + rw - 2, y + 1, *(item if isinstance(item, tuple) else (item,)))
        F.warp(ex, 31, *back, 'bas'), F.warp(ex + 1, 31, *back, 'bas')
        F.zone(0, 0, 40, 32, *lv)
    else:
        finish(F, ex, br, oth, 'veilleur_phare', [('elixir', 4), 'gemme_maitre', ('or', 30000), 'gemme_maitre'], back, None, None, lv)
for F, *_ in floors:
    F.save()
carte.save('echos', E)

# Le boss : le Veilleur du phare (recoloration du Gardien des Lueurs), la récompense, l'événement, la quête
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
ESP.append({"id": "veilleur_phare", "nom": "Veilleur du phare", "sprite": "veilleur_phare", "types": ["lumiere", "esprit"],
            "base": {"pv": 130, "pm": 70, "attaque": 80, "defense": 80, "magie": 90, "resistance": 82, "vitesse": 62},
            "immunites": ["sommeil"], "forme": "boss", "couleurs": ["#e0b040", "#fff8d0"],
            "apprend": [[1, "prisme"], [1, "nova"], [1, "cauchemar"], [1, "lame_ombre"], [1, "soins"]], "talent": "esprit_clair"})
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
V = os.path.join(os.path.dirname(ICI), 'sprites', 'variantes.json')
vt = open(V, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in vt else '\n'
vt = vt.rstrip()[:-1].rstrip() + ',' + nl + '  ' + json.dumps({"id": "veilleur_phare", "de": "gardien_lueurs", "teintes": [[-1, 360, 44, 1.2, 1.15]]},
                                                              ensure_ascii=False) + nl + ']' + nl
open(V, 'w', encoding='utf-8', newline='').write(vt)
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('medaille_eleveuse') + 1
OBJ.insert(k, {"id": "lanterne_veilleur", "nom": "Lanterne du veilleur", "icone": "ic_lanterne_or", "prix": 12000, "equipement": "accessoire",
               "description": "La lanterne du dernier veilleur du phare. Sa flamme ne s'éteint jamais, même au fond de l'Abîme.",
               "bonus": {"pv": 100, "magie": 10, "resistance": 10, "vitesse": 5, "critique": 4}})
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')


def say(t): return {"action": "dire", "texte": t}


EV = {"phare5_boss": {"actions": [
    say("Au sommet du phare, une lanterne immense brille dans le noir. Une silhouette dorée se tient devant elle."),
    say("Veilleur du phare : Mille ans que je garde cette lumière. Mille ans que personne ne monte jusqu'ici. Montrez-moi que vous la méritez."),
    {"action": "question", "texte": "Affronter le Veilleur du phare ?", "oui": [
        {"action": "combat", "musique": "boss_final", "ennemis": [
            {"espece": "spectrecho", "niveau": 104},
            {"espece": "veilleur_phare", "niveau": 110, "pv": 9, "boss": True, "bouclier": 18,
             "phases": [{"pv": 0.7, "message": "La lanterne s'embrase : le Veilleur brille plus fort !", "bonus": {"magie": 1}},
                        {"pv": 0.4, "message": "Le Veilleur appelle les échos du phare !", "bouclier": 14,
                         "renforts": [{"espece": "fossiloup", "niveau": 104}]},
                        {"pv": 0.15, "message": "La lumière vacille… le Veilleur rassemble ses dernières forces !", "bonus": {"attaque": 1, "vitesse": 1}}]},
            {"espece": "petrolin", "niveau": 104}], "boss": True, "fuite": False, "capture": False,
         "victoire": [{"action": "drapeau", "nom": "phare5_boss"},
                      say("Le Veilleur baisse la tête. Sa lumière devient douce, comme une bougie."),
                      say("Veilleur du phare : Vous êtes les premiers. Prenez ma lanterne : qu'elle éclaire votre route, partout où vous irez."),
                      {"action": "donner", "objet": "lanterne_veilleur", "quantite": 1}, {"action": "quete", "id": "phare", "fin": True}]}],
     "non": [say("La lanterne tourne lentement. Le Veilleur attend.")]}]},
    "phare_sonneur": {"pages": [
        {"si": {"drapeau": "phare5_boss"}, "actions": [say("Gardien des ruines : Le Veilleur vous a donné sa lanterne ? Alors le phare est en paix.")]},
        {"si": {"drapeau": "echos_fin"}, "actions": [
            say("Gardien des ruines : Sous le vieux phare, cinq étages descendent dans le noir. Tout en bas… ou tout en haut, on ne sait plus… veille le gardien de la lumière."),
            say("Gardien des ruines : Les créatures y sont plus fortes que partout ailleurs. Même l'Abîme, à côté, c'est une promenade."),
            {"action": "quete", "id": "phare"}]},
        {"actions": [say("Gardien des ruines : Le vieux phare est fermé. Tant que l'écho gronde, personne n'y descend.")]}]}}
m = carte.load('echos')
p = pu.near_reach(m, gx + 2, gy, ring=1)
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "garde", "direction": "gauche", "evenement": "phare_sonneur"})
carte.save('echos', m)
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
json.load(open(D + 'evenements.json', encoding='utf-8'))
Q = {"id": "phare", "nom": "Le Vieux Phare", "lieu": "Port-Écho",
     "etapes": [{"texte": "Descendre les cinq étages du Vieux Phare, sous les ruines de Port-Écho, et affronter le Veilleur du phare."}],
     "fin_texte": "Le Veilleur du phare vous a confié sa lanterne."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
print('Vieux Phare écrit : entrée en', (gx, gy), 'gardien en', p)
