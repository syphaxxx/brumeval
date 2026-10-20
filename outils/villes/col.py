"""Le Col des Hurlevents (étape « 20 heures », 2026-10-20) : une région de l'histoire entre les Monts Cendrelune et les
Pics Givrés. Après Ignarok, le chemin du nord passe par ce col venteux, son camp de bergers et la Harpie de givre qui
souffle le vent glacé sur les Pics. Une seule fois (refuse si data/cartes/col.json existe).

    python outils/villes/col.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
from cartegen import Map  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/col.json'):
    sys.exit('Le col existe déjà.')

W, H = 40, 52
C = Map('col', "Col des Hurlevents", 'cendres', W, H, 'm', 60)
C.m['ambiance'] = {"effet": "neige"}
# le chemin qui serpente vers le nord, avec des replats d'herbes sèches (rencontres)
pts = [(19, 51), (19, 44), (11, 44), (11, 36), (24, 36), (24, 27), (14, 27), (14, 18), (27, 18), (27, 11), (19, 11), (19, 0)]
C.path(pts, '=', 2)
for cx, cy, r in [(8, 46, 4), (30, 44, 4), (6, 33, 4), (31, 32, 4), (9, 24, 3), (32, 23, 4), (8, 14, 4), (33, 13, 3)]:
    C.blob(cx, cy, r, 'g', only='m')
for cx, cy, r in [(19, 40, 4), (24, 31, 3), (20, 22, 3)]:
    C.blob(cx, cy, r, 'a', only='m')
C.path([(11, 44), (8, 44), (8, 46)], 'a', 1)
C.path([(24, 36), (30, 36), (30, 32)], 'a', 1)
C.path([(14, 27), (9, 27), (9, 24)], 'a', 1)
C.path([(27, 18), (32, 18), (32, 23)], 'a', 1)
C.path([(14, 18), (8, 18), (8, 14)], 'a', 1)
# le camp des bergers, au milieu du col
C.rect(14, 28, 24, 34, 'a')
C.put(19, 31, 'X')
C.put(15, 29, 'C'), C.put(16, 29, 'C'), C.put(23, 29, 'v')
C.sprinkle(30, 'R', 'g', 2, 2, 37, 49, away=1)
C.sprinkle(12, 'd', 'ag', 2, 2, 37, 49, away=1)
# le défilé du nord : deux cases de large, la Harpie le bouche
for y in range(2, 9):
    for x in range(16, 23):
        if x not in (19, 20):
            C.put(x, y, 'm')
C.path([(19, 9), (19, 2)], '=', 2)
C.boss(19, 4, 'harpie_givre', 'harpie', 'harpie_givre')
C.npc(17, 30, 'docteur', 'droite', 'soin_col')
C.npc(22, 32, 'marchande', 'gauche', 'boutique_col')
C.npc(20, 34, 'paysan', 'haut', 'berger_col')
C.npc(18, 33, 'enfant', 'droite', lines=["Les moutons ne sortent plus : le vent les emporterait !", "Papa dit que c'est la Harpie qui souffle."])
C.npc(21, 10, 'chevalier', 'bas', lines=["La Harpie de givre niche juste au-dessus. Tant qu'elle souffle, personne ne passe.", "Sélène, de Givreval, est venue jusqu'ici avant de rebrousser chemin."])
C.chest(8, 45, 'superpotion', 2), C.chest(31, 32, 'cristal_givre'), C.chest(9, 23, 'gemme_vie'), C.chest(32, 22, 'or', 1500), C.chest(8, 13, 'elixir')
C.zone(0, 0, W, H, 22, 26, 3, ['voltigeon', 'glaconnet', 'zephyrin', 'galetor', 'givrelin'])
C.warp(19, 51, 'cendrelune', 13, 1, 'bas'), C.warp(20, 51, 'cendrelune', 13, 1, 'bas')
C.warp(19, 0, 'pics', 30, 42, 'haut', {"drapeau": "harpie"}, "La Harpie de givre barre le défilé.")
C.warp(20, 0, 'pics', 31, 42, 'haut', {"drapeau": "harpie"}, "La Harpie de givre barre le défilé.")
C.m['declencheurs'] = [{"x": 19, "y": 48, "l": 2, "h": 2, "evenement": "col_entree", "jusqua": "col_vu"}]
C.m['carte_monde'] = [68, 38]
r = C.reach((19, 50))
for p in [(8, 44), (31, 31), (9, 22), (32, 21), (8, 12), (19, 9), (17, 29)]:
    if p not in r:
        C.show()
        sys.exit('col : case inaccessible %s' % (p,))
C.save()

# Les Monts Cendrelune et les Pics mènent au col (au lieu de l'un à l'autre)
M = carte.load('cendrelune')
for w in M['passages']:
    if w['vers'] == 'pics':
        w['vers'], w['tx'], w['ty'] = 'col', 19, 50
        w['condition'] = {"drapeau": "boss2"}
carte.save('cendrelune', M)
P = carte.load('pics')
for w in P['passages']:
    if w['vers'] == 'cendrelune':
        w['vers'], w['tx'], w['ty'] = 'col', w['x'] - 11, 1
carte.save('pics', P)

# La Harpie (variante recolorée du griffon)
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
ESP.append({"id": "harpie_givre", "nom": "Harpie de givre", "sprite": "harpie_givre", "types": ["vent", "glace"],
            "base": {"pv": 88, "pm": 40, "attaque": 60, "defense": 50, "magie": 62, "resistance": 54, "vitesse": 72},
            "immunites": ["sommeil"], "forme": "oiseau", "couleurs": ["#c8e8ff", "#ffffff"],
            "apprend": [[1, "rafale"], [1, "eclat_glace"], [1, "lame_air"], [1, "poudreuse"]], "talent": "vif"})
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
V = os.path.join(os.path.dirname(ICI), 'sprites', 'variantes.json')
vt = open(V, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in vt else '\n'
open(V, 'w', encoding='utf-8', newline='').write(vt.rstrip()[:-1].rstrip() + ',' + nl + '  ' + json.dumps(
    {"id": "harpie_givre", "de": "griffon", "teintes": [[20, 70, 200, 0.5, 1.3], [200, 280, 190, 0.6, 1.4]]}, ensure_ascii=False) + nl + ']' + nl)

OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('braise_ignarok') + 1
OBJ.insert(k, {"id": "plume_harpie", "nom": "Plume de harpie", "description": "Trophée : une plume de givre qui ne fond jamais, prise à la Harpie du col.", "prix": 0, "trophee": True})
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')

R = json.load(open(D + 'regles.json', encoding='utf-8'))
cons = R['conseils']
i = [c['drapeau'] for c in cons].index('boss2') + 1
cons.insert(i, {"drapeau": "harpie", "boss": "harpie_givre", "lieu": "le défilé du Col des Hurlevents", "gemme": "gemme_feu",
                "texte": "Très rapide : elle joue souvent deux fois de suite. À mi-vie, son blizzard gèle le terrain."})
open(D + 'regles.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(R) + '\n')


def say(t): return {"action": "dire", "texte": t}


EV = {
    "col_entree": {"actions": [{"action": "drapeau", "nom": "col_vu"},
                               say("Le vent hurle entre les falaises. Des flocons volent, alors que le volcan fume encore en bas."),
                               say("Un col étroit monte vers le nord. Quelque part là-haut, quelque chose souffle ce vent glacé.")]},
    "soin_col": {"actions": [{"action": "soigner"}, {"action": "reveil"}, say("Guérisseuse : Une tisane chaude, et en route ! Votre équipe est en pleine forme.")]},
    "boutique_col": {"actions": [{"action": "boutique", "objets": ["potion", "superpotion", "ether", "remede", "plume", "lanterne_argent",
                                                                    "cotte_mailles", "robe_mage", "bague_vif", "gemme_feu", "gemme_givre", "gemme_force"]}]},
    "berger_col": {"pages": [
        {"si": {"drapeau": "harpie"}, "actions": [say("Berger : Le vent est tombé ! Mes moutons broutent de nouveau. Merci, voyageurs.")]},
        {"actions": [say("Berger : Depuis qu'Ignarok est tombé, une Harpie de givre a fait son nid au-dessus du col."),
                     say("Berger : Elle souffle le vent glacé sur les Pics. C'est elle qui a gelé Givreval, j'en suis sûr."),
                     say("Berger : Reposez-vous au camp avant d'y monter. Elle est rapide comme l'éclair.")]}]},
    "harpie_givre": {"actions": [
        say("Sur un rocher, une immense harpie aux plumes de givre déploie ses ailes. Le vent redouble."),
        say("Harpie de givre : Le volcan s'est tu… Maintenant, le froid règne. Personne ne passera mon col !"),
        {"action": "question", "texte": "Affronter la Harpie de givre ?", "oui": [
            {"action": "combat", "ennemis": [
                {"espece": "zephyrin", "niveau": 29},
                {"espece": "harpie_givre", "niveau": 31, "pv": 5, "boss": True, "bouclier": 6,
                 "phases": [{"pv": 0.5, "message": "La Harpie déchaîne un blizzard !", "meteo": "blizzard", "bonus": {"vitesse": 1}}]},
                {"espece": "glaconnet", "niveau": 29}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [{"action": "drapeau", "nom": "harpie"}, {"action": "donner", "objet": "plume_harpie", "quantite": 1},
                          say("La Harpie s'envole vers le nord en poussant un dernier cri. Le vent tombe d'un coup."),
                          say("Au bout du défilé, on aperçoit les toits enneigés de Givreval.")]}],
         "non": [say("La Harpie vous fixe, immobile, au milieu du vent.")]}]},
}
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
print('col écrit')
