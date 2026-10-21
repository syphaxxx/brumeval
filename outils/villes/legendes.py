"""Les Échos des légendes (étape « 20 heures », 2026-10-20) : après l'acte 4 (`echos_fin`), la gardienne des échos de
Port-Écho réveille l'écho des grands boss de l'aventure. Cinq cercles de trois
combats d'affilée, sans soin entre eux (comme la Ligue), du niveau 98 au niveau 120 ; chaque cercle gagné donne de
l'or, des gemmes ou un équipement unique. Drapeaux `legende1`… `legende5`, quête `legendes`, un exploit. Une seule fois
(refuse si la quête « legendes » existe).

    python outils/villes/legendes.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util as pu  # noqa: E402

D = carte.R
QT = [json.loads(l.strip().rstrip(',')) for l in open(D + 'quetes.json', encoding='utf-8') if l.strip().startswith('{')]
if any(q['id'] == 'legendes' for q in QT):
    sys.exit('Déjà fait.')
SP = {s['id']: s for s in json.load(open(D + 'especes.json', encoding='utf-8'))}


def say(t): return {"action": "dire", "texte": t}


# cercle : (nom, niveau, [(boss, nom affiché, escorte)], récompenses, or)
CERCLES = [
    ("premier", 98, [("sylvarque", "Écho de Sylvarque", "brumeloup"), ("ignarok", "Écho d'Ignarok", "fournezard"),
                     ("givrecorne", "Écho du Givrecorne", "givrenard")], [("gemme_maitre", 1)], 15000),
    ("deuxième", 103, [("mere_brumes", "Écho de la Mère des Brumes", "nebulis"), ("kraken", "Écho du Kraken", "murenix"),
                       ("griffon", "Écho du Griffon", "fulgoray")], [("gemme_maitre", 1), ("elixir", 3)], 20000),
    ("troisième", 108, [("seigneur_orage", "Écho du Seigneur des Orages", "orageline"), ("gardien_lueurs", "Écho du Gardien des Lueurs", "lueurine"),
                        ("reine_marais", "Écho de la Reine du Marais", "vasepent")], [("medaillon_legendes", 1)], 25000),
    ("quatrième", 113, [("brume_noire", "Écho de la Brume Noire", "aubespectre"), ("colosse_echo", "Écho du Colosse", "petrolin"),
                        ("phenix_echo", "Écho du Phénix", "echoplume")], [("gemme_maitre", 2), ("elixir", 5)], 30000),
    ("dernier", 118, [("orage_ancien", "Écho de l'Orage ancien", "fossiloup"), ("veilleur_phare", "Écho du Veilleur", "spectrecho"),
                      ("ombre_gardien", "Écho de l'Ombre du Premier Gardien", "loup_aube")], [("couronne_echos", 1)], 50000),
]
ITEMS = [
    {"id": "medaillon_legendes", "nom": "Médaillon des légendes", "icone": "ic_acc_4", "prix": 10000, "equipement": "accessoire",
     "description": "Un médaillon où dorment les échos de trois légendes. On les entend murmurer pendant les combats.",
     "bonus": {"pv": 60, "attaque": 8, "magie": 8, "vitesse": 4, "critique": 4}},
    {"id": "couronne_echos", "nom": "Couronne des échos", "icone": "ic_acc_8", "prix": 15000, "equipement": "accessoire",
     "description": "La couronne de celui qui a vaincu tous les échos. Tous les gardiens de l'histoire l'ont un jour portée.",
     "bonus": {"pv": 120, "attaque": 10, "magie": 10, "defense": 8, "resistance": 8, "vitesse": 6, "critique": 5}},
]


def evolved(sp, lvl):
    s = SP[sp]
    while s.get('evolution') and s['evolution'].get('niveau', 999) <= lvl and s['evolution'].get('espece') in SP:
        s = SP[s['evolution']['espece']]
    return s['id']


for c in CERCLES:
    for b, _, e in c[2]:
        assert b in SP and e in SP, (b, e)

pages = [{"si": {"drapeau": "legende5"}, "actions": [
    say("Gardienne des échos : Tous les échos se sont tus. Ils dorment en paix, maintenant. Merci, gardiens.")]}]
for k in range(len(CERCLES), 0, -1):
    nom, lvl, fights, rewards, gold = CERCLES[k - 1]
    flag = 'legende%d' % k
    reward = [{"action": "drapeau", "nom": flag}, say("Gardienne des échos : Le %s cercle est apaisé. Les échos se reposent." % nom), {"action": "or", "quantite": gold}]
    for it, q in rewards:
        reward.append({"action": "donner", "objet": it, "quantite": q})
    if k == len(CERCLES):
        reward += [{"action": "quete", "id": "legendes", "fin": True},
                   say("Gardienne des échos : Vous avez vaincu l'écho de chaque légende. Aucun gardien n'avait jamais fait ça.")]
    chain = reward
    for j in range(len(fights) - 1, -1, -1):  # trois combats d'affilée : chaque victoire lance le suivant
        boss, label, escort = fights[j]
        lv = lvl + j
        chain = [say("Gardienne des échos : %s se lève !" % label),
                 {"action": "combat", "nom": label, "musique": "boss_final" if k == len(CERCLES) and j == len(fights) - 1 else "boss",
                  "ennemis": [{"espece": boss, "niveau": lv, "pv": 4, "boss": True, "bouclier": 10,
                               "phases": [{"pv": 0.5, "message": "L'écho se renforce !", "bonus": {"attaque": 1, "magie": 1}}]},
                              {"espece": evolved(escort, lv), "niveau": lv - 6}],
                  "boss": True, "fuite": False, "capture": False, "victoire": chain}]
    intro = [say("Gardienne des échos : Le %s cercle : trois échos d'affilée, sans soin entre eux." % nom)]
    if k == 1:
        intro = [say("Gardienne des échos : L'île garde l'écho de tout ce qui s'est passé. Même celui des grandes légendes que vous avez apaisées."),
                 say("Gardienne des échos : Je peux les réveiller. Cinq cercles, trois échos chacun. Ils ne vous feront pas de cadeau."),
                 {"action": "quete", "id": "legendes"}]
    pages.append({"si": {"drapeau": ["legende%d" % (k - 1), "echos_fin"]} if k > 1 else {"drapeau": "echos_fin"},
                  "actions": intro + [{"action": "question", "texte": "Réveiller les échos du %s cercle ?" % nom, "oui": chain,
                                       "non": [say("Gardienne des échos : Les échos attendront. Ils ont tout leur temps.")]}]})
EV = {'legendes': {"pages": pages + [{"actions": [say("Gardienne des échos : L'île est encore troublée par l'ombre. Revenez quand elle sera apaisée.")]}]}}

m = carte.load('echos')
before = pu.reachable(m)
p = pu.near_reach(m, 28, 16, ring=1)
assert p, 'gardienne des échos'
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "villageoise", "direction": "bas", "evenement": "legendes"})
if before - pu.reachable(m) - {tuple(p)}:
    sys.exit('echos : des cases deviennent inaccessibles')
carte.save('echos', m)

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
json.load(open(D + 'evenements.json', encoding='utf-8'))
Q = {"id": "legendes", "nom": "Les Échos des légendes", "lieu": "Port-Écho",
     "etapes": [{"texte": "Apaiser les échos des légendes, cercle après cercle, avec la gardienne des échos de Port-Écho."}] +
              [{"si": {"drapeau": "legende%d" % k}, "texte": "%s cercle apaisé." % CERCLES[k - 1][0].capitalize()} for k in range(1, 5)],
     "fin_texte": "Tous les échos des légendes sont apaisés."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('lanterne_veilleur') + 1
for i2, o in enumerate(ITEMS):
    OBJ.insert(k + i2, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
ex = open(D + 'exploits.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in ex else '\n'
e = {"id": "legendes", "nom": "Plus fort que les légendes", "description": "Apaiser les cinq cercles des Échos des légendes, à Port-Écho.", "si": {"drapeau": "legende5"}}
open(D + 'exploits.json', 'w', encoding='utf-8', newline='').write(ex.rstrip()[:-1].rstrip() + ',' + nl + '  ' + json.dumps(e, ensure_ascii=False) + nl + ']' + nl)
print('Échos des légendes écrits : gardienne en', p)
