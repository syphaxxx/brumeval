"""Donjons optionnels (étape « 20 heures », 2026-10-20) : un par moment de l'histoire, plus l'Abîme d'après Aube.

    python outils/villes/donjons.py     # une seule fois (refuse si data/cartes/mines.json existe)

Chaque donjon : des salles reliées par des couloirs (tirées depuis une graine fixe), l'entrée en bas, le boss dans la
salle la plus éloignée, des coffres dans les autres salles, parfois l'énigme des rochers (gabarit déjà vérifié dans le
Gouffre d'Aube). L'entrée est une case de bord d'une carte existante, ouverte sous condition (drapeau d'histoire).
Écrit aussi les boss (variantes recolorées), les événements, les habitants qui en parlent et une quête par donjon.
"""
import json
import os
import sys
from collections import deque

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
from cartegen import Map, W_OK  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/mines.json'):
    sys.exit('Les donjons existent déjà.')


def dungeon(mid, nom, theme, w, h, seed, floor, enc, wall='#', nrooms=9, puzzle=False, decor='R'):
    """Salles + couloirs. floor : sol des salles ; enc : sol des couloirs (rencontres). Rend (carte, entrée, salle du
    boss, autres salles)."""
    M = Map(mid, nom, theme, w, h, wall, seed)
    rnd = M.rnd
    rooms = []
    tries = 0
    while len(rooms) < nrooms and tries < 2000:
        tries += 1
        rw, rh = rnd.randint(5, 9), rnd.randint(4, 7)
        x, y = rnd.randint(2, w - rw - 3), rnd.randint(2, h - rh - 6)
        if any(x < r[0] + r[2] + 2 and r[0] < x + rw + 2 and y < r[1] + r[3] + 2 and r[1] < y + rh + 2 for r in rooms):
            continue
        rooms.append((x, y, rw, rh))
    centers = [(x + rw // 2, y + rh // 2) for x, y, rw, rh in rooms]
    # couloirs : arbre couvrant (le plus proche déjà relié), puis deux boucles
    linked = [0]
    for _ in range(1, len(rooms)):
        best = None
        for i in range(len(rooms)):
            if i in linked:
                continue
            for j in linked:
                d = abs(centers[i][0] - centers[j][0]) + abs(centers[i][1] - centers[j][1])
                if best is None or d < best[0]:
                    best = (d, i, j)
        _, i, j = best
        M.path([centers[j], (centers[i][0], centers[j][1]), centers[i]], enc, 2)
        linked.append(i)
    for _ in range(2):
        i, j = rnd.sample(range(len(rooms)), 2)
        M.path([centers[i], (centers[j][0], centers[i][1]), centers[j]], enc, 1)
    for x, y, rw, rh in rooms:
        M.rect(x, y, x + rw - 1, y + rh - 1, floor)
        for cx, cy in ((x, y), (x + rw - 1, y), (x, y + rh - 1), (x + rw - 1, y + rh - 1)):  # décor dans les coins
            if rnd.random() < .7:
                M.put(cx, cy, rnd.choice(decor))
    # entrée : la salle la plus basse, reliée au bord du bas
    low = max(range(len(rooms)), key=lambda i: rooms[i][1] + rooms[i][3])
    ex = centers[low][0]
    M.path([(ex, h - 1), centers[low]], floor, 2)
    # boss : la salle la plus loin de l'entrée (à pied)
    dist = {}
    q = deque([(ex, h - 2)])
    dist[(ex, h - 2)] = 0
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in dist and 0 <= n[0] < w and 0 <= n[1] < h and M.get(*n) in W_OK:
                dist[n] = dist[(x, y)] + 1
                q.append(n)
    far = max(range(len(rooms)), key=lambda i: dist.get(centers[i], -1))
    others = [i for i in range(len(rooms)) if i not in (far, low)]
    if puzzle:  # le gabarit de l'énigme dans la plus grande autre salle (agrandie à 9 × 8, alcôve à droite)
        pi = max(others, key=lambda i: rooms[i][2] * rooms[i][3])
        others.remove(pi)
        x, y = rooms[pi][0], rooms[pi][1]
        x = min(x, w - 15)
        y = min(y, h - 14)
        M.path([centers[pi], (x + 1, y + 6)], floor, 1)  # relié au reste d'abord : le gabarit passe par-dessus
        M.rect(x, y, x + 8, y + 7, '.')
        M.put(x + 4, y + 3, 'O'), M.put(x + 6, y + 5, 'O')
        M.put(x + 4, y + 1, 'P'), M.put(x + 8, y + 5, 'P')
        M.rect(x + 9, y + 1, x + 11, y + 3, wall)
        M.put(x + 9, y + 2, 'D'), M.put(x + 10, y + 2, '.'), M.put(x + 11, y + 2, '.')
        M.m['enigme'] = {"drapeau": mid + "_grille", "x": x + 1, "y": y + 3,
                         "message": "Les dalles s'enfoncent : la grille de l'alcôve se lève !", "zone": {"x": x, "y": y, "l": 12, "h": 8}}
        M.chest(x + 11, y + 2, 'gemme_maitre' if mid != 'mines' else 'gemme_esprit')
        rooms[pi] = (x, y, 9, 8)
    return M, ex, rooms[far], [rooms[i] for i in others]


def finish(M, ex, boss_room, others, boss, chests, back, cond, msg, zone, look_room_npc=None):
    bx, by = boss_room[0] + boss_room[2] // 2 - 1, boss_room[1] + 1
    M.rect(boss_room[0], boss_room[1], boss_room[0] + boss_room[2] - 1, boss_room[1] + boss_room[3] - 1, '.')
    M.boss(bx, by, boss, M.mid + '_boss', M.mid + '_boss')
    for (x, y, rw, rh), item in zip(others, chests):
        M.chest(x + rw - 2, y + 1, *(item if isinstance(item, tuple) else (item,)))
    M.warp(ex, M.h - 1, back[0], back[1], back[2], 'bas')
    M.warp(ex + 1, M.h - 1, back[0], back[1], back[2], 'bas')
    M.zone(0, 0, M.w, M.h, *zone)
    r = M.reach((ex, M.h - 2))
    for o in M.m['coffres']:
        if not any(abs(o['x'] - x) + abs(o['y'] - y) == 1 for x, y in r) and not M.m.get('enigme'):
            raise SystemExit('%s : coffre inaccessible %s' % (M.mid, o))
    if not any(abs(bx - x) <= 2 and abs(by - y) <= 2 for x, y in r):
        raise SystemExit('%s : boss inaccessible' % M.mid)
    return M


def entrance(mid, x, y, d, to, tx, ty, cond, msg, carve=None):
    """Ouvre une case de bord d'une carte existante (sol) et y met le passage vers le donjon. carve : (x0, y0, x1, y1),
    une clairière à dégager d'abord (forêt fermée)."""
    m = carte.load(mid)
    rows = [list(r) for r in m['tuiles']]
    if carve:
        for yy in range(carve[1], carve[3] + 1):
            for xx in range(carve[0], carve[2] + 1):
                rows[yy][xx] = '.'

    rows[y][x] = 'k' if m['theme'] in ('cendres', 'grotte', 'neige') else '='
    m['tuiles'] = [''.join(r) for r in rows]
    m['passages'].append({"x": x, "y": y, "vers": to, "tx": tx, "ty": ty, "direction": d, "condition": cond, "message": msg})
    return m


MAPS = []
# 1. Mines de Forgeroc (acte 1, après Sylvarque) -------------------------------------------------------------------
M, ex, br, oth = dungeon('mines', "Mines de Forgeroc", 'grotte', 40, 32, 41, '.', 'c', puzzle=True, decor='RCvx')
M.m['ambiance'] = {"effet": "obscurite"}
finish(M, ex, br, oth, 'troll_mines', ['minerai', ('coeur_braise', 2), ('minerai', 3), 'superpotion', ('or', 1500)],
       ('cendrelune', 1, 48), None, None, (16, 20, 3, ['galetor', 'rocaillou', 'tisseline', 'fumerole', 'brutor']))
MAPS.append(M)
E1 = entrance('cendrelune', 0, 48, 'gauche', 'mines', ex, 30, {"drapeau": "boss1"}, "Des planches clouées ferment la vieille mine.")
E1['habitants'].append({"x": 2, "y": 47, "apparence": "montagnard", "direction": "gauche", "evenement": "mineur"})

# 2. Glacier de Givreval (acte 2, après Ignarok) ---------------------------------------------------------------------
G2, ex2, br2, oth2 = dungeon('glacier', "Glacier de Givreval", 'neige', 40, 32, 42, 'i', 'n', puzzle=True, decor='xR')
G2.m['ambiance'] = {"effet": "neige"}
finish(G2, ex2, br2, oth2, 'colosse_givre', [('cristal_givre', 2), 'elixir', 'gemme_givre', ('or', 3000), 'plume'],
       ('pics', 82, 24), None, None, (27, 31, 3, ['givrelin', 'banquisot', 'glaconnet', 'givrenard', 'cristallin']))
MAPS.append(G2)
E2 = entrance('pics', 83, 24, 'droite', 'glacier', ex2, 30, {"drapeau": "boss2"}, "Un mur de glace ferme le passage.")
E2['habitants'].append({"x": 81, "y": 22, "apparence": "chevalier", "direction": "droite", "evenement": "garde_glacier"})

# 3. Épave du Léviathan (après la Mère des Brumes) -------------------------------------------------------------------
G3, ex3, br3, oth3 = dungeon('epave', "Épave du Léviathan", 'grotte', 40, 32, 43, '.', 'c', decor='Cv')
G3.m['ambiance'] = {"effet": "brume"}
for _ in range(6):  # de l'eau dans les cales
    G3.blob(G3.rnd.randint(4, 35), G3.rnd.randint(3, 26), 2, '~', only='#')
finish(G3, ex3, br3, oth3, 'leviathan', [('perle', 1), 'gemme_vol', ('elixir', 2), ('or', 6000), 'lame_ecume'],
       ('recif', 28, 34), None, None, (46, 50, 4, ['noyelin', 'coquillon', 'voltanguille', 'bastillon', 'ecumet']))
MAPS.append(G3)
E3 = entrance('recif', 28, 35, 'bas', 'epave', ex3, 30, {"drapeau": "boss4"}, "La marée est trop haute pour atteindre l'épave.")

# 4. L'Abîme (après l'acte 3) : trois étages -------------------------------------------------------------------------
ab = []
for k in range(1, 4):
    A, exa, bra, otha = dungeon('abime%d' % k, "L'Abîme (étage %d)" % k, 'grotte', 40, 32, 50 + k, '.', 'c', nrooms=10, decor='xR')
    A.m['ambiance'] = {"effet": "obscurite"}
    ab.append((A, exa, bra, otha))
for k, (A, exa, bra, otha) in enumerate(ab, start=1):
    lv = (76 + 4 * k, 80 + 4 * k, 3, ['rocaube', 'aubespectre', 'nacrelin', 'loup_aube', 'brumeloup', 'nebulis'])
    if k < 3:  # pas de boss : un escalier dans la salle la plus loin mène à l'étage suivant
        sx, sy = bra[0] + bra[2] // 2, bra[1] + 1
        A.rect(bra[0], bra[1], bra[0] + bra[2] - 1, bra[1] + bra[3] - 1, '.')
        A.put(sx, sy, 'k')
        nxt = ab[k][1]
        A.warp(sx, sy, 'abime%d' % (k + 1), nxt, 30, 'haut')
        for (x, y, rw, rh), item in zip(otha, [('elixir', 2), 'gemme_maitre', ('or', 8000), 'plume', 'gemme_orage']):
            A.chest(x + rw - 2, y + 1, *(item if isinstance(item, tuple) else (item,)))
        back = ('lueurs', 1, 22) if k == 1 else ('abime%d' % (k - 1), ab[k - 2][2][0] + ab[k - 2][2][2] // 2, ab[k - 2][2][1] + 2)
        A.warp(exa, 31, back[0], back[1], back[2], 'bas'), A.warp(exa + 1, 31, back[0], back[1], back[2], 'bas')
        A.zone(0, 0, 40, 32, *lv)
    else:
        back = ('abime2', ab[1][2][0] + ab[1][2][2] // 2, ab[1][2][1] + 2)
        finish(A, exa, bra, otha, 'orage_ancien', [('elixir', 3), 'gemme_maitre', ('or', 15000), 'insigne_gardien'],
               back, None, None, lv)
    MAPS.append(A)
E4 = entrance('lueurs', 0, 22, 'gauche', 'abime1', ab[0][1], 30, {"drapeau": "aube_fin"},
              "Un vent glacé sort d'une faille. Il vaut mieux attendre que l'île soit en paix.", carve=(1, 21, 5, 23))
E4['habitants'].append({"x": 2, "y": 21, "apparence": "mage_givre", "direction": "gauche", "evenement": "ilya_abime", "si": {"drapeau": "aube_fin"}})

# Écriture des cartes
for M in MAPS:
    M.save()
for mid, m in (('cendrelune', E1), ('pics', E2), ('recif', E3), ('lueurs', E4)):
    carte.save(mid, m)

# ------------------------------------------------------------------------------------------- espèces, objets, histoire
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))


def esp(i, nom, types, base, couleurs, apprend, talent, src):
    e = {"id": i, "nom": nom, "sprite": i}
    e["types" if len(types) > 1 else "type"] = types if len(types) > 1 else types[0]
    e["base"] = dict(zip(["pv", "pm", "attaque", "defense", "magie", "resistance", "vitesse"], base))
    e.update({"immunites": ["sommeil"], "forme": "boss", "couleurs": couleurs, "apprend": apprend, "talent": talent})
    return e


ESP += [
    esp("troll_mines", "Troll des mines", ["roche", "normal"], [96, 20, 66, 70, 30, 44, 34], ["#8a6a4a", "#e0c8a0"],
        [[1, "fracas"], [1, "eboul"], [1, "seisme"], [1, "cri"]], "carapace", "troll_grand"),
    esp("colosse_givre", "Colosse de givre", ["glace", "roche"], [104, 30, 70, 74, 50, 58, 36], ["#a0d0f0", "#ffffff"],
        [[1, "corne_givre"], [1, "avalanche"], [1, "blizzard_eternel"], [1, "carapace"]], "cuirasse", "golem_grand"),
    esp("leviathan", "Léviathan", ["eau", "ombre"], [110, 50, 70, 62, 72, 64, 56], ["#2a6a6a", "#a0ffe0"],
        [[1, "trombe"], [1, "lame_ombre"], [1, "vague"], [1, "eclipse"]], "piquant", "kraken"),
    esp("orage_ancien", "Orage ancien", ["foudre", "feu"], [130, 80, 80, 72, 86, 78, 68], ["#c0402a", "#ffe08a"],
        [[1, "foudre_noire"], [1, "pyro"], [1, "orage"], [1, "ouragan"], [1, "eclipse"]], "esprit_clair", "seigneur_orage"),
]
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
V = os.path.join(os.path.dirname(ICI), 'sprites', 'variantes.json')
vt = open(V, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in vt else '\n'
body = vt.rstrip()[:-1].rstrip()
for vid, src, te in [("troll_mines", "troll_grand", [[-1, 0, 30, 0.6, 1.0, 0.3]]),
                     ("colosse_givre", "golem_grand", [[-1, 0, 200, 0.5, 1.4, 0.3]]),
                     ("leviathan", "kraken", [[200, 300, 165, 1.0, 0.85]]),
                     ("orage_ancien", "seigneur_orage", [[200, 320, 8, 1.1, 1.1]])]:
    body += ',' + nl + '  ' + json.dumps({"id": vid, "de": src, "teintes": te}, ensure_ascii=False)
open(V, 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)

OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('coeur_aube') + 1
for i, o in enumerate([
    {"id": "croc_troll", "nom": "Croc de troll", "description": "Trophée : un croc gros comme un poing, rapporté des Mines de Forgeroc.", "prix": 300, "trophee": True},
    {"id": "coeur_colosse", "nom": "Cœur du colosse", "description": "Trophée : un bloc de glace qui ne fond jamais, venu du Glacier de Givreval.", "prix": 700, "trophee": True},
    {"id": "oeil_leviathan", "nom": "Œil du Léviathan", "description": "Trophée : une perle noire, l'œil du monstre de l'épave.", "prix": 1200, "trophee": True},
    {"id": "eclair_fige", "nom": "Éclair figé", "description": "Trophée : un éclair pris dans le cristal, au fond de l'Abîme.", "prix": 2500, "trophee": True},
    {"id": "lame_orage", "nom": "Lame de l'orage ancien", "icone": "ic_epee_4", "description": "Forgée dans l'éclair figé. Elle gronde quand on la tire.", "prix": 8000,
     "equipement": "arme", "bonus": {"attaque": 24, "magie": 8, "vitesse": 3, "critique": 6}},
]):
    OBJ.insert(k + i, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')


def say(t): return {"action": "dire", "texte": t}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}


def boss_event(mid, intro, foes, renforts, win, trophy, extra, quest):
    v = [{"action": "drapeau", "nom": mid + "_boss"}, give(trophy)] + extra + win + [{"action": "quete", "id": quest, "fin": True}]
    return {"actions": intro + [{"action": "question", "texte": "Combattre ?", "oui": [
        {"action": "combat", "ennemis": foes, "renforts": renforts, "boss": True, "fuite": False, "capture": False, "victoire": v}],
        "non": [say("Vous reculez sans bruit.")]}]}


EV = {
    "mineur": {"pages": [
        {"si": {"drapeau": "mines_boss"}, "actions": [say("Mineur : Le troll est parti ! On va pouvoir rouvrir la mine. Merci, les jeunes.")]},
        {"si": {"drapeau": "boss1"}, "actions": [
            say("Mineur : La vieille mine, à l'ouest… Un troll s'y est installé quand le volcan a commencé à gronder."),
            say("Mineur : Il y a encore du bon minerai là-dedans. Et une salle de pierres qu'on n'a jamais su ouvrir."),
            {"action": "quete", "id": "mines"}]},
        {"actions": [say("Mineur : Tant que Sylvarque garde le col, plus personne ne vient acheter notre minerai.")]}]},
    "mines_boss": boss_event("mines", [say("Au fond de la galerie, un troll énorme ronge des cailloux. Il vous a sentis !")],
                             [{"espece": "rocaillou", "niveau": 18}, {"espece": "troll_mines", "niveau": 21, "pv": 4, "boss": True, "bouclier": 5},
                              {"espece": "galetor", "niveau": 18}], [{"espece": "brutor", "niveau": 19}],
                             [say("Le troll s'enfuit par une galerie effondrée. Il a laissé un croc derrière lui.")], "croc_troll",
                             [{"action": "or", "quantite": 800}], "mines"),
    "garde_glacier": {"pages": [
        {"si": {"drapeau": "glacier_boss"}, "actions": [say("Garde : Le colosse est tombé ! Le glacier redevient silencieux.")]},
        {"si": {"drapeau": "boss2"}, "actions": [
            say("Garde : Le glacier, à l'est, s'est réveillé en même temps que le Givrecorne. Un colosse de glace y marche."),
            say("Garde : Si vous y allez, couvrez-vous. Et méfiez-vous des dalles : les anciens y ont caché quelque chose."),
            {"action": "quete", "id": "glacier"}]},
        {"actions": [say("Garde : Personne ne passe. Le glacier est trop dangereux.")]}]},
    "glacier_boss": boss_event("glacier", [say("Une montagne de glace se déplie lentement : le Colosse de givre ouvre deux yeux bleus.")],
                               [{"espece": "givrelin", "niveau": 30}, {"espece": "colosse_givre", "niveau": 33, "pv": 4.5, "boss": True, "bouclier": 6,
                                 "phases": [{"pv": 0.5, "message": "Le Colosse se couvre de givre !", "bonus": {"defense": 1}}]},
                                {"espece": "banquisot", "niveau": 30}], [],
                               [say("Le Colosse s'effondre en mille éclats. Au milieu, un bloc de glace bat comme un cœur.")], "coeur_colosse",
                               [{"action": "or", "quantite": 2000}], "glacier"),
    "epave_boss": boss_event("epave", [say("Dans la cale noyée, l'eau se met à bouillonner. Un œil immense s'ouvre sous la surface."),
                                       say("Le Léviathan surgit, couvert d'algues et de coquillages !")],
                             [{"espece": "noyelin", "niveau": 48}, {"espece": "leviathan", "niveau": 52, "pv": 5, "boss": True, "bouclier": 8,
                               "phases": [{"pv": 0.5, "message": "Le Léviathan appelle les profondeurs !", "renforts": [{"espece": "voltanguille", "niveau": 49}]}]},
                              {"espece": "noyelin", "niveau": 48}], [],
                             [say("Le Léviathan sombre dans les profondeurs. Une perle noire roule sur le pont : son œil.")], "oeil_leviathan",
                             [{"action": "or", "quantite": 5000}], "epave"),
    "ilya_abime": {"pages": [
        {"si": {"drapeau": "abime3_boss"}, "actions": [say("Ilya : L'Orage ancien… Même mon peuple ne savait pas qu'il dormait là. Vous êtes vraiment incroyables.")]},
        {"actions": [say("Ilya : Cette faille s'est ouverte quand la Brume Noire est tombée. Elle descend très profond."),
                     say("Ilya : Tout au fond, quelque chose gronde. Un reste de l'orage d'il y a mille ans, peut-être."),
                     say("Ilya : N'y allez que si vous êtes très forts. Trois étages, et personne n'en est jamais remonté."),
                     {"action": "quete", "id": "abime"}]}]},
    "abime3_boss": boss_event("abime3", [say("Au fond de l'Abîme, l'air crépite. Une tempête de feu et d'éclairs prend forme."),
                                         say("Orage ancien : Mille ans… Mille ans dans le noir. Je vais enfin revoir le ciel !")],
                              [{"espece": "nebulis", "niveau": 94}, {"espece": "orage_ancien", "niveau": 100, "pv": 9, "boss": True, "bouclier": 18,
                                "phases": [{"pv": 0.7, "message": "L'Orage ancien embrase l'Abîme !", "bonus": {"magie": 1}},
                                           {"pv": 0.4, "message": "Des éclairs jaillissent de partout !", "bouclier": 14,
                                            "renforts": [{"espece": "orageline", "niveau": 95}]},
                                           {"pv": 0.15, "message": "L'Orage ancien rassemble ses dernières forces !", "bonus": {"attaque": 1, "vitesse": 1}}]},
                               {"espece": "brumeloup", "niveau": 94}], [],
                              [say("L'Orage ancien se disperse en une pluie d'étincelles. Il ne reste qu'un éclair figé dans le cristal."),
                               give("lame_orage")], "eclair_fige", [{"action": "or", "quantite": 20000}], "abime"),
}
for m in ('mines', 'glacier', 'epave', 'abime3'):
    EV[m + '_boss']["actions"][-1]["oui"][0]["musique"] = "boss_final" if m == 'abime3' else "boss"
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
QUESTS = [
    {"id": "mines", "nom": "La vieille mine", "lieu": "Monts Cendrelune", "etapes": [{"texte": "Chasser le troll des Mines de Forgeroc, à l'ouest de la ville."}], "fin_texte": "Le troll a quitté la mine."},
    {"id": "glacier", "nom": "Le colosse du glacier", "lieu": "Pics Givrés", "etapes": [{"texte": "Abattre le Colosse de givre, au fond du Glacier de Givreval, à l'est des Pics."}], "fin_texte": "Le glacier est redevenu silencieux."},
    {"id": "epave", "nom": "L'épave du Léviathan", "lieu": "Récif d'Écume", "etapes": [{"texte": "Explorer l'épave échouée au sud du Récif d'Écume, et vaincre ce qui vit dans sa cale."}], "fin_texte": "Le Léviathan a sombré."},
    {"id": "abime", "nom": "L'Abîme", "lieu": "Bois des Lueurs", "etapes": [{"texte": "Descendre les trois étages de l'Abîme, à l'ouest du Bois des Lueurs, jusqu'à ce qui gronde au fond."}], "fin_texte": "L'Orage ancien est dissipé."},
]
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
body = qt.rstrip()[:-1].rstrip()
for q in QUESTS:
    body += ',' + nl + json.dumps(q, ensure_ascii=False)
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)
for M in MAPS:
    print(M.mid, 'entrée x', [p for p in M.m['passages']][:1])
print('donjons écrits')
