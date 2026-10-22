"""L'île de Corail et la Grotte des Marées (étape « 20 heures », 2026-10-20) : une étape de l'acte 2 entre le Récif
d'Écume et les Ruines englouties. Les Ruines sont scellées : il faut le Trident d'écume, gardé au fond de la Grotte
des Marées par le Gardien des marées. Une seule fois (refuse si data/cartes/corail.json existe).

    python outils/villes/corail.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
from cartegen import Map, dungeon, finish  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/corail.json'):
    sys.exit("L'île de Corail existe déjà.")

# ------------------------------------------------------------------------------------------------ le village
W, H = 40, 30
K = Map('corail', "Île de Corail", 'vallee', W, H, '~', 70)
K.blob(20, 15, 12, '.', only='~')
K.blob(14, 12, 9, '.', only='~')
K.blob(27, 17, 9, '.', only='~')
K.rect(0, 14, 7, 15, 'B')                                   # le ponton qui vient du Récif
K.rect(15, 2, 26, 6, '#')                                   # les falaises du nord et la grotte
K.put(20, 6, 'k'), K.put(21, 6, 'k')
K.path([(7, 14), (20, 14), (20, 7)], '=', 2)
K.rect(16, 11, 25, 17, 'q')                                 # la place
K.put(20, 13, 'W'), K.put(21, 13, 'W'), K.put(20, 12, 'W'), K.put(21, 12, 'W')
for b in [dict(x=9, y=8, l=5, h=4, genre='soin', nom="Maison de soin", toit='#e08a7a', evenement='soin_corail'),
          dict(x=27, y=8, l=5, h=4, genre='boutique2', nom="Comptoir des coraux", toit='#4ab0b8', evenement='boutique_corail'),
          dict(x=10, y=18, l=6, h=4, genre='maison', nom="Maison de la doyenne", toit='#c87ab8', evenement='maison_corail')]:
    K.m['batiments'].append(b)
    K.rect(b['x'], b['y'], b['x'] + b['l'] - 1, b['y'] + b['h'] - 1, '.')
    dx = b['x'] + b['l'] // 2
    K.path([(dx, b['y'] + b['h']), (dx, 16 if b['y'] < 12 else b['y'] + b['h'] + 1), (20, 16 if b['y'] < 12 else b['y'] + b['h'] + 1)], '=', 1, keep='qW')
K.path([(20, 23), (20, 17)], '=', 1, keep='qW')
for x, y in [(15, 10), (26, 10), (15, 18), (26, 18)]:
    K.put(x, y, 'L')
K.sprinkle(20, 'T', '.', 4, 4, 35, 25, away=1)
K.sprinkle(20, 'F', '.', 4, 4, 35, 25)
K.sprinkle(8, 'R', '.', 4, 4, 35, 25, away=1)
K.npc(22, 15, 'ancien', 'bas', 'doyenne')
K.npc(18, 16, 'enfant', 'droite', roam=2, lines=["Mon grand frère dit qu'un monstre vit au fond de la grotte, là-haut.", "Il garde un trident aussi grand qu'un arbre !"])
K.npc(24, 20, 'pecheur', 'gauche', lines=["Les Ruines, au nord du Récif, sont fermées par une porte de corail. Personne ne l'a jamais ouverte.", "La doyenne sait des choses. Allez la voir."])
K.npc(8, 13, 'voyageuse', 'bas', lines=["Bienvenue sur l'île de Corail ! On ne voit pas souvent des étrangers.", "Depuis que la brume est partie, les bateaux reviennent un peu."])
K.chest(33, 20, 'perle'), K.chest(6, 20, 'superpotion', 2)
K.warp(0, 14, 'recif', 45, 15, 'gauche'), K.warp(0, 15, 'recif', 45, 16, 'gauche')
K.m['declencheurs'] = [{"x": 7, "y": 14, "l": 1, "h": 2, "evenement": "corail_entree", "jusqua": "corail_vu"}]
K.m['carte_monde'] = [62, 92]

# ------------------------------------------------------------------------------------------- la Grotte des Marées
G, ex, br, oth = dungeon('marees', "Grotte des Marées", 'grotte', 40, 32, 71, '.', 'c', puzzle=True, decor='Cvx')
G.m['ambiance'] = {"effet": "brume"}
for _ in range(7):
    G.blob(G.rnd.randint(4, 35), G.rnd.randint(3, 26), 2, '~', only='#')
finish(G, ex, br, oth, 'gardien_marees', [('perle', 1), 'gemme_contre', ('elixir', 1), ('or', 3000), ('superpotion', 2)],
       ('corail', 20, 8), None, None, (44, 48, 3, ['noyelin', 'coquillon', 'ecumet', 'voltanguille', 'bastillon']))
K.warp(20, 6, 'marees', ex, 30, 'haut'), K.warp(21, 6, 'marees', ex, 30, 'haut')
r = K.reach((1, 14))
for p in [(20, 7), (11, 12), (29, 12), (13, 22), (21, 15)]:
    if p not in r:
        K.show()
        sys.exit('corail : case inaccessible %s' % (p,))
K.save()
G.save()

# Le Récif : une passe vers l'est (île de Corail), et les Ruines scellées sans le trident
R = carte.load('recif')
rows = [list(r) for r in R['tuiles']]
for y in (15, 16):
    for x in (46, 47):
        rows[y][x] = '.'
R['tuiles'] = [''.join(r) for r in rows]
R['passages'] += [{"x": 47, "y": 15, "vers": "corail", "tx": 1, "ty": 14, "direction": "droite"},
                  {"x": 47, "y": 16, "vers": "corail", "tx": 1, "ty": 15, "direction": "droite"}]
for w in R['passages']:
    if w['vers'] == 'ruines':
        w['condition'] = {"objet": "trident"}
        w['message'] = "Une porte de corail scelle les Ruines. On dit que seul le Trident d'écume peut l'ouvrir."
carte.save('recif', R)

# ------------------------------------------------------------------------------------------- données
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
ESP.append({"id": "gardien_marees", "nom": "Gardien des marées", "sprite": "gardien_marees", "types": ["eau", "roche"],
            "base": {"pv": 104, "pm": 40, "attaque": 64, "defense": 78, "magie": 58, "resistance": 66, "vitesse": 40},
            "immunites": ["sommeil"], "forme": "boss", "couleurs": ["#2a9a9a", "#ffe08a"],
            "apprend": [[1, "vague"], [1, "trombe"], [1, "avalanche"], [1, "carapace"]], "talent": "cuirasse"})
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
V = os.path.join(os.path.dirname(ICI), 'sprites', 'variantes.json')
vt = open(V, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in vt else '\n'
open(V, 'w', encoding='utf-8', newline='').write(vt.rstrip()[:-1].rstrip() + ',' + nl + '  ' + json.dumps(
    {"id": "gardien_marees", "de": "bastillon", "teintes": [[20, 60, 45, 1.3, 1.2], [60, 200, 175, 1.2, 1.1]]}, ensure_ascii=False) + nl + ']' + nl)

OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('perle') + 1
OBJ.insert(k, {"id": "trident", "nom": "Trident d'écume", "icone": "ic_lance_4",
               "description": "Une arme ancienne des gardiens de la mer. Elle ouvre la porte de corail des Ruines englouties.", "prix": 0})
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')

RG = json.load(open(D + 'regles.json', encoding='utf-8'))
cons = RG['conseils']
i = [c['drapeau'] for c in cons].index('kraken')
cons.insert(i, {"drapeau": "marees_boss", "boss": "gardien_marees", "lieu": "le fond de la Grotte des Marées", "gemme": "gemme_foudre",
                "texte": "Une carapace très solide : la magie passe mieux que les coups. À mi-vie, il appelle la marée."})
open(D + 'regles.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(RG) + '\n')


def say(t): return {"action": "dire", "texte": t}


EV = {
    "corail_entree": {"actions": [{"action": "drapeau", "nom": "corail_vu"},
                                  say("Une petite île couverte de coraux roses. Des maisons basses, des filets qui sèchent, une odeur de sel."),
                                  say("Au nord, des falaises percées d'une grotte où la mer s'engouffre en grondant.")]},
    "doyenne": {"pages": [
        {"si": {"drapeau": "kraken"}, "actions": [say("Doyenne : Les Ruines sont ouvertes, et le Kraken ne gronde plus. Les anciens gardiens seraient fiers de vous.")]},
        {"si": {"objet": "trident"}, "actions": [
            say("Doyenne : Le Trident d'écume ! Je n'aurais jamais cru le revoir."),
            say("Doyenne : Allez à la porte de corail, au nord du Récif. Elle s'ouvrira devant lui."),
            say("Doyenne : Mais méfiez-vous : derrière, le Kraken garde le chemin de la Tour de l'Orage.")]},
        {"si": {"drapeau": "quete:trident"}, "actions": [say("Doyenne : Le trident est au fond de la Grotte des Marées, au nord de l'île. Le Gardien des marées ne le laissera pas partir sans combat.")]},
        {"actions": [
            say("Doyenne : Des voyageurs du continent ? Alors c'est vous qui avez apaisé la Mère des Brumes."),
            say("Doyenne : Vous cherchez la Tour de l'Orage ? Le seul chemin passe par les Ruines englouties."),
            say("Doyenne : Mais les Ruines sont scellées par une porte de corail, depuis mille ans. Seul le Trident d'écume peut l'ouvrir."),
            say("Doyenne : Il repose au fond de la Grotte des Marées, ici, au nord de l'île. Son gardien n'a jamais laissé personne en revenir."),
            {"action": "quete", "id": "trident"}]}]},
    "soin_corail": {"actions": [{"action": "soigner"}, {"action": "reveil"}, say("Soigneuse : L'eau de mer guérit tout, ou presque ! Voilà, tout le monde est remis.")]},
    "boutique_corail": {"actions": [{"action": "boutique", "objets": ["potion", "superpotion", "ether", "remede", "elixir", "plume", "lanterne_or",
                                                                       "lame_givre", "sceptre_aurore", "armure_givre", "plume_ange", "amulette_maree",
                                                                       "gemme_foudre", "gemme_contre", "gemme_vol"]}]},
    "maison_corail": {"actions": [say("La maison de la doyenne. Des coquillages pendent au plafond, et une vieille carte montre une porte de corail.")]},
    "marees_boss": {"actions": [
        say("Au fond de la grotte, un trident brille dans un bassin. L'eau se soulève : une tortue géante couverte de corail se dresse."),
        say("Gardien des marées : Mille ans que je garde ce trident. Prouvez que vous le méritez !"),
        {"action": "question", "texte": "Affronter le Gardien des marées ?", "oui": [
            {"action": "combat", "musique": "boss", "ennemis": [
                {"espece": "ecumet", "niveau": 46},
                {"espece": "gardien_marees", "niveau": 50, "pv": 5, "boss": True, "bouclier": 8,
                 "phases": [{"pv": 0.5, "message": "Le Gardien appelle la marée !", "meteo": "pluie", "renforts": [{"espece": "noyelin", "niveau": 47}]}]},
                {"espece": "coquillon", "niveau": 46}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [{"action": "drapeau", "nom": "marees_boss"}, {"action": "donner", "objet": "trident", "quantite": 1},
                          say("Gardien des marées : … Vous êtes dignes. Prenez le trident, et ouvrez les Ruines."),
                          say("La tortue retourne au fond du bassin. Le Trident d'écume est à vous."),
                          {"action": "quete", "id": "trident", "fin": True}]}],
         "non": [say("Le Gardien vous observe, immobile, sous l'eau.")]}]},
}
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
Q = {"id": "trident", "nom": "Le Trident d'écume", "lieu": "Île de Corail",
     "etapes": [{"texte": "Trouver le Trident d'écume au fond de la Grotte des Marées, au nord de l'île de Corail, pour ouvrir les Ruines englouties."}],
     "fin_texte": "Le Trident d'écume ouvre la porte de corail des Ruines."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
print('île de Corail écrite ; grotte : entrée x', ex)
