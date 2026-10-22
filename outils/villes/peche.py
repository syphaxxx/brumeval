"""La pêche (étape « 20 heures », 2026-10-20) : poissons (objets + data/peche.json), canne à pêche, maître pêcheur du
port et carnet de pêche (quête). Une seule fois (refuse si data/peche.json existe).

    python outils/villes/peche.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
if os.path.exists(D + 'peche.json'):
    sys.exit('La pêche existe déjà (data/peche.json).')

# objet, nom, eaux, poids, difficulté, prix, description
POISSONS = [
    ("gardon", "Gardon", ["vallee"], 30, 1, 20, "Un petit poisson argenté des lacs de la vallée."),
    ("truite", "Truite arc-en-ciel", ["vallee", "foret"], 18, 2, 60, "Ses écailles brillent de toutes les couleurs."),
    ("brochet_dore", "Brochet doré", ["vallee"], 4, 4, 400, "Rare : un brochet couleur de soleil. Les pêcheurs en rêvent."),
    ("perche", "Perche moussue", ["foret"], 25, 2, 45, "Elle se cache sous les racines des vieux arbres."),
    ("silure", "Silure des marais", ["foret", "marais"], 10, 3, 150, "Énorme et paresseux, mais il tire fort."),
    ("omble", "Omble des glaces", ["neige"], 22, 2, 90, "Il vit sous la glace et n'a jamais froid."),
    ("lotte", "Lotte givrée", ["neige"], 8, 4, 260, "Une lumière bleue pend devant sa bouche."),
    ("aveugle", "Poisson aveugle", ["grotte", "source"], 20, 2, 110, "Il n'a pas d'yeux, mais il sent tout."),
    ("sardine", "Sardine", ["port", "recif", "aube"], 30, 1, 15, "Elle nage en bancs serrés le long des quais."),
    ("maquereau", "Maquereau", ["port", "recif"], 20, 2, 50, "Rayé de bleu, rapide comme une flèche."),
    ("bar", "Bar d'écume", ["port", "recif"], 12, 3, 140, "Le poisson préféré du capitaine Morvan."),
    ("raie", "Raie lune", ["recif", "ruines"], 7, 4, 300, "Elle glisse sur le sable comme un oiseau dans le ciel."),
    ("poulpe", "Poulpe des ruines", ["recif"], 9, 3, 220, "Il vient des ruines englouties et vole les pièces des noyés."),
    ("espadon", "Espadon d'orage", ["recif", "tour"], 3, 5, 900, "Très rare : on dit qu'il naît des éclairs qui tombent dans la mer."),
    ("poisson_lune", "Poisson-lune", ["aube"], 18, 2, 120, "Rond et rose, il dort à la surface les nuits d'Aube."),
    ("carpe_aube", "Carpe d'aube", ["aube", "lueurs"], 10, 4, 500, "Ses écailles ont la couleur du soleil levant."),
    ("nacre", "Poisson nacré", ["marais", "lueurs"], 6, 4, 420, "Il brille comme une perle dans l'eau pâle."),
    ("roi_brumes", "Roi des brumes", ["source"], 2, 5, 1500, "Le poisson légendaire de la Source des Brumes. Presque personne ne l'a vu."),
]

doc = {"poissons": [{"objet": o, "eaux": e, "poids": p, "difficulte": d} for o, _, e, p, d, _, _ in POISSONS]}
open(D + 'peche.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(doc) + '\n')

OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('lanterne_or') + 1
new = [{"id": "canne", "nom": "Canne à pêche", "icone": "ic_lance_1",
        "description": "Face à l'eau, Entrée pour lancer la ligne. Quand ça mord, Entrée vite, puis Entrée dans la zone verte pour ferrer.",
        "prix": 0}]
for o, nom, _, _, d, prix, desc in POISSONS:
    new.append({"id": o, "nom": nom, "description": desc + " Difficulté : " + "★" * d + ".", "prix": prix * 2})
for i, o in enumerate(new):
    OBJ.insert(k + i, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')


def say(t): return {"action": "dire", "texte": t}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}
def flag(f): return {"action": "drapeau", "nom": f}


N = len(POISSONS)
paliers = [(5, "carnet5", [give("lanterne_or", 3), {"action": "or", "quantite": 1000}], "Cinq espèces ! Tu as la main, mon gars."),
           (10, "carnet10", [give("gemme_eveil")], "Dix espèces ! Celle-là, je l'ai trouvée dans le ventre d'un bar."),
           (15, "carnet15", [give("gemme_maitre")], "Quinze ! Même moi, je n'en ai jamais pris autant."),
           (N, "carnet_complet", [give("amulette_maree"), {"action": "or", "quantite": 10000}],
            "Toutes les espèces… Même le Roi des brumes ! Tu es le plus grand pêcheur de l'archipel.")]
pages = []
for n, f, rew, txt in reversed(paliers):
    acts = [say("Maître pêcheur : " + txt)] + rew + [flag(f)]
    if n == N:
        acts.append({"action": "quete", "id": "peche", "fin": True})
    pages.append({"si": {"drapeau": "quete:peche", "sans_drapeau": f, "poissons_min": n}, "actions": acts})
pages.append({"si": {"drapeau": "quete:peche"}, "actions": [
    say("Maître pêcheur : Chaque eau a ses poissons : les lacs de la vallée, la forêt, la neige, les grottes, la mer, et même l'île d'Aube."),
    say("Maître pêcheur : Reviens me voir à 5, 10 et 15 espèces, et quand le carnet sera complet. Les plus rares sont les plus durs à ferrer !")]})
pages.append({"actions": [
    say("Maître pêcheur : Hé, toi ! Tu as l'air d'avoir de la patience. Ça te dirait d'apprendre à pêcher ?"),
    give("canne"),
    say("Maître pêcheur : Face à l'eau, lance ta ligne. Quand ça mord : vite ! Puis appuie quand le curseur passe dans la zone verte."),
    say("Maître pêcheur : Tiens aussi ce carnet. Montre-moi ce que tu attrapes : j'ai des cadeaux pour les bons pêcheurs."),
    {"action": "quete", "id": "peche"}]})
EV = {"maitre_pecheur": {"pages": pages}}
QUEST = {"id": "peche", "nom": "Le carnet de pêche", "lieu": "Port d'Écume",
         "etapes": [{"texte": "Pêcher toutes les espèces de poissons (%d) et les montrer au maître pêcheur du port, à 5, 10 et 15 espèces." % N}],
         "fin_texte": "Le carnet de pêche est complet."}

P = carte.load('port')
P['habitants'].append({"x": 6, "y": 21, "apparence": "pecheur", "direction": "droite", "evenement": "maitre_pecheur"})
carte.save('port', P)

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(QUEST, ensure_ascii=False) + nl + ']' + nl)
print('pêche :', N, 'poissons')
