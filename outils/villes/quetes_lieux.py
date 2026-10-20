"""Cinq quêtes de plus dans les nouveaux lieux (étape « 20 heures », 2026-10-20) : le chien du berger (Col des
Hurlevents), le collier de perles (île de Corail), le dernier message du pêcheur (Val des Murmures → Port), le tournoi
de Forgeroc, et l'étude des créatures d'Aube. Une seule fois (refuse si la quête « chien » existe).

    python outils/villes/quetes_lieux.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util  # noqa: E402

D = carte.R
QT = json.load(open(D + 'quetes.json', encoding='utf-8'))
if any(q['id'] == 'chien' for q in QT):
    sys.exit('Déjà fait.')

maps = {}


def M(mid):
    if mid not in maps:
        maps[mid] = carte.load(mid)
    return maps[mid]


def npc(mid, x, y, look, ev, d='bas', si=None, cache=None):
    m = M(mid)
    p = place_util.near(m, x, y)
    assert p, (mid, x, y)
    o = {"x": p[0], "y": p[1], "apparence": look, "direction": d, "evenement": ev}
    if si:
        o["si"] = si
    if cache:
        o["cache_si"] = cache
    m['habitants'].append(o)
    return p


def say(t): return {"action": "dire", "texte": t}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}
def gold(n): return {"action": "or", "quantite": n}
def flag(f): return {"action": "drapeau", "nom": f}
def qstart(i): return {"action": "quete", "id": i}
def qend(i): return {"action": "quete", "id": i, "fin": True}
def ask(t, oui, non): return {"action": "question", "texte": t, "oui": oui, "non": non}
def page(acts, si=None):
    p = {}
    if si:
        p["si"] = si
    p["actions"] = acts
    return p
def done(q): return {"drapeau": "quete:%s:fin" % q}


EV, QS, ITEMS = {}, [], []

# 1. Le chien du berger (Col des Hurlevents)
npc('col', 21, 35, 'paysan', 'quete_chien', 'gauche')
npc('col', 31, 37, 'chien', 'chien_col', 'bas', si={"drapeau": "quete:chien"}, cache="chien_trouve")
npc('col', 22, 36, 'chien', 'chien_rentre', 'gauche', si={"drapeau": "chien_trouve"})
EV['quete_chien'] = {"pages": [
    page([say("Bergère : Flocon ne me quitte plus d'une semelle. Merci encore !")], done('chien')),
    page([say("Bergère : Flocon ! Tu es là ! Je croyais que la Harpie l'avait emporté…"), say("Bergère : Prenez ceci. Ma grand-mère disait qu'elle rend la vie à qui la porte."),
          give("gemme_vie"), gold(600), qend('chien')], {"drapeau": ["quete:chien", "chien_trouve"]}),
    page([say("Bergère : Flocon aime les herbes sèches, plus bas dans le col, à l'est. Il a peur du vent.")], {"drapeau": "quete:chien"}),
    page([say("Bergère : Mon chien Flocon s'est enfui quand le vent s'est levé. Il n'est jamais revenu…"),
          ask("Chercher Flocon ?", [qstart('chien'), say("Bergère : Il est tout blanc, avec une oreille noire. Il doit se cacher dans les herbes, plus bas.")],
              [say("Bergère : Il reviendra peut-être tout seul…")])])]}
EV['chien_col'] = {"actions": [say("Ouaf ! Flocon remue la queue, puis file vers le camp des bergers."), flag("chien_trouve")]}
EV['chien_rentre'] = {"actions": [say("Flocon se roule dans la neige.")]}
QS.append({"id": "chien", "nom": "Le chien de la bergère", "lieu": "Col des Hurlevents",
           "etapes": [{"texte": "Retrouver Flocon, le chien de la bergère du camp, caché dans les herbes du bas du col."},
                      {"si": {"drapeau": "chien_trouve"}, "texte": "Flocon est rentré : prévenir la bergère."}],
           "fin_texte": "Flocon est rentré au camp."})

# 2. Le collier de perles (île de Corail)
ITEMS.append({"id": "collier_corail", "nom": "Collier de corail", "icone": "ic_acc_5", "description": "Trois perles d'écume sur un fil de corail rose. La mer protège celui qui le porte.",
              "prix": 2400, "equipement": "accessoire", "bonus": {"pv": 25, "resistance": 6, "esquive": 3}})
npc('corail', 30, 15, 'villageoise', 'quete_collier', 'gauche')
EV['quete_collier'] = {"pages": [
    page([say("Plongeuse : Le collier vous va bien ! Il brille encore plus au soleil.")], done('collier')),
    page([{"action": "retirer", "objet": "perle", "quantite": 3}, say("Plongeuse : Trois perles d'écume ! Laissez-moi une minute…"),
          say("Plongeuse : Voilà. Un collier de corail, le plus beau de l'île. Il est à vous."), give("collier_corail"), qend('collier')],
         {"drapeau": "quete:collier", "objet_min": {"perle": 3}}),
    page([say("Plongeuse : Il me faut trois perles d'écume. On en trouve dans les coffres du Récif et des grottes de la mer.")], {"drapeau": "quete:collier"}),
    page([say("Plongeuse : Je fais des colliers de corail. Mais il me manque des perles d'écume, et la mer est trop agitée pour plonger."),
          ask("Rapporter trois perles d'écume ?", [qstart('collier'), say("Plongeuse : Merci ! Je vous ferai le plus beau collier de l'archipel.")],
              [say("Plongeuse : Une autre fois, alors.")])])]}
QS.append({"id": "collier", "nom": "Le collier de corail", "lieu": "Île de Corail",
           "etapes": [{"texte": "Rapporter trois perles d'écume à la plongeuse de l'île de Corail."},
                      {"si": {"objet_min": {"perle": 3}}, "texte": "Vous avez trois perles : les apporter à la plongeuse."}],
           "fin_texte": "La plongeuse vous a fait un collier de corail."})

# 3. Le dernier message du pêcheur (Val des Murmures → Port)
npc('port', 18, 19, 'villageoise', 'fille_pecheur', 'bas')
EV['fille_pecheur'] = {"pages": [
    page([say("Marine : Je vais souvent au bout du quai, maintenant. Je crois qu'il me voit. Merci.")], done('message')),
    page([say("Marine : Mon père… Il a dit ça ? Qu'il était fier de moi ?"), say("Marine : Il est parti pêcher un matin de brume et n'est jamais rentré. J'ai attendu des années."),
          say("Marine : Tenez, c'était son porte-bonheur. Il voudrait que vous l'ayez."), give("gemme_soin"), gold(1200), qend('message')],
         {"drapeau": "quete:message"}),
    page([say("Marine : Mon père était pêcheur. Un jour de brume, il n'est pas rentré. Je regarde la mer en l'attendant encore.")])]}
QS.append({"id": "message", "nom": "Le dernier message", "lieu": "Val des Murmures",
           "etapes": [{"texte": "L'esprit du pêcheur perdu, au Val des Murmures, voulait dire à sa fille Marine, au Port d'Écume, qu'il était fier d'elle."}],
           "fin_texte": "Marine a reçu le dernier message de son père."})
# l'esprit du pêcheur confie son message en partant (ajouté à sa victoire)

# 4. Le tournoi de Forgeroc (trois combats de plus en plus durs)
npc('cendrelune', 27, 36, 'chevalier', 'tournoi', 'bas', si={"drapeau": "boss2"})
def tour(k, lv, team, gain, item):
    return {"action": "combat", "nom": ["Garde Ossian", "Chevalière Mara", "Champion Torvald"][k], "ennemis": [{"espece": s, "niveau": lv + d} for s, d in team],
            "fuite": False, "capture": False, "victoire": [flag("tournoi%d" % (k + 1)), gold(gain), give(item)] +
            ([say("Héraut : Victoire ! Revenez pour la manche suivante.")] if k < 2 else [say("Héraut : Les champions de Forgeroc s'inclinent ! Voici la ceinture du tournoi."), give("bague_vif"), qend('tournoi')])}
EV['tournoi'] = {"pages": [
    page([say("Héraut : Gloire aux champions du tournoi de Forgeroc ! Votre nom est gravé sur l'enclume d'honneur.")], done('tournoi')),
    page([say("Héraut : Dernière manche ! Le Champion Torvald vous attend."),
          ask("Combattre le Champion Torvald ?", [tour(2, 36, [("fournezard", 0), ("ferrogeant", 1), ("galetor", 0)], 2500, "gemme_force")], [say("Héraut : Reposez-vous, champions.")])],
         {"drapeau": "tournoi2"}),
    page([say("Héraut : Deuxième manche : la Chevalière Mara !"),
          ask("Combattre la Chevalière Mara ?", [tour(1, 31, [("ferrolin", 0), ("draconnet", 1), ("tisonnel", 0)], 1500, "superpotion")], [say("Héraut : Comme vous voulez.")])],
         {"drapeau": "tournoi1"}),
    page([say("Héraut : Oyez ! Le tournoi de Forgeroc est ouvert à tous les dresseurs. Trois manches, trois adversaires."),
          ask("Entrer dans le tournoi ?", [qstart('tournoi'), tour(0, 27, [("galetor", 0), ("voltigeon", 0), ("tisonnel", 1)], 800, "ether")],
              [say("Héraut : Le tournoi vous attend quand vous voudrez.")])])]}
QS.append({"id": "tournoi", "nom": "Le tournoi de Forgeroc", "lieu": "Monts Cendrelune",
           "etapes": [{"texte": "Gagner les trois manches du tournoi de Forgeroc."},
                      {"si": {"drapeau": "tournoi1"}, "texte": "Première manche gagnée. Prochaine adversaire : la Chevalière Mara."},
                      {"si": {"drapeau": "tournoi2"}, "texte": "Deuxième manche gagnée. Dernier adversaire : le Champion Torvald."}],
           "fin_texte": "Vous êtes les champions de Forgeroc."})

# 5. L'étude des créatures d'Aube (montrer trois créatures d'Aube)
npc('aube', 27, 18, 'voyageuse', 'savante_aube', 'bas')
EV['savante_aube'] = {"pages": [
    page([say("Savante : Mon étude sur les créatures d'Aube sera lue dans tout l'archipel. Votre nom y est !")], done('etude_aube')),
    page([say("Savante : Une Lueurine, un Loup d'aube et un Nacrelin ! Je n'en reviens pas."), say("Savante : Prenez ceci pour vos recherches."),
          give("gemme_maitre"), gold(3000), qend('etude_aube')],
         {"drapeau": "quete:etude_aube", "membre": ["lueurine", "loup_aube", "nacrelin"]}),
    page([say("Savante : Il me faut voir de près une Lueurine, un Loup d'aube et un Nacrelin, ensemble dans votre équipe.")], {"drapeau": "quete:etude_aube"}),
    page([say("Savante : Je suis venue étudier les créatures d'Aube. Elles ressemblent aux nôtres, mais la lumière de l'île les a changées."),
          ask("Aider la savante dans son étude ?", [qstart('etude_aube'), say("Savante : Montrez-moi une Lueurine, un Loup d'aube et un Nacrelin, tous les trois dans votre équipe.")],
              [say("Savante : Tant pis, je continuerai seule.")])])]}
QS.append({"id": "etude_aube", "nom": "Les créatures d'Aube", "lieu": "Havre-d'Aube",
           "etapes": [{"texte": "Avoir ensemble une Lueurine, un Loup d'aube et un Nacrelin dans l'équipe, et les montrer à la savante de Havre-d'Aube."}],
           "fin_texte": "L'étude des créatures d'Aube est terminée."})

# ---------------------------------------------------------------------------------------------- écriture
for mid, m in maps.items():
    carte.save(mid, m)
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
# le message du pêcheur : ajouté à la victoire contre son esprit
marker = '"L\'esprit sourit enfin. « Merci… Je me souviens du chemin, maintenant. » Il s\'efface dans la lumière."'
i = et.index('"esprit_val_1"')
j = et.index(marker, i)
k = et.index('}', j) + 1
add = (', {"action": "dire", "texte": "Avant de disparaître, l\'esprit murmure : « Dites à ma fille Marine, au port, que j\'étais fier d\'elle… »"}'
       ', {"action": "quete", "id": "message"}')
et = et[:k] + add + et[k:]
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
k = [o['id'] for o in OBJ].index('amulette_maree') + 1
for i2, o in enumerate(ITEMS):
    OBJ.insert(k + i2, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
print('5 quêtes écrites')
