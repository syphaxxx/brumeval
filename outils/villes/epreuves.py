"""Les épreuves des gardiens (étape « 20 heures », 2026-10-20) : après l'acte 3, le sanctuaire d'Aube ouvre neuf
combats de plus en plus durs (une « ligue »), contre les esprits des anciens gardiens puis Ilya.

    python outils/villes/epreuves.py     # une seule fois (refuse si l'événement « epreuves » existe déjà)
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
EVS = json.load(open(D + 'evenements.json', encoding='utf-8'))
if 'epreuves' in EVS:
    sys.exit('Les épreuves existent déjà.')


def say(t): return {"action": "dire", "texte": t}
def flag(f): return {"action": "drapeau", "nom": f}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}
def foe(sp, lv, lead=False):
    e = {"espece": sp, "niveau": lv}
    if lead:
        e.update({"pv": 1.8, "boss": True, "bouclier": 6})
    return e


# (titre, nom du gardien, phrase d'accueil, équipe, renforts, récompense)
EPREUVES = [
    ("Épreuve de la Flamme", "Gardienne Ardente", "Le feu ne pardonne pas l'hésitation. Montrez-moi votre courage !",
     [("fournezard", 0), ("flammenard", 1), ("lucifee", 0)], ["draconnet", "fournezard"], "gemme_force"),
    ("Épreuve des Flots", "Gardien des Marées", "La mer use la pierre la plus dure. Saurez-vous tenir ?",
     [("torrentin", 0), ("kraken", 1), ("fulgoray", 0)], ["nacrelin", "bastillon"], "gemme_esprit"),
    ("Épreuve des Racines", "Gardienne Sylve", "Les racines patientent mille ans. Moi aussi.",
     [("ronceflore", 0), ("ronce_mere", 1), ("vasepent", 0)], ["champignard", "crapale"], "gemme_vie"),
    ("Épreuve du Givre", "Gardien Boréal", "Le froid ralentit tout, sauf la volonté. Voyons la vôtre.",
     [("givrenard", 0), ("banquisot", 1), ("givrelin", 0)], ["prismalin", "glaconnet"], "gemme_eveil"),
    ("Épreuve de la Foudre", "Gardienne Fulgurante", "Un éclair ne prévient jamais. Soyez plus rapides que moi !",
     [("fulgurin", 0), ("griffon", 1), ("orageon", 0)], ["orageline", "voltanguille"], "gemme_rempart"),
    ("Épreuve de la Pierre", "Gardien Rempart", "Personne n'a jamais fait reculer mes murailles.",
     [("ferrogeant", 0), ("rocaube", 1), ("bastillon", 0)], ["orbrume", "carapic"], "gemme_contre"),
    ("Épreuve des Ombres", "Gardienne Nocturne", "Dans le noir, on ne voit que ce qu'on craint. Que craignez-vous ?",
     [("brumeloup", 0), ("nebulis", 1), ("noctaile", 0)], ["aubespectre", "osselet"], "gemme_vol"),
    ("Épreuve du Vent", "Gardien Céleste", "Le vent ne se bat pas : il emporte. Accrochez-vous !",
     [("aubelle", 0), ("zephyrin", 1), ("lueurine", 0)], ["griffon", "nuageon"], "gemme_briseur"),
]

pages = []
# Après la dernière épreuve
pages.append({"si": {"drapeau": "epreuve9"}, "actions": [
    say("Ilya : Vous êtes les nouveaux gardiens d'Aube. Les esprits du sanctuaire dorment en paix."),
    say("Ilya : Revenez me voir de temps en temps. Les créatures de l'île vous aiment bien.")]})
# La finale : Ilya
ilya_team = [foe("loup_aube", 84), foe("nacrelin", 86, True), foe("aubespectre", 84)]
pages.append({"si": {"drapeau": "epreuve8"}, "actions": [
    say("Les huit esprits se rassemblent autour d'Ilya. Ils s'inclinent devant vous."),
    say("Ilya : Il reste une épreuve. La mienne. Mon peuple disait qu'un gardien doit vaincre le dernier gardien pour le remplacer."),
    say("Ilya : Je ne vous ferai pas de cadeau !"),
    {"action": "question", "texte": "Affronter Ilya, dernière gardienne d'Aube ?", "oui": [
        {"action": "combat", "nom": "Ilya", "musique": "boss_final", "ennemis": ilya_team,
         "renforts": [foe("rocaube", 84), foe("lueurine", 85)], "boss": True, "fuite": False, "capture": False,
         "victoire": [flag("epreuve9"), give("insigne_gardien"), {"action": "or", "quantite": 12000},
                      say("Ilya : … Bravo. Les esprits vous ont choisis, et moi aussi."),
                      say("Ilya : Prenez cet insigne. Il n'a été porté que par les plus grands gardiens d'Aube."),
                      {"action": "quete", "id": "epreuves", "fin": True}]}],
     "non": [say("Ilya : Prenez votre temps. Je serai là.")]}]})
for k in range(len(EPREUVES), 0, -1):
    titre, nom, accueil, team, renf, gemme = EPREUVES[k - 1]
    lv = 68 + 2 * k
    cond = {"drapeau": "epreuve%d" % (k - 1) if k > 1 else "aube_fin", "sans_drapeau": "epreuve%d" % k}
    nomme = titre[0].lower() + titre[1:]  # « épreuve de la Flamme »
    acts = [say("Un esprit de lumière prend forme : %s." % nom), say("%s : %s" % (nom, accueil)),
            {"action": "question", "texte": "Commencer l'%s ?" % nomme,
             "oui": ([{"action": "quete", "id": "epreuves"}] if k == 1 else []) + [
                 {"action": "combat", "nom": nom, "ennemis": [foe(sp, lv + 1 if lead else lv, bool(lead)) for sp, lead in team],
                  "renforts": [foe(sp, lv) for sp in renf], "fuite": False, "capture": False,
                  "victoire": [flag("epreuve%d" % k), {"action": "or", "quantite": 2500 + 500 * k}, give(gemme),
                               say("%s : Vous avez réussi l'%s. Que la lumière vous accompagne." % (nom, nomme))]}],
             "non": [say("L'esprit s'efface doucement. Il reviendra quand vous serez prêts.")]}]
    pages.append({"si": cond, "actions": acts})
pages.append({"actions": [say("Les portes du sanctuaire sont scellées par une lumière douce. Quelque chose attend, à l'intérieur."),
                          say("(Revenez quand la Brume Noire ne menacera plus l'île.)")]})
EVS_NEW = {"epreuves": {"pages": pages}}

# La quête du journal
QUEST = {"id": "epreuves", "nom": "Les épreuves des gardiens", "lieu": "Havre-d'Aube",
         "etapes": [{"texte": "Réussir les huit épreuves du sanctuaire d'Aube, puis vaincre Ilya, la dernière gardienne."}] +
                   [{"si": {"drapeau": "epreuve%d" % k}, "texte": "Épreuves réussies : %d sur 8. Retourner au sanctuaire d'Aube." % k} for k in range(1, 8)] +
                   [{"si": {"drapeau": "epreuve8"}, "texte": "Les huit épreuves sont réussies : affronter Ilya, au sanctuaire d'Aube."}],
         "fin_texte": "Vous êtes les nouveaux gardiens d'Aube."}

# L'insigne
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('lame_aube') + 1
OBJ.insert(k, {"id": "insigne_gardien", "nom": "Insigne des gardiens", "icone": "ic_acc_2",
               "description": "Porté par les plus grands gardiens d'Aube. Il réchauffe le cœur au combat.", "prix": 6000,
               "equipement": "accessoire", "bonus": {"pv": 40, "attaque": 4, "defense": 4, "magie": 4, "resistance": 4, "vitesse": 4}})
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')

# Le sanctuaire d'Aube devient une porte
A = carte.load('aube')
for b in A['batiments']:
    if b['nom'] == 'Sanctuaire des gardiens':
        b['genre'] = 'maison'
        b['evenement'] = 'epreuves'
        dx, dy = b['x'] + b['l'] // 2, b['y'] + b['h']
rows = [list(r) for r in A['tuiles']]
for y in range(dy, 10):  # un chemin de la porte jusqu'à la place
    if rows[y][dx] in 'TFURL.':
        rows[y][dx] = '='
A['tuiles'] = [''.join(r) for r in rows]
carte.save('aube', A)

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EVS_NEW.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(QUEST, ensure_ascii=False) + nl + ']' + nl)
print('épreuves écrites ; porte du sanctuaire en', dx, dy - 1)
