"""L'éleveuse de Forgeroc (étape « 20 heures », 2026-10-20) : une quête au long cours qui demande des captures. Elle
veut voir, une par une, huit créatures évoluées (il faut les capturer et les faire grandir, ou les trouver déjà
évoluées) ; chaque demande remplie rapporte plus que la précédente, la dernière un accessoire unique. Drapeaux
`eleveuse1`… `eleveuse8`, quête `eleveuse`. Une seule fois (refuse si la quête existe).

    python outils/villes/eleveuse.py
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
if any(q['id'] == 'eleveuse' for q in QT):
    sys.exit('Déjà fait.')
SP = {s['id']: s for s in json.load(open(D + 'especes.json', encoding='utf-8'))}

# (espèce évoluée, sa forme de base, récompense)
DEMANDES = [("lucifee", "lucioline", [{"action": "or", "quantite": 800}, {"action": "donner", "objet": "superpotion", "quantite": 3}]),
            ("crapoison", "grenouillon", [{"action": "or", "quantite": 1200}, {"action": "donner", "objet": "ether", "quantite": 3}]),
            ("fumerole", "brumelin", [{"action": "or", "quantite": 1600}, {"action": "donner", "objet": "gemme_vie", "quantite": 1}]),
            ("fournezard", "tisonnel", [{"action": "or", "quantite": 2200}, {"action": "donner", "objet": "elixir", "quantite": 1}]),
            ("serpentox", "serpentin", [{"action": "or", "quantite": 3000}, {"action": "donner", "objet": "gemme_force", "quantite": 1}]),
            ("noctaile", "chauvenuit", [{"action": "or", "quantite": 4000}, {"action": "donner", "objet": "elixir", "quantite": 2}]),
            ("carapic", "carapierre", [{"action": "or", "quantite": 5000}, {"action": "donner", "objet": "gemme_esprit", "quantite": 1}]),
            ("prismalin", "cristallin", [{"action": "or", "quantite": 8000}, {"action": "donner", "objet": "medaille_eleveuse", "quantite": 1}])]
for evo, base, _ in DEMANDES:
    assert SP[base]['evolution']['espece'] == evo, (base, evo)
ITEM = {"id": "medaille_eleveuse", "nom": "Médaille de l'éleveuse", "icone": "ic_acc_4", "prix": 7000, "equipement": "accessoire",
        "description": "La médaille des grands éleveurs de Forgeroc. Les créatures qui la portent grandissent plus fortes.",
        "bonus": {"pv": 40, "attaque": 5, "defense": 5, "magie": 5, "resistance": 5, "vitesse": 3}}


def say(t): return {"action": "dire", "texte": t}


def nm(i): return SP[i]['nom']


pages = [{"si": {"drapeau": "eleveuse8"}, "actions": [
    say("Éleveuse : Huit créatures évoluées, toutes en pleine santé. Vous êtes les meilleurs éleveurs que j'aie jamais vus.")]}]
for k in range(len(DEMANDES), 0, -1):  # une demande remplie (la plus avancée d'abord)
    evo, base, reward = DEMANDES[k - 1]
    prev = "eleveuse%d" % (k - 1) if k > 1 else "quete:eleveuse"
    acts = [say("Éleveuse : %s ! Regardez-moi ces couleurs… Comme il a bien grandi !" % nm(evo))] + reward + \
        [{"action": "drapeau", "nom": "eleveuse%d" % k}]
    if k < len(DEMANDES):
        nxt = DEMANDES[k][0]
        acts.append(say("Éleveuse : La prochaine fois, je voudrais voir %s, l'évolution de %s." % (nm(nxt), nm(DEMANDES[k][1]))))
    else:
        acts += [say("Éleveuse : Voilà ma médaille. Je n'ai plus rien à vous apprendre."), {"action": "quete", "id": "eleveuse", "fin": True}]
    pages.append({"si": {"drapeau": prev, "sans_drapeau": "eleveuse%d" % k, "membre": [evo]}, "actions": acts})
for k in range(len(DEMANDES), 0, -1):  # la demande en cours
    evo, base, _ = DEMANDES[k - 1]
    prev = "eleveuse%d" % (k - 1) if k > 1 else "quete:eleveuse"
    pages.append({"si": {"drapeau": prev}, "actions": [
        say("Éleveuse : Demande %d sur %d : %s, l'évolution de %s, dans votre équipe." % (k, len(DEMANDES), nm(evo), nm(base)))]})
pages.append({"actions": [
    say("Éleveuse : Je suis éleveuse. Je ne me bats pas : je regarde les créatures grandir. Et je collectionne celles qui ont évolué."),
    say("Éleveuse : Si vous m'en montrez, une par une, je vous récompenserai. De mieux en mieux."),
    {"action": "question", "texte": "Aider l'éleveuse dans sa collection ?",
     "oui": [{"action": "quete", "id": "eleveuse"},
             say("Éleveuse : Pour commencer, je voudrais voir Lucifée, l'évolution de Lucioline.")],
     "non": [say("Éleveuse : Les créatures ne grandissent pas toutes seules, vous savez.")]}]})
EV = {"eleveuse": {"pages": pages}}
Q = {"id": "eleveuse", "nom": "L'éleveuse de Forgeroc", "lieu": "Monts Cendrelune",
     "etapes": [{"texte": "Montrer à l'éleveuse de Forgeroc, une par une, les huit créatures évoluées qu'elle demande."}] +
     [{"si": {"drapeau": "eleveuse%d" % k}, "texte": "%d demande%s remplie%s sur 8." % (k, 's' if k > 1 else '', 's' if k > 1 else '')} for k in (2, 4, 6)],
     "fin_texte": "L'éleveuse vous a donné sa médaille."}

m = carte.load('cendrelune')
before = pu.reachable(m)
p = pu.near_reach(m, 22, 40, ring=1)
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "paysan", "direction": "bas", "evenement": "eleveuse", "si": {"drapeau": "boss1"}})
lost = before - pu.reachable(m) - {tuple(p)}
if lost:
    sys.exit('cendrelune : des cases deviennent inaccessibles %s' % sorted(lost)[:5])
carte.save('cendrelune', m)
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
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('cloche_echo') + 1
OBJ.insert(k, ITEM)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
print("L'éleveuse est à Forgeroc en", p)
