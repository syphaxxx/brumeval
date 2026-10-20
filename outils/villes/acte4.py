"""Acte 4 (étape « 20 heures », 2026-10-20) : l'île des Échos, après la Brume Noire.

    python outils/villes/acte4.py      # depuis la racine du dépôt ; une seule fois (refuse si data/cartes/echos.json existe)
    python outils/sprites/decoupe.py   # ensuite : les sprites des nouvelles créatures (variantes recolorées)

Quand la Brume Noire se dissipe (`aube_fin`), une île apparaît au nord-ouest d'Aube. Une barque attend au bout d'une
nouvelle jetée, à l'ouest de Havre-d'Aube. Sur l'île : Port-Écho (Oriane, la gardienne de l'île), la Forêt Pétrifiée
(le Colosse d'écho), le Cratère des Échos (le Phénix d'écho) et le Sanctuaire des Échos (donjon, énigme), où attend
l'Ombre du Premier Gardien. Écrit les cartes, 6 créatures et 3 boss (variantes recolorées), les objets, les
événements, la quête `echos` et deux discussions. Pour refaire : remettre data/ et outils/sprites/variantes.json avec
git, puis relancer.
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
from cartegen import Map, dungeon, finish  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/echos.json'):
    sys.exit("L'acte 4 existe déjà (data/cartes/echos.json).")
PROBLEMES = []


def check(M, start, pts):
    r = M.reach(start)
    for p in pts:
        if p not in r and not any((p[0] + dx, p[1] + dy) in r for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
            PROBLEMES.append((M.mid, p))


# =====================================================================================================================
# 1. Port-Écho : le village de l'île
# =====================================================================================================================
E = Map('echos', "Port-Écho", 'vallee', 44, 32, '.', 61)
E.rect(0, 0, 43, 1, 'T'), E.rect(0, 0, 1, 31, 'T'), E.rect(42, 0, 43, 31, 'T')
E.rect(0, 25, 43, 31, '~')
E.rect(2, 23, 41, 24, '.')
E.path([(21, 0), (21, 24)], '=', 2)            # grand-rue : de la jetée à la forêt de pierre
E.rect(21, 25, 22, 31, 'B')                    # jetée
E.path([(23, 13), (43, 13)], '=', 2)           # route de l'est : le cratère
E.rect(13, 10, 30, 16, 'q')                    # place
E.rect(16, 12, 17, 13, 'W')                    # fontaine
for x, y in [(12, 9), (31, 9), (12, 17), (31, 17)]:
    E.put(x, y, 'L')
E.put(26, 16, 'Q'), E.put(14, 16, 'Q')
E.put(15, 20, 'M'), E.put(17, 20, 'M')
E.put(27, 21, 'C'), E.put(28, 21, 'C'), E.put(29, 21, 'K')
E.put(8, 22, 'X')
for b in [dict(x=4, y=6, l=5, h=4, genre='soin', nom="Maison de soin", toit='#7a8ab0', evenement='echos_soin'),
          dict(x=34, y=6, l=5, h=4, genre='boutique1', nom="Comptoir des Échos", toit='#c8a050', evenement='echos_boutique'),
          dict(x=4, y=16, l=6, h=4, genre='maison', nom="Maison d'Oriane", toit='#6a5aa8', evenement='echos_maison'),
          dict(x=33, y=16, l=8, h=4, genre='decor', nom="Ruines du vieux phare", toit='#a09080', dessin='ruines')]:
    E.m['batiments'].append(b)
    E.rect(b['x'], b['y'], b['x'] + b['l'] - 1, b['y'] + b['h'] - 1, '.')
    dx = b['x'] + b['l'] // 2
    if b['genre'] != 'decor':
        if b['y'] < 13:
            E.path([(dx, b['y'] + b['h']), (dx, 13)], '=', 1, keep='qW=')
        else:
            E.path([(dx, b['y'] + b['h']), (dx, 21), (21, 21)], '=', 1, keep='qW=')
E.path([(6, 13), (13, 13)], '=', 1, keep='q')
E.sprinkle(30, 'F', '.', 2, 2, 41, 22)
E.sprinkle(16, 'T', '.', 2, 2, 41, 5, away=1)
E.sprinkle(10, 'R', '.', 2, 21, 41, 24, away=1)
E.sprinkle(6, 'U', '.', 2, 17, 41, 22, away=1)
E.warp(21, 31, 'aube', 8, 27, 'haut'), E.warp(22, 31, 'aube', 9, 27, 'haut')
E.warp(21, 0, 'pierres', 23, 38, 'haut'), E.warp(22, 0, 'pierres', 24, 38, 'haut')
E.warp(43, 13, 'cratere', 1, 17, 'droite'), E.warp(43, 14, 'cratere', 1, 18, 'droite')
E.npc(24, 14, 'ermite', 'bas', 'oriane')
E.npc(19, 20, 'pecheur', 'droite', lines=["Ici, quand on crie, la montagne répond trois fois. Les enfants adorent ça.",
                                         "Mille ans que l'île dormait sous la brume. On ne savait même pas qu'il y avait une mer autour !"])
E.npc(30, 11, 'villageoise', 'gauche', lines=["Les arbres du nord sont en pierre. On dit qu'ils étaient vivants, avant que l'écho ne les fige.",
                                              "Un grand oiseau de feu tourne au-dessus du cratère, à l'est. Il ne dort jamais."])
E.npc(10, 21, 'enfant', 'bas', roam=2, lines=["Écho ! … Écho ! … Tu entends ? L'île me répond !"])
E.npc(36, 22, 'chat', 'gauche', roam=1, lines=["Le chat de Port-Écho miaule. L'écho miaule aussi."])
E.m['declencheurs'] = [{"x": 21, "y": 24, "l": 2, "h": 1, "evenement": "echos_arrivee", "jusqua": "echos_vue"}]
E.m['carte_monde'] = [5, 80]
check(E, (21, 30), [(21, 1), (42, 13), (6, 11), (36, 11), (7, 21), (24, 15), (21, 30)])

# =====================================================================================================================
# 2. La Forêt Pétrifiée : arbres de pierre, le Colosse d'écho, la porte du sanctuaire au nord
# =====================================================================================================================
P = Map('pierres', "Forêt Pétrifiée", 'foret', 48, 40, 'T', 62)
P.m['ambiance'] = {"effet": "brume"}
P.path([(23, 39), (23, 32), (12, 32), (12, 22), (30, 22), (30, 12), (23, 12), (23, 0)], '=', 2)
for cx, cy, r in [(16, 32, 4), (30, 30, 4), (8, 24, 4), (21, 22, 4), (38, 18, 5), (30, 8, 4), (14, 8, 4), (40, 30, 3)]:
    P.disc(cx, cy, r, ',', only='T')
P.disc(6, 14, 4, '.', only='T,')               # clairière du Colosse
P.path([(12, 22), (12, 16), (8, 16)], '=', 2)
P.path([(30, 22), (38, 22), (38, 18)], '.', 1)
P.path([(24, 32), (30, 32), (30, 30), (40, 30)], '.', 1)
P.path([(23, 12), (14, 12), (14, 8)], '.', 1)
P.path([(30, 12), (30, 8)], '.', 1)
P.sprinkle(60, 'R', 'T', 2, 2, 45, 37)        # arbres pétrifiés : des rochers parmi les troncs (puis 170 de plus,
#                                                 au cœur des bois : retouche faite après coup sur la carte)
P.sprinkle(20, 'R', ',', 2, 2, 45, 37, away=1)
P.boss(5, 12, 'colosse_echo', 'colosse_echo_vaincu', 'colosse_echo')
P.chest(38, 16, 'elixir', 2), P.chest(40, 31, 'gemme_force'), P.chest(14, 6, ('or'), 8000), P.chest(30, 6, 'gemme_maitre')
P.zone(0, 0, 48, 40, 84, 88, 3, ['petrolin', 'fossiloup', 'echoplume', 'spectrecho', 'loup_aube'])
P.warp(23, 39, 'echos', 21, 1, 'bas'), P.warp(24, 39, 'echos', 22, 1, 'bas')
GATE = {"drapeau": ["colosse_echo_vaincu", "phenix_echo_vaincu"]}
GATE_MSG = "Une porte de pierre ferme le chemin. Deux symboles y sont gravés : une montagne et une flamme. Ils sont éteints."
check(P, (23, 38), [(23, 1), (8, 15), (38, 17), (40, 30), (14, 7), (30, 7)])

# =====================================================================================================================
# 3. Le Cratère des Échos : cendres, lave, le Phénix d'écho sur le rebord
# =====================================================================================================================
C = Map('cratere', "Cratère des Échos", 'cendres', 48, 36, 'm', 63)
C.m['ambiance'] = {"effet": "cendres"}
for cx, cy, r in [(6, 17, 4), (12, 11, 4), (18, 10, 5), (24, 15, 5), (30, 20, 5), (40, 20, 5), (16, 26, 4), (34, 9, 4)]:
    C.disc(cx, cy, r, 'a', only='m')
C.path([(0, 17), (10, 17), (10, 10), (24, 10), (24, 20), (36, 20)], '=', 2)
C.path([(24, 20), (24, 26), (16, 26)], '=', 1)
C.path([(24, 10), (34, 10), (34, 8)], '=', 1)
for cx, cy, r in [(12, 11, 2), (24, 16, 2), (30, 21, 2), (16, 27, 2), (34, 9, 2), (18, 9, 2)]:
    C.blob(cx, cy, r, 'g', only='a')
C.blob(42, 9, 4, 'l', only='m'), C.blob(8, 28, 3, 'l', only='m'), C.blob(44, 30, 3, 'l', only='m')
C.sprinkle(18, 'R', 'a', 2, 2, 45, 33, away=1)
C.boss(40, 19, 'phenix_echo', 'phenix_echo_vaincu', 'phenix_echo')
C.chest(16, 28, 'elixir', 2), C.chest(35, 7, 'gemme_vie'), C.chest(28, 22, ('or'), 9000)
C.zone(0, 0, 48, 36, 86, 90, 3, ['cendrelys', 'braisaure', 'fossiloup', 'echoplume'])
C.warp(0, 17, 'echos', 42, 13, 'gauche'), C.warp(0, 18, 'echos', 42, 14, 'gauche')
C.m['carte_monde'] = [9, 79]
check(C, (1, 17), [(39, 20), (16, 27), (35, 8), (28, 21)])

# =====================================================================================================================
# 4. Le Sanctuaire des Échos : donjon, énigme, l'Ombre du Premier Gardien
# =====================================================================================================================
S, ex, br, oth = dungeon('sanctuaire_echos', "Sanctuaire des Échos", 'grotte', 40, 32, 64, '.', 'c', puzzle=True, decor='xR')
S.m['ambiance'] = {"effet": "lucioles"}
finish(S, ex, br, oth, 'ombre_gardien', [('elixir', 3), 'gemme_maitre', ('or', 12000), 'gemme_esprit', ('elixir', 2)],
       ('pierres', 23, 1), None, None, (88, 92, 3, ['spectrecho', 'echoplume', 'braisaure', 'petrolin', 'fossiloup']))
P.warp(23, 0, 'sanctuaire_echos', ex, 30, 'haut', GATE, GATE_MSG), P.warp(24, 0, 'sanctuaire_echos', ex + 1, 30, 'haut', GATE, GATE_MSG)
# la sortie du bas du sanctuaire ramène devant la porte (et non dessus)
for w in S.m['passages']:
    if w['vers'] == 'pierres':
        w['tx'], w['ty'] = 23 + (w['x'] - ex), 1

if PROBLEMES:
    for M in (E, P, C):
        if any(p[0] == M.mid for p in PROBLEMES):
            M.show()
    sys.exit('Cases inaccessibles : %s' % PROBLEMES)

# Aube : une jetée à l'ouest de la plage, et sa barque (fermée tant que la Brume Noire règne)
A = carte.load('aube')
rows = [list(r) for r in A['tuiles']]
for y in range(24, 30):
    rows[y][8] = rows[y][9] = 'B'
A['tuiles'] = [''.join(r) for r in rows]
BOAT = {"drapeau": "aube_fin"}
BOAT_MSG = "Une barque est amarrée au bout de la jetée. Ysolde dit que la mer est trop sombre tant que la Brume Noire règne."
A['passages'] += [{"x": 8, "y": 29, "vers": "echos", "tx": 21, "ty": 30, "direction": "haut", "condition": BOAT, "message": BOAT_MSG},
                  {"x": 9, "y": 29, "vers": "echos", "tx": 22, "ty": 30, "direction": "haut", "condition": BOAT, "message": BOAT_MSG}]

for M in (E, P, C, S):
    M.save()
carte.save('aube', A)

# =====================================================================================================================
# Espèces, sprites (variantes), objets
# =====================================================================================================================
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))


def esp(i, nom, types, base, forme, couleurs, apprend, talent, **k):
    e = {"id": i, "nom": nom, "sprite": i}
    e["types" if len(types) > 1 else "type"] = types if len(types) > 1 else types[0]
    e["base"] = dict(zip(["pv", "pm", "attaque", "defense", "magie", "resistance", "vitesse"], base))
    e.update({k2: v for k2, v in k.items() if k2 in ("esquive", "critique")})
    if "immunites" in k:
        e["immunites"] = k["immunites"]
    e["forme"] = forme
    e["couleurs"] = couleurs
    e["apprend"] = apprend
    e["talent"] = talent
    if "role" in k:
        e["role_combat"] = k["role"]
    if "taille" in k:
        e["taille"] = k["taille"]
    return e


NEW = [
    esp("petrolin", "Pétrolin", ["roche", "plante"], [82, 18, 74, 90, 40, 64, 32], "rocher", ["#8a9a80", "#d8e0c8"],
        [[1, "eboul"], [1, "etreinte"], [1, "carapace"], [1, "seisme"], [86, "avalanche"]], "carapace", role="protecteur"),
    esp("fossiloup", "Fossiloup", ["roche", "normal"], [78, 20, 86, 70, 34, 50, 70], "loup", ["#9a9088", "#e8e0d8"],
        [[1, "morsure"], [1, "eboul"], [1, "cri"], [1, "eclair"], [86, "avalanche"]], "intimidation", critique=8, role="chef"),
    esp("echoplume", "Échoplume", ["vent", "esprit"], [62, 34, 64, 50, 72, 62, 88], "oiseau", ["#9a7ad0", "#f0e8ff"],
        [[1, "rafale"], [1, "onde"], [1, "lame_air"], [1, "cauchemar"], [86, "ouragan"]], "vif", esquive=8),
    esp("cendrelys", "Cendrelys", ["feu", "esprit"], [64, 40, 52, 54, 84, 70, 72], "lezard", ["#5a8ad8", "#e0f0ff"],
        [[1, "flam"], [1, "onde"], [1, "pyro"], [1, "hurlement_spectral"], [88, "brasier"]], "meditation"),
    esp("braisaure", "Braisaure", ["feu", "roche"], [80, 24, 84, 76, 58, 56, 58], "lezard", ["#9a2a1a", "#ffb060"],
        [[1, "crocs"], [1, "magma"], [1, "eboul"], [1, "poing"], [88, "seisme"]], "rage", taille=1.15),
    esp("spectrecho", "Spectrécho", ["esprit", "ombre"], [60, 44, 40, 52, 86, 76, 70], "fantome", ["#3aa8a0", "#e0fff8"],
        [[1, "onde"], [1, "voile"], [1, "cauchemar"], [1, "eclipse"], [90, "hurlement_spectral"]], "furtif", role="soigneur"),
    esp("colosse_echo", "Colosse d'écho", ["roche", "esprit"], [120, 40, 82, 92, 60, 74, 38], "boss", ["#6a7a6a", "#e0f0d0"],
        [[1, "fracas"], [1, "avalanche"], [1, "cauchemar"], [1, "seisme"], [1, "carapace"]], "cuirasse", immunites=["sommeil"]),
    esp("phenix_echo", "Phénix d'écho", ["feu", "vent"], [115, 60, 76, 64, 88, 72, 80], "boss", ["#e05a1a", "#fff0a0"],
        [[1, "pyro"], [1, "ouragan"], [1, "brasier"], [1, "lame_air"], [1, "soins"]], "regeneration", immunites=["sommeil"]),
    esp("ombre_gardien", "Ombre du Gardien", ["ombre", "lumiere"], [135, 80, 86, 78, 90, 82, 70], "boss", ["#2a2a5a", "#ffe8a0"],
        [[1, "lame_ombre"], [1, "prisme"], [1, "eclipse"], [1, "nova"], [1, "foudre_noire"], [1, "charge_heroique"]], "esprit_clair",
        immunites=["sommeil"]),
]
TALENTS = {t for s in ESP for t in [s.get('talent')] if t}
for e in NEW:
    if e['talent'] not in TALENTS:
        sys.exit('talent inconnu : %s (%s)' % (e['talent'], e['id']))
ESP += NEW
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
V = os.path.join(os.path.dirname(ICI), 'sprites', 'variantes.json')
vt = open(V, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in vt else '\n'
body = vt.rstrip()[:-1].rstrip()
for vid, src, te in [("petrolin", "elem_terre", [[-1, 360, 95, 0.35, 0.95, 0.12]]),
                     ("fossiloup", "loup", [[-1, 360, 30, 0.15, 1.05, 0.05]]),
                     ("echoplume", "piafouine", [[-1, 360, 268, 0.75, 1.05]]),
                     ("cendrelys", "tisonnel", [[0, 70, 215, 1.0, 1.1], [300, 360, 215, 1.0, 1.1]]),
                     ("braisaure", "dragonnet", [[-1, 360, 8, 1.1, 0.8]]),
                     ("spectrecho", "fantome", [[-1, 360, 175, 0.9, 1.0, 0.25]]),
                     ("colosse_echo", "golem_grand", [[-1, 360, 100, 0.25, 0.95, 0.1]]),
                     ("phenix_echo", "griffon", [[-1, 360, 14, 1.9, 1.2, 0.35]]),
                     ("ombre_gardien", "seigneur_orage", [[180, 320, 245, 0.7, 0.6], [0, 179, 45, 1.0, 1.1], [321, 360, 45, 1.0, 1.1]])]:
    body += ',' + nl + '  ' + json.dumps({"id": vid, "de": src, "teintes": te}, ensure_ascii=False)
open(V, 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)

OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('recueil_orages') + 1
for i2, o in enumerate([
        {"id": "echo_pierre", "nom": "Écho de pierre", "icone": "ic_gemme_cristal", "prix": 0, "important": True,
         "description": "Un galet qui murmure quand on le serre dans la main. Le Colosse d'écho le gardait."},
        {"id": "echo_flamme", "nom": "Écho de flamme", "icone": "ic_gemme_feu", "prix": 0, "important": True,
         "description": "Une braise qui ne brûle pas, et qui chante tout bas. Le Phénix d'écho la gardait."},
        {"id": "coeur_echo", "nom": "Cœur d'écho", "icone": "ic_acc_8", "prix": 9000, "equipement": "accessoire",
         "description": "Le dernier écho du Premier Gardien, devenu calme. Il bat au rythme de celui qui le porte.",
         "bonus": {"pv": 80, "attaque": 8, "magie": 8, "vitesse": 4, "critique": 3}}]):
    OBJ.insert(k + i2, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')

# =====================================================================================================================
# Événements, quête, discussions
# =====================================================================================================================


def say(t): return {"action": "dire", "texte": t}


EV = {
    "echos_arrivee": {"actions": [{"action": "drapeau", "nom": "echos_vue"},
                                  say("Une île de pierre grise, où chaque bruit revient trois fois. Un village de pêcheurs s'accroche aux rochers."),
                                  say("Au nord, une forêt d'arbres de pierre. À l'est, une montagne fume.")]},
    "echos_soin": {"actions": [{"action": "soigner"}, {"action": "reveil"}, say("Soigneuse : Reposez-vous. Ici, même le silence fait écho.")]},
    "echos_boutique": {"actions": [{"action": "boutique", "objets": ["superpotion", "elixir", "ether", "remede", "plume", "lanterne_or",
                                                                       "gemme_vie", "gemme_soin", "gemme_force", "gemme_esprit"]}]},
    "echos_maison": {"actions": [say("La maison d'Oriane. Des coquillages pendent au plafond ; quand le vent passe, ils chantent.")]},
    "oriane": {"pages": [
        {"si": {"drapeau": "echos_fin"}, "actions": [say("Oriane : L'écho s'est tu. Le Premier Gardien repose enfin. Merci, gardiens.")]},
        {"si": {"drapeau": ["colosse_echo_vaincu", "phenix_echo_vaincu"]}, "actions": [
            say("Oriane : Les deux échos sont libres. La porte du sanctuaire, au nord de la forêt de pierre, s'est ouverte."),
            say("Oriane : L'Ombre du Premier Gardien vous y attend. Elle a sa force, mais plus son cœur. Rendez-le-lui.")]},
        {"si": {"drapeau": "quete:echos"}, "actions": [
            say("Oriane : Le Colosse dort dans la forêt de pierre, au nord. Le Phénix tourne au-dessus du cratère, à l'est.")]},
        {"actions": [
            say("Oriane : Je vous attendais. Je suis Oriane, la gardienne de cette île. Les anciens l'appelaient l'île des Échos."),
            say("Oriane : Quand le Premier Gardien a enfermé l'orage, il a laissé ici l'écho de sa force, pour veiller si tout recommençait."),
            say("Oriane : Mais la Brume Noire l'a touché avant de mourir. Son écho est devenu une ombre, et l'ombre a réveillé les deux gardiens de l'île."),
            say("Oriane : Le Colosse de la forêt de pierre, et le Phénix du cratère. Libérez-les : leurs échos ouvriront le sanctuaire."),
            {"action": "quete", "id": "echos"}]}]},
    "colosse_echo": {"actions": [
        say("Au milieu de la clairière, un arbre de pierre se lève. Ce n'était pas un arbre : c'est un colosse couvert de mousse grise."),
        say("Colosse d'écho : … QUI … QUI … QUI … réveille … la forêt ?"),
        {"action": "question", "texte": "Affronter le Colosse d'écho ?", "oui": [
            {"action": "combat", "musique": "boss", "ennemis": [
                {"espece": "petrolin", "niveau": 85},
                {"espece": "colosse_echo", "niveau": 88, "pv": 5.5, "boss": True, "bouclier": 12,
                 "phases": [{"pv": 0.5, "message": "Le Colosse se couvre de pierre : sa défense grandit, et la forêt envoie de l'aide !",
                             "bonus": {"defense": 1}, "renforts": [{"espece": "petrolin", "niveau": 85}]}]},
                {"espece": "fossiloup", "niveau": 85}], "renforts": [{"espece": "fossiloup", "niveau": 85}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [{"action": "drapeau", "nom": "colosse_echo_vaincu"}, {"action": "donner", "objet": "echo_pierre", "quantite": 1},
                          say("Le Colosse se rassoit lentement. La mousse grise tombe : dessous, l'écorce redevient vivante."),
                          say("Colosse d'écho : … Merci … merci … merci … Prenez … mon écho.")]}],
         "non": [say("Le Colosse se fige de nouveau. On dirait un arbre comme les autres.")]}]},
    "phenix_echo": {"actions": [
        say("Sur le rebord du cratère, un oiseau de feu se pose. Ses ailes soulèvent des tourbillons de cendres."),
        say("Phénix d'écho : L'ombre m'a dit que vous viendrez éteindre ma flamme. Je brûlerai d'abord !"),
        {"action": "question", "texte": "Affronter le Phénix d'écho ?", "oui": [
            {"action": "combat", "musique": "boss", "ennemis": [
                {"espece": "cendrelys", "niveau": 86},
                {"espece": "phenix_echo", "niveau": 89, "pv": 5.5, "boss": True, "bouclier": 12,
                 "phases": [{"pv": 0.6, "message": "Le Phénix bat des ailes : l'air du cratère devient brûlant !", "meteo": "canicule"},
                            {"pv": 0.3, "message": "Le Phénix renaît de ses cendres : ses plumes flambent de nouveau !",
                             "bonus": {"magie": 1, "vitesse": 1}, "renforts": [{"espece": "cendrelys", "niveau": 86}]}]},
                {"espece": "braisaure", "niveau": 86}], "renforts": [{"espece": "braisaure", "niveau": 86}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [{"action": "drapeau", "nom": "phenix_echo_vaincu"}, {"action": "donner", "objet": "echo_flamme", "quantite": 1},
                          say("Le Phénix se pose, épuisé. Sa flamme devient douce, presque dorée."),
                          say("Phénix d'écho : L'ombre m'avait menti… Tenez. Mon écho vous ouvrira le chemin.")]}],
         "non": [say("Le Phénix reprend son vol au-dessus du cratère.")]}]},
    "sanctuaire_echos_boss": {"actions": [
        say("Au fond du sanctuaire, une silhouette en armure se tient immobile. Elle a les traits du Premier Gardien, mais ses yeux sont vides."),
        say("Ombre du Premier Gardien : Encore des gardiens… J'en ai vu mourir mille. Vous serez les derniers."),
        {"action": "question", "texte": "Affronter l'Ombre du Premier Gardien ?", "oui": [
            {"action": "combat", "musique": "boss_final", "ennemis": [
                {"espece": "spectrecho", "niveau": 90},
                {"espece": "ombre_gardien", "niveau": 95, "pv": 7, "boss": True, "bouclier": 16,
                 "phases": [{"pv": 0.7, "message": "L'Ombre lève son épée : une lumière noire l'entoure !", "bonus": {"magie": 1}},
                            {"pv": 0.45, "message": "L'Ombre appelle les échos du sanctuaire !",
                             "renforts": [{"espece": "spectrecho", "niveau": 90}, {"espece": "echoplume", "niveau": 90}]},
                            {"pv": 0.2, "message": "L'Ombre vacille… puis frappe de toutes ses forces !", "bonus": {"attaque": 1, "vitesse": 1}}]},
                {"espece": "echoplume", "niveau": 90}], "renforts": [{"espece": "braisaure", "niveau": 90}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [{"action": "drapeau", "nom": "sanctuaire_echos_boss"}, {"action": "drapeau", "nom": "echos_fin"},
                          say("L'Ombre baisse son épée. Peu à peu, la lumière revient dans ses yeux."),
                          say("Premier Gardien : … Lior. C'est bien ton nom ? Tu tiens mon épée comme je la tenais, autrefois. Mal, et sans jamais la lâcher."),
                          say("Premier Gardien : Mon écho n'a plus rien à garder. Le monde a de nouveaux gardiens. Prends ce qu'il reste de moi."),
                          {"action": "donner", "objet": "coeur_echo", "quantite": 1}, {"action": "or", "quantite": 20000},
                          {"action": "quete", "id": "echos", "fin": True},
                          {"action": "fin", "texte": "L'écho du Premier Gardien s'est tu. Sur l'île des Échos, chaque bruit ne revient plus qu'une fois : celui des vagues. Lior et ses amis rentrent à Havre-d'Aube, où Ilya les attend avec un sourire."}]}],
         "non": [say("L'Ombre ne bouge pas. Elle attend.")]}]},
}
Q = {"id": "echos", "nom": "L'île des Échos", "lieu": "Port-Écho",
     "etapes": [{"texte": "Libérer les deux gardiens de l'île des Échos : le Colosse de la forêt de pierre (nord) et le Phénix du cratère (est)."},
                {"si": {"drapeau": "colosse_echo_vaincu"}, "texte": "Le Colosse est libre."},
                {"si": {"drapeau": "phenix_echo_vaincu"}, "texte": "Le Phénix est libre."},
                {"si": {"drapeau": ["colosse_echo_vaincu", "phenix_echo_vaincu"]},
                 "texte": "Les deux échos ouvrent le sanctuaire, au nord de la forêt de pierre : y apaiser l'Ombre du Premier Gardien."}],
     "fin_texte": "L'Ombre du Premier Gardien est apaisée."}
EVS = json.load(open(D + 'evenements.json', encoding='utf-8'))
EVS['ysolde_aube'] = {"pages": [
    {"si": {"drapeau": "aube_fin"}, "actions": [
        say("Ysolde : Vous avez vu ? Depuis que la Brume Noire s'est dissipée, une île est apparue au nord-ouest. Personne ne l'avait jamais vue."),
        say("Ysolde : J'ai amarré une barque au bout de la nouvelle jetée, à l'ouest de la plage. Elle est à vous, si le cœur vous en dit.")]},
    {"actions": EVS['ysolde_aube']['actions']}]}
carte.rewrite_events(['ysolde_aube'], EVS)
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
json.load(open(D + 'evenements.json', encoding='utf-8'))
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)

DI = [("echos", "Trois fois", {"drapeau": "echos_vue"}, ["lior", "maelle"], [
          "Maëlle : Ohé ! … Ohé ! … Ohé !",
          "Lior : Maëlle…",
          "Maëlle : Quoi ? L'île répond ! C'est la première fois qu'une île me répond.",
          "Lior : Elle répond trois fois. Ça veut dire qu'elle a beaucoup de choses à dire."]),
      ("premier_gardien", "Mal, et sans jamais la lâcher", {"drapeau": "echos_fin"}, ["lior", "selene"], [
          "Sélène : « Mal, et sans jamais la lâcher. » C'est exactement ce que je t'avais dit, à Givreval.",
          "Lior : Tu crois que le Premier Gardien était aussi mauvais que moi, au début ?",
          "Sélène : Je crois que tous les gardiens commencent mal. Ce qui compte, c'est de ne jamais lâcher.",
          "Lior : … C'était un compliment ?",
          "Sélène : Le deuxième de l'année. Ne t'habitue pas."])]
p = D + 'discussions.json'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
body = s.rstrip()[:-1].rstrip()


def jl(x): return json.dumps(x, ensure_ascii=False, separators=(', ', ': '))


for i, t, si, mem, dlg in DI:
    body += ',' + nl + '  {' + nl + '    "id": %s,' % jl(i) + nl + '    "titre": %s,' % jl(t) + nl + '    "si": %s,' % jl(si) + nl + \
        '    "membres": %s,' % jl(mem) + nl + '    "dialogue": [' + nl + (',' + nl).join('      ' + jl(l) for l in dlg) + nl + '    ]' + nl + '  }'
open(p, 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)
json.load(open(p, encoding='utf-8'))
print("Acte 4 écrit : l'île des Échos (lancer ensuite outils/sprites/decoupe.py)")
