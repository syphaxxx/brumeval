"""Le grand bestiaire (étape « 20 heures », 2026-10-20) : une fois le carnet de la naturaliste rempli, sa professeure
récompense les espèces obtenues (condition « especes_min » : 15, 30, 45, 60). Une seule fois.

    python outils/villes/bestiaire.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util  # noqa: E402

D = carte.R
EV = json.load(open(D + 'evenements.json', encoding='utf-8'))
if 'professeure' in EV:
    sys.exit('Le grand bestiaire existe déjà.')


def say(t): return {"action": "dire", "texte": t}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}
def flag(f): return {"action": "drapeau", "nom": f}


paliers = [(15, [give("lanterne_or", 5), {"action": "or", "quantite": 2000}], "Quinze espèces ! Pour un début, c'est remarquable."),
           (30, [give("gemme_maitre")], "Trente espèces ! Vous en savez plus que la moitié de mes collègues."),
           (45, [give("bottes_ailees")], "Quarante-cinq ! Vous avez dû parcourir tout l'archipel."),
           (60, [give("medaille_naturaliste"), {"action": "or", "quantite": 10000}],
            "Soixante espèces… Le plus grand bestiaire jamais réuni. Il portera votre nom, dans les livres de l'archipel.")]
pages = []
for n, rew, txt in reversed(paliers):
    acts = [say("Professeure : " + txt)] + rew + [flag("bestiaire%d" % n)]
    if n == 60:
        acts.append({"action": "quete", "id": "bestiaire", "fin": True})
    pages.append({"si": {"drapeau": "quete:bestiaire", "sans_drapeau": "bestiaire%d" % n, "especes_min": n}, "actions": acts})
pages.append({"si": {"drapeau": "quete:bestiaire"}, "actions": [
    say("Professeure : Chaque espèce obtenue (capturée, reçue ou évoluée) compte. Le Bestiaire du menu tient le compte."),
    say("Professeure : Revenez à 15, 30, 45 et 60 espèces !")]})
pages.append({"actions": [
    say("Professeure : C'est vous qui avez aidé mon élève à remplir son carnet ? Ses notes sont excellentes."),
    say("Professeure : J'écris le grand bestiaire de l'archipel. Il me faudrait quelqu'un sur le terrain…"),
    {"action": "quete", "id": "bestiaire"},
    say("Professeure : Rapportez-moi des espèces, autant que possible. Je récompense chaque étape.")]})
NEW = {"professeure": {"pages": pages}}

OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('insigne_gardien') + 1
OBJ.insert(k, {"id": "medaille_naturaliste", "nom": "Médaille du naturaliste", "icone": "ic_acc_4",
               "description": "Remise à ceux qui connaissent toutes les créatures. Elles semblent vous faire confiance.", "prix": 5000,
               "equipement": "accessoire", "bonus": {"pv": 30, "attaque": 3, "defense": 3, "magie": 3, "resistance": 3, "vitesse": 3, "precision": 5}})
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')

V = carte.load('vallee')
nat = [h for h in V['habitants'] if h.get('evenement') == 'quete_naturaliste'][0]
x, y = place_util.near(V, nat['x'] + 2, nat['y'])
V['habitants'].append({"x": x, "y": y, "apparence": "ancien", "direction": "bas", "evenement": "professeure", "si": {"drapeau": "nat3"}})
carte.save('vallee', V)

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in NEW.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
Q = {"id": "bestiaire", "nom": "Le grand bestiaire", "lieu": "Vallée de Brumeval",
     "etapes": [{"texte": "Obtenir des espèces de créatures (Bestiaire) et les montrer à la professeure de la Vallée : 15, 30, 45 et 60."}],
     "fin_texte": "Le grand bestiaire de l'archipel est écrit."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
print('professeure en', x, y)
