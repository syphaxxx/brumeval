"""La Ligue du port (étape « 20 heures », 2026-10-20) : la maîtresse de la Ligue, au nord du Port d'Écume, propose
quatre rangs (Bronze après la Mère des Brumes, Argent après le Seigneur des Orages, Or après l'acte 3, Champion après
les épreuves des gardiens). Chaque rang est une série de trois combats d'affilée, sans soin entre eux ; la victoire
donne un équipement unique et de l'or. Drapeaux `ligue1`… `ligue4`, quête `ligue`. Une seule fois (refuse si la quête
« ligue » existe).

    python outils/villes/ligue.py
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
if any(q['id'] == 'ligue' for q in QT):
    sys.exit('Déjà fait.')
SP = {s['id']: s for s in json.load(open(D + 'especes.json', encoding='utf-8'))}


def say(t): return {"action": "dire", "texte": t}


# rang : (nom, condition, niveau, [(adversaire, réplique, [espèces], [renforts])], récompense, or)
RANGS = [
    ("Bronze", "boss4", 47, [
        ("Marin Brice", "Marin Brice : Premier combat ! Mes créatures ont le pied marin, et vous ?",
         ['voltanguille', 'coquillon', 'ecumet'], ['noyelin']),
        ("Dresseuse Ondine", "Dresseuse Ondine : Pas le temps de souffler ! À moi de jouer.",
         ['orageline', 'nuageon', 'zephyrin'], ['voltigeon']),
        ("Capitaine Hugo", "Capitaine Hugo : Le dernier du rang Bronze, c'est moi. Et je ne coule jamais !",
         ['bastillon', 'voltanguille', 'chevalier'], ['coquillon'])],
     {"id": "echarpe_lutteur", "nom": "Écharpe du lutteur", "icone": "ic_acc_2",
      "description": "L'écharpe rouge des vainqueurs du rang Bronze de la Ligue.", "prix": 2600, "equipement": "accessoire",
      "bonus": {"attaque": 8, "vitesse": 3, "critique": 3}}, 3000),
    ("Argent", "boss5", 57, [
        ("Mage Célestin", "Mage Célestin : Le rang Argent ne pardonne pas. Ma magie non plus.",
         ['mage_givre', 'orbrume', 'nebulis'], ['lanternelle']),
        ("Chevalière Isaure", "Chevalière Isaure : Vous avez tenu un combat. Tiendrez-vous le deuxième ?",
         ['chevalier', 'ferrogeant', 'givrenard'], ['galetor']),
        ("Dompteur Barnabé", "Dompteur Barnabé : Mes bêtes ont mangé des champions au petit-déjeuner !",
         ['brumeloup', 'fournezard', 'draconnet'], ['fulgoray'])],
     {"id": "cape_stratege", "nom": "Cape du stratège", "icone": "ic_acc_6",
      "description": "La cape gris argent des vainqueurs du rang Argent. On dit qu'elle aide à voir venir les coups.",
      "prix": 4200, "equipement": "accessoire", "bonus": {"defense": 10, "resistance": 10, "esquive": 3}}, 6000),
    ("Or", "aube_fin", 80, [
        ("Gardienne Liora", "Gardienne Liora : Les créatures d'Aube sont avec moi. Prêts ?",
         ['loup_aube', 'lueurine', 'aubelle'], ['nacrelin']),
        ("Sage Ambroise", "Sage Ambroise : J'ai vu tomber l'orage. Je ne tomberai pas, moi.",
         ['aubespectre', 'rocaube', 'vasepent'], ['crapale']),
        ("Championne Maëva", "Championne Maëva : Le rang Or, c'est moi. Et je ne l'ai jamais perdu.",
         ['murenix', 'draconnet', 'loup_aube'], ['corailleur'])],
     {"id": "couronne_colisee", "nom": "Couronne de la Ligue", "icone": "ic_acc_8",
      "description": "Une couronne d'or ciselée de vagues. Elle n'appartient qu'aux vainqueurs du rang Or.",
      "prix": 8000, "equipement": "accessoire", "bonus": {"pv": 40, "attaque": 6, "magie": 6, "defense": 6, "resistance": 6, "vitesse": 3}}, 12000),
    ("Champion", "epreuve9", 92, [
        ("Ancien champion Edgar", "Ancien champion Edgar : On m'a fait revenir pour vous. J'espère que vous en valez la peine.",
         ['ferrogeant', 'fournezard', 'givrenard'], ['brumeloup']),
        ("Archimage Solène", "Archimage Solène : Vous avez battu Ilya ? Alors je ne retiendrai pas mes coups.",
         ['mage_givre', 'aubespectre', 'orbrume'], ['lueurine']),
        ("Maîtresse Ysaline", "Maîtresse de la Ligue : Personne n'a jamais atteint ce combat. Montrez-moi tout !",
         ['chevalier', 'draconnet', 'loup_aube'], ['murenix'])],
     None, 30000),
]


def evolved(sp, lvl):
    s = SP[sp]
    while s.get('evolution') and s['evolution'].get('niveau', 999) <= lvl and s['evolution'].get('espece') in SP:
        s = SP[s['evolution']['espece']]
    return s['id']


ITEMS, EV = [], {}
for sp in [s for r in RANGS for f in r[3] for s in f[2] + f[3]]:
    assert sp in SP, sp
pages = [{"si": {"drapeau": "ligue4"}, "actions": [
    say("Maîtresse de la Ligue : Les champions de la Ligue ! Votre nom est gravé au-dessus de la porte. Revenez me voir quand vous voulez.")]}]
for k in range(len(RANGS), 0, -1):
    nom, cond, lvl, fights, item, gold = RANGS[k - 1]
    flag = 'ligue%d' % k
    reward = [{"action": "drapeau", "nom": flag}, say("Maîtresse de la Ligue : Le rang %s est à vous ! Bravo." % nom), {"action": "or", "quantite": gold}]
    if item:
        ITEMS.append(item)
        reward.append({"action": "donner", "objet": item['id'], "quantite": 1})
    else:
        reward += [{"action": "donner", "objet": "gemme_maitre", "quantite": 1}, {"action": "quete", "id": "ligue", "fin": True},
                   say("Maîtresse de la Ligue : Vous êtes les champions de la Ligue d'Écume. Le port tout entier va fêter ça !")]
    chain = reward
    for j in range(len(fights) - 1, -1, -1):  # trois combats d'affilée : chaque victoire lance le suivant
        foe, line, team, renf = fights[j]
        lv = lvl + j
        chain = [say(line), {"action": "combat", "nom": foe, "ennemis": [{"espece": evolved(s, lv), "niveau": lv + (1 if i == 0 and j == 2 else 0)} for i, s in enumerate(team)],
                             "renforts": [{"espece": evolved(s, lv), "niveau": lv} for s in renf], "fuite": False, "capture": False, "victoire": chain}]
    intro = [say("Maîtresse de la Ligue : Le rang %s : trois combats d'affilée, sans soin entre eux. Une défaite, et tout est à refaire." % nom)]
    if k == 1:
        intro = [say("Maîtresse de la Ligue : Bienvenue à la Ligue d'Écume ! Ici, les meilleurs dresseurs de l'archipel s'affrontent."),
                 say("Maîtresse de la Ligue : Quatre rangs : Bronze, Argent, Or et Champion. Chaque rang, ce sont trois combats d'affilée, sans soin entre eux."),
                 {"action": "quete", "id": "ligue"}]
    pages.append({"si": {"drapeau": ["ligue%d" % (k - 1), cond]} if k > 1 else {"drapeau": cond},
                  "actions": intro + [{"action": "question", "texte": "Tenter le rang %s ?" % nom, "oui": chain,
                                       "non": [say("Maîtresse de la Ligue : Revenez quand vous serez prêts.")]}]})
    if k > 1:
        pages.append({"si": {"drapeau": "ligue%d" % (k - 1)}, "actions": [
            say("Maîtresse de la Ligue : Bravo pour le rang %s ! Le rang %s ouvrira plus tard : vous n'êtes pas encore prêts." % (RANGS[k - 2][0], nom))]})
# pages du rang le plus haut au plus bas (la première page vraie joue), chacune suivie de « pas encore prêts »
EV['ligue'] = {"pages": pages + [{"actions": [say("Maîtresse de la Ligue : La Ligue d'Écume n'accepte que les dresseurs qui ont apaisé la Mère des Brumes. Revenez plus tard !")]}]}

m = carte.load('port')
before = pu.reachable(m)
p = pu.near_reach(m, 30, 4, ring=1)
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "garde", "direction": "bas", "evenement": "ligue"})
for dx in (-2, 2):
    if m['tuiles'][p[1] - 1][p[0] + dx] == '.':
        row = m['tuiles'][p[1] - 1]
        m['tuiles'][p[1] - 1] = row[:p[0] + dx] + 'J' + row[p[0] + dx + 1:]
lost = before - pu.reachable(m) - {tuple(p)} - {(p[0] - 2, p[1] - 1), (p[0] + 2, p[1] - 1)}
if lost:
    sys.exit('port : des cases deviennent inaccessibles %s' % sorted(lost)[:5])
carte.save('port', m)

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
json.load(open(D + 'evenements.json', encoding='utf-8'))
Q = {"id": "ligue", "nom": "La Ligue d'Écume", "lieu": "Port d'Écume",
     "etapes": [{"texte": "Gagner le rang Bronze de la Ligue d'Écume (trois combats d'affilée)."},
                {"si": {"drapeau": "ligue1"}, "texte": "Rang Bronze gagné. Le rang Argent ouvre après le Seigneur des Orages."},
                {"si": {"drapeau": "ligue2"}, "texte": "Rang Argent gagné. Le rang Or ouvre après la fin de l'acte 3."},
                {"si": {"drapeau": "ligue3"}, "texte": "Rang Or gagné. Le rang Champion ouvre après les épreuves des gardiens."}],
     "fin_texte": "Vous êtes les champions de la Ligue d'Écume."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('recueil_orages') + 1
for i2, o in enumerate(ITEMS):
    OBJ.insert(k + i2, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
print('Ligue écrite : maîtresse en', p)
