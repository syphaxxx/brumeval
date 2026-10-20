"""Cartes des gardiens (étape « 20 heures », 2026-10-20) : les joueurs de cartes (src/cartes.cpp). Écrit
data/cartes_jeu.json (cinq cartes de départ, neuf adversaires de plus en plus forts), place les joueurs dans les auberges
et les maisons (un habitant sans rien à dire devient joueur quand il y en a un), leurs événements (action « cartes »),
la quête « cartes » et la récompense du Maître des cartes ; deux exploits de plus. Une seule fois (refuse si
data/cartes_jeu.json existe).

    python outils/villes/cartes.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util as pu  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes_jeu.json'):
    sys.exit('Déjà fait.')

# id, intérieur, apparence (None : l'habitant muet de l'intérieur), nom, cartes, niveau de jeu, or, carte gagnée, condition
FOES = [
    ("pip", "vallee_boutique", "enfant", "Pip", ["mulotin", "piafouine", "champichou", "lucioline", "rocaillou"], 0, 200, "louvet", None),
    ("meunier", "plaine_maison", None, "Le meunier", ["rongecrin", "louvet", "lucioline", "rocaillou", "grenouillon"], 0, 400, "sylvarque", None),
    ("mineur", "cendrelune_auberge", "montagnard", "Gunnar le mineur", ["tisonnel", "galetor", "voltigeon", "fumerole", "draconnet"], 1, 800, "ignarok", None),
    ("chevaliere", "pics_auberge", "chevalier", "Dame Elwen", ["givrelin", "zephyrin", "cristallin", "etincelot", "banquisot"], 1, 1200, "givrecorne", None),
    ("matelot", "port_auberge", "pecheur", "Matelot Bastien", ["ecumet", "voltanguille", "orageline", "noyelin", "nuageon"], 1, 2000, "mere_brumes", None),
    ("mousse", "corail_maison", None, "Lou, la mousse", ["corailleur", "perlin", "murenix", "ecumet", "bastillon"], 2, 3000, "seigneur_orage", None),
    ("voyageuse", "aube_auberge", "voyageuse", "Mira la voyageuse", ["lueurine", "loup_aube", "aubelle", "vasepent", "crapale"], 2, 4000, "brume_noire", None),
    ("ermite", "echos_maison", "ermite", "L'ermite des échos", ["petrolin", "fossiloup", "echoplume", "spectrecho", "loup_aube"], 2, 6000, "ombre_gardien", None),
]
MASTER = ("maitre", "port_auberge", "ancien", "Le Maître des cartes", ["mere_brumes", "seigneur_orage", "brume_noire", "ombre_gardien", "veilleur_phare"],
          2, 10000, "veilleur_phare", {"drapeau": ["cartes:" + f[0] for f in FOES]})

LINES = {
    "pip": ("Pip : Tu joues aux Cartes des gardiens ? Moi, je suis le meilleur de la vallée ! Enfin, de la boutique.",
            "Pip : Tu as triché ! … Non ? Bon. Je m'entraîne et on rejoue.", "Pip : Gagné ! Reviens quand tu veux perdre encore."),
    "meunier": ("Le meunier : Une partie, entre deux sacs de farine ? Je ne dis jamais non.",
                "Le meunier : Bien joué. Le vent tourne, comme les ailes du moulin.", "Le meunier : Hé hé. La farine rend malin."),
    "mineur": ("Gunnar : À Forgeroc, on joue aux cartes avec des créatures de feu. Ça chauffe, je te préviens.",
               "Gunnar : Par ma pioche ! Tu as la main chaude.", "Gunnar : Tu reviendras quand tu auras de meilleures cartes."),
    "chevaliere": ("Dame Elwen : Un chevalier doit savoir manier l'épée… et les cartes. En garde !",
                   "Dame Elwen : Une belle victoire. Je m'incline.", "Dame Elwen : La glace ne pardonne pas. Ni moi."),
    "matelot": ("Bastien : Les marins jouent pendant les longues nuits en mer. Tu veux essayer ?",
                "Bastien : Coulé ! Tu joues comme un vieux loup de mer.", "Bastien : La mer a gagné. Comme toujours."),
    "mousse": ("Lou : Personne ne veut jouer avec moi sur l'île. Toi, tu veux bien ?",
               "Lou : Tu es fort ! Tu m'apprendras ?", "Lou : J'ai gagné ! J'ai gagné ! Encore !"),
    "voyageuse": ("Mira : J'ai joué dans tout l'archipel. Voyons ce que tu vaux.",
                  "Mira : Rares sont ceux qui me battent. Je retiens ton nom.", "Mira : Pas mal. Mais pas assez."),
    "ermite": ("L'ermite : Les échos m'apprennent à jouer. Ils répètent chacun de tes coups… avant que tu les joues.",
               "L'ermite : Les échos se sont tus. Tu as joué juste.", "L'ermite : Les échos avaient raison."),
    "maitre": ("Le Maître des cartes : Tu as battu les huit joueurs de l'archipel. Il ne reste que moi. Mes cartes sont celles des gardiens eux-mêmes.",
               "Le Maître des cartes : … Le titre est à toi. Le jeu a un nouveau maître.", "Le Maître des cartes : Les gardiens ne se laissent pas battre si facilement."),
}


def say(t): return {"action": "dire", "texte": t}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}
def qstart(i): return {"action": "quete", "id": i}
def qend(i): return {"action": "quete", "id": i, "fin": True}


maps, before, EV = {}, {}, {}
doc = {"depart": ["mulotin", "piafouine", "champichou", "grenouillon", "brumelin"], "adversaires": []}
for fid, mid, look, name, deck, lvl, gold, card, cond in FOES + [MASTER]:
    doc["adversaires"].append({"id": fid, "nom": name, "cartes": deck, "niveau": lvl, "or": gold, "carte": card})
    if mid not in maps:
        maps[mid] = carte.load(mid)
        before[mid] = pu.reachable(maps[mid])
    m = maps[mid]
    ev = "cartes_" + fid
    mute = [h for h in m['habitants'] if 'evenement' not in h and 'dialogue' not in h and h.get('apparence') not in ('chat', 'chien')]
    if look is None and mute:
        mute[0]['evenement'] = ev
    else:
        p = pu.near_reach(m, 3 if fid != "maitre" else 8, 6, ring=0)
        assert p, (mid, fid)
        o = {"x": p[0], "y": p[1], "apparence": look or "villageois", "direction": "bas", "evenement": ev}
        if cond:
            o["si"] = cond
        m['habitants'].append(o)
    hello, win, lose = LINES[fid]
    first = [say(hello)]
    if fid == "pip":  # le premier joueur : la quête des cartes
        first = [say(hello), qstart("cartes"), say("Pip : Chaque créature que tu obtiens te donne sa carte. Et voilà cinq cartes pour commencer !")]
    victory = [say(win)]
    if fid == "maitre":
        victory = [say(win), give("as_gardiens"), qend("cartes")]
    EV[ev] = {"actions": first + [{"action": "cartes", "adversaire": fid, "victoire": victory, "defaite": [say(lose)]}]}

ITEM = {"id": "as_gardiens", "nom": "As des gardiens", "icone": "ic_acc_8", "prix": 9000, "equipement": "accessoire",
        "description": "La carte la plus rare du jeu, montée en broche. Le Maître des cartes la donne à celui qui l'a battu.",
        "bonus": {"pv": 30, "vitesse": 4, "esquive": 4, "critique": 8}}
QS = [{"id": "cartes", "nom": "Les Cartes des gardiens", "lieu": "Vallée de Brumeval",
       "etapes": [{"texte": "Battre les joueurs de cartes de l'archipel (Vallée, Plaine, Forgeroc, Givreval, port, Corail, Aube, Port-Écho)."},
                  {"si": MASTER[8], "texte": "Les huit joueurs sont battus : le Maître des cartes attend à l'auberge du port."}],
       "fin_texte": "Vous êtes le nouveau Maître des cartes."}]

for mid, m in maps.items():
    lost = before[mid] - pu.reachable(m) - set((o['x'], o['y']) for o in m['habitants'])
    if lost:
        sys.exit('%s : des cases deviennent inaccessibles %s' % (mid, sorted(lost)[:5]))
for mid, m in maps.items():
    carte.save(mid, m)
open(D + 'cartes_jeu.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(doc) + '\n')
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
body = qt.rstrip()[:-1].rstrip()
for q in QS:
    body += ',' + nl + json.dumps(q, ensure_ascii=False)
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('lanterne_veilleur') + 1
OBJ.insert(k, ITEM)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
ex = open(D + 'exploits.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in ex else '\n'
body = ex.rstrip()[:-1].rstrip()
for e in [{"id": "cartes", "nom": "Maître des cartes", "description": "Battre le Maître des cartes, à l'auberge du port.", "si": {"drapeau": "cartes:maitre"}},
          {"id": "cartes40", "nom": "Collectionneur de cartes", "description": "Avoir 40 cartes des gardiens.", "si": {"cartes_min": 40}}]:
    body += ',' + nl + '  ' + json.dumps(e, ensure_ascii=False)
open(D + 'exploits.json', 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)
for mid, m in maps.items():
    print(mid, [(h['x'], h['y'], h.get('apparence'), h.get('evenement')) for h in m['habitants']])
print('9 joueurs de cartes')
