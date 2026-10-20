"""Les couleurs rares (étape « 20 heures », 2026-10-20) : l'assistant de la professeure, dans la Vallée, étudie les
créatures chromatiques. Il récompense une, puis trois créatures chromatiques dans l'équipe ou la réserve (condition
`chromatiques_min`). Drapeaux `chroma1`, `chroma3`, quête `chroma`. Une seule fois (refuse si la quête existe).

    python outils/villes/chromatiques.py
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
if any(q['id'] == 'chroma' for q in QT):
    sys.exit('Déjà fait.')


def say(t): return {"action": "dire", "texte": t}


ITEM = {"id": "prisme_chromatique", "nom": "Prisme chromatique", "icone": "ic_gemme_cristal", "prix": 9000, "equipement": "accessoire",
        "description": "Un prisme qui décompose la lumière en couleurs que personne n'a jamais vues. Il porte bonheur aux chasseurs de raretés.",
        "bonus": {"pv": 50, "attaque": 6, "defense": 6, "magie": 6, "resistance": 6, "vitesse": 4, "critique": 4}}
EV = {"assistant_chroma": {"pages": [
    {"si": {"drapeau": "chroma3"}, "actions": [say("Assistant : Trois créatures chromatiques ! Ma thèse est terminée grâce à vous.")]},
    {"si": {"drapeau": ["quete:chroma", "chroma1"], "sans_drapeau": "chroma3", "chromatiques_min": 3}, "actions": [
        say("Assistant : Trois ! Trois créatures aux couleurs rares, réunies ! La professeure ne va pas me croire."),
        say("Assistant : Prenez ce prisme. Il décompose la lumière en couleurs que personne n'a jamais vues."),
        {"action": "donner", "objet": "prisme_chromatique", "quantite": 1}, {"action": "or", "quantite": 10000},
        {"action": "drapeau", "nom": "chroma3"}, {"action": "quete", "id": "chroma", "fin": True}]},
    {"si": {"drapeau": "quete:chroma", "sans_drapeau": "chroma1", "chromatiques_min": 1}, "actions": [
        say("Assistant : Une créature chromatique ! Regardez ces couleurs… On en voit une sur cent cinquante, à peine."),
        say("Assistant : Voici pour vous remercier. Si vous en trouvez trois, revenez me voir !"),
        {"action": "donner", "objet": "gemme_maitre", "quantite": 1}, {"action": "or", "quantite": 3000}, {"action": "drapeau", "nom": "chroma1"}]},
    {"si": {"drapeau": "chroma1"}, "actions": [say("Assistant : Il m'en faudrait trois, ensemble, dans votre équipe ou votre réserve.")]},
    {"si": {"drapeau": "quete:chroma"}, "actions": [
        say("Assistant : Une créature chromatique brille de couleurs rares dès le début du combat. Capturez-la !")]},
    {"actions": [
        say("Assistant : J'étudie les créatures chromatiques : des créatures aux couleurs rares, qui brillent un peu."),
        say("Assistant : On en croise une sur cent cinquante, environ. Si vous en capturez une, montrez-la-moi !"),
        {"action": "quete", "id": "chroma"}]}]}}
Q = {"id": "chroma", "nom": "Les couleurs rares", "lieu": "Vallée de Brumeval",
     "etapes": [{"texte": "Capturer une créature chromatique (aux couleurs rares) et la montrer à l'assistant de la professeure."},
                {"si": {"drapeau": "chroma1"}, "texte": "Réunir trois créatures chromatiques (équipe ou réserve) et revenir voir l'assistant."}],
     "fin_texte": "L'assistant a fini sa thèse sur les créatures chromatiques."}

m = carte.load('vallee')
before = pu.reachable(m)
p = pu.near_reach(m, 35, 13, ring=1)
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "voyageuse", "direction": "bas", "evenement": "assistant_chroma"})
lost = before - pu.reachable(m) - {tuple(p)}
if lost:
    sys.exit('vallee : des cases deviennent inaccessibles %s' % sorted(lost)[:5])
carte.save('vallee', m)
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
k = [o['id'] for o in OBJ].index('lanterne_veilleur') + 1
OBJ.insert(k, ITEM)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
print('Assistant en', p)
