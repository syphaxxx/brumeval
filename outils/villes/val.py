"""Le Val des Murmures (étape « 20 heures », 2026-10-20) : entre le sanctuaire du Givrecorne et la Source des Brumes.
Une forêt noyée de brume où errent trois esprits ; il faut les apaiser (un combat chacun) pour que la gardienne des
lanternes ouvre le chemin de la Source. Une seule fois (refuse si data/cartes/val.json existe).

    python outils/villes/val.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
from cartegen import Map  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/val.json'):
    sys.exit('Le Val des Murmures existe déjà.')

W, H = 48, 40
V = Map('val', "Val des Murmures", 'foret', W, H, 'T', 80)
V.m['ambiance'] = {"effet": "brume"}
V.path([(23, 39), (23, 33), (12, 33), (12, 24), (34, 24), (34, 14), (23, 14), (23, 0)], '=', 2)
for cx, cy, r in [(23, 33, 4), (8, 30, 4), (12, 22, 4), (36, 30, 4), (40, 20, 4), (34, 12, 4), (10, 10, 5), (23, 6, 3)]:
    V.blob(cx, cy, r, ',', only='T')
for cx, cy, r in [(23, 35, 3), (12, 24, 2), (34, 16, 2)]:
    V.blob(cx, cy, r, '.', only='T,')
V.blob(30, 32, 3, '~', only='T,')
V.blob(16, 8, 2, '~', only='T,')
V.path([(12, 30), (8, 30)], '.', 1), V.path([(34, 24), (40, 24), (40, 20)], '.', 1), V.path([(23, 10), (10, 10)], '.', 1)
V.sprinkle(30, 'd', ',', 2, 2, 45, 37, away=1)
V.sprinkle(20, 'F', ',.', 2, 2, 45, 37)
# le hameau de Brumaille, à l'entrée
V.rect(19, 35, 28, 37, '.')
V.put(20, 36, 'X'), V.put(27, 35, 'L'), V.put(19, 35, 'L')
V.npc(24, 35, 'ancien', 'bas', 'gardienne_lanternes')
V.npc(21, 37, 'villageoise', 'droite', lines=["Brumaille était un village joyeux, avant que la brume ne prenne nos morts.", "Leurs esprits errent dans le val. Ils ne savent plus rentrer."])
# les trois esprits, au bout de chaque clairière
for k, (x, y) in enumerate([(7, 30), (41, 19), (9, 9)], start=1):
    V.npc(x, y, 'ermite', 'bas', 'esprit_val_%d' % k, cache='esprit_val_%d' % k)
V.chest(36, 31, 'superpotion', 3), V.chest(12, 22, 'gemme_esprit'), V.chest(33, 11, 'or', 2500), V.chest(24, 6, 'elixir')
V.zone(0, 0, W, H, 34, 38, 3, ['esprillon', 'nebulis', 'brumeloup', 'chauvenuit', 'orbrume'])
V.warp(23, 39, 'sanctuaire', 9, 2, 'bas'), V.warp(24, 39, 'sanctuaire', 9, 2, 'bas')
V.warp(23, 0, 'source', 19, 28, 'haut', {"drapeau": ["esprit_val_1", "esprit_val_2", "esprit_val_3"]},
       "Une brume épaisse comme un mur ferme le chemin. Elle chuchote des noms…")
V.warp(24, 0, 'source', 20, 28, 'haut', {"drapeau": ["esprit_val_1", "esprit_val_2", "esprit_val_3"]},
       "Une brume épaisse comme un mur ferme le chemin. Elle chuchote des noms…")
V.m['declencheurs'] = [{"x": 23, "y": 38, "l": 2, "h": 1, "evenement": "val_entree", "jusqua": "val_vu"}]
V.m['carte_monde'] = [66, 18]
r = V.reach((23, 38))
for p in [(8, 30), (41, 20), (9, 10), (35, 31), (13, 22), (32, 11), (23, 7), (23, 1)]:
    if p not in r and not any((p[0] + dx, p[1] + dy) in r for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
        V.show()
        sys.exit('val : case inaccessible %s' % (p,))
for h in V.m['habitants']:
    if V.get(h['x'], h['y']) not in '.,=F':
        sys.exit('val : habitant sur %s en %s' % (V.get(h['x'], h['y']), (h['x'], h['y'])))
V.save()

S = carte.load('sanctuaire')
for w in S['passages']:
    if w['vers'] == 'source':
        w['vers'], w['tx'], w['ty'] = 'val', 23, 38
carte.save('sanctuaire', S)
Src = carte.load('source')
for w in Src['passages']:
    if w['vers'] == 'sanctuaire':
        w['vers'], w['tx'], w['ty'] = 'val', w['x'] + 4, 1
carte.save('source', Src)


def say(t): return {"action": "dire", "texte": t}


ESPRITS = [("Le pêcheur perdu", "Un vieil homme translucide fixe l'eau. « Où est ma barque ? Où est mon village ? »", "nebulis"),
           ("La fillette perdue", "Une petite silhouette pâle joue toute seule. « Tu veux jouer avec moi ? Pour toujours ? »", "esprillon"),
           ("Le soldat perdu", "Un soldat en armure rouillée monte la garde. « Halte ! Personne ne passe tant que la brume règne ! »", "brumeloup")]
EV = {
    "val_entree": {"actions": [{"action": "drapeau", "nom": "val_vu"},
                               say("La brume est si épaisse qu'on n'y voit pas à trois pas. On entend des voix, partout et nulle part."),
                               say("Un hameau se devine à l'entrée du val. Des lanternes y brûlent encore.")]},
    "gardienne_lanternes": {"pages": [
        {"si": {"drapeau": ["esprit_val_1", "esprit_val_2", "esprit_val_3"]}, "actions": [
            say("Gardienne : Les trois esprits sont rentrés. Écoutez… La brume s'ouvre vers le nord."),
            say("Gardienne : La Source des Brumes est au bout du chemin. Que nos lanternes vous guident."),
            {"action": "quete", "id": "esprits", "fin": True}]},
        {"si": {"drapeau": "quete:esprits"}, "actions": [say("Gardienne : Trois esprits errent encore : un pêcheur près des eaux, une fillette au fond du val, un soldat sur la colline. Apaisez-les.")]},
        {"actions": [
            say("Gardienne : Vous venez du sanctuaire ? Alors vous avez vaincu le Givrecorne…"),
            say("Gardienne : Je garde les lanternes de Brumaille. La brume a pris nos morts : leurs esprits errent dans le val et la rendent épaisse."),
            say("Gardienne : Tant qu'ils errent, personne n'atteindra la Source. Apaisez-les, et je rallumerai le chemin."),
            {"action": "quete", "id": "esprits"}]}]},
}
for k, (nom, phrase, sp) in enumerate(ESPRITS, start=1):
    EV["esprit_val_%d" % k] = {"actions": [
        say(phrase),
        say("La brume se resserre autour de l'esprit : des créatures surgissent pour le protéger !"),
        {"action": "combat", "nom": nom, "ennemis": [{"espece": sp, "niveau": 37}, {"espece": "esprillon", "niveau": 38, "pv": 2.5, "boss": True},
                                                     {"espece": sp, "niveau": 37}],
         "fuite": True, "capture": False,
         "victoire": [{"action": "drapeau", "nom": "esprit_val_%d" % k},
                      say("L'esprit sourit enfin. « Merci… Je me souviens du chemin, maintenant. » Il s'efface dans la lumière.")]}]}
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
Q = {"id": "esprits", "nom": "Les esprits du val", "lieu": "Val des Murmures",
     "etapes": [{"texte": "Apaiser les trois esprits perdus du Val des Murmures : un pêcheur près des eaux, une fillette au fond du val, un soldat sur la colline."},
                {"si": {"drapeau": ["esprit_val_1", "esprit_val_2", "esprit_val_3"]}, "texte": "Les trois esprits sont apaisés : retourner voir la gardienne des lanternes."}],
     "fin_texte": "Les esprits de Brumaille sont rentrés."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
print('Val des Murmures écrit')
