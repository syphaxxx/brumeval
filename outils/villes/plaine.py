"""La Plaine des Moulins (étape « 20 heures », 2026-10-20) : une région de l'acte 1 entre la Vallée (après Sylvarque)
et les Monts Cendrelune. Le village de Moulinbourg, sa rivière, et un pont rongé par une bande de Rongecrins : il faut
les chasser pour que le charpentier le répare. Une seule fois (refuse si data/cartes/plaine.json existe).

    python outils/villes/plaine.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
from cartegen import Map  # noqa: E402

D = carte.R
if os.path.exists(D + 'cartes/plaine.json'):
    sys.exit('La plaine existe déjà.')

W, H = 56, 36
P = Map('plaine', "Plaine des Moulins", 'vallee', W, H, '.', 90)
P.rect(0, 0, W - 1, 1, 'T'), P.rect(0, H - 2, W - 1, H - 1, 'T')
P.rect(0, 0, 0, H - 1, 'T'), P.rect(W - 1, 0, W - 1, H - 1, 'T')
P.rect(36, 2, 38, H - 3, '~')                                  # la rivière
P.path([(0, 16), (12, 16), (12, 20), (30, 20), (30, 16), (35, 16)], '=', 2)
P.path([(39, 16), (55, 16)], '=', 2)
P.rect(36, 16, 38, 17, 'B')                                    # le pont (rongé : passage fermé à l'est)
for cx, cy, r in [(8, 6, 5), (24, 6, 5), (46, 7, 5), (8, 28, 5), (26, 29, 5), (47, 27, 5)]:
    P.blob(cx, cy, r, ',', only='.')
P.sprinkle(30, 'T', '.', 2, 2, 34, 33, away=1)
P.sprinkle(18, 'T', '.', 40, 2, 53, 33, away=1)
P.sprinkle(30, 'F', '.,', 2, 2, 53, 33)
# Moulinbourg
P.rect(14, 10, 28, 14, 'q')
for b in [dict(x=14, y=5, l=5, h=4, genre='soin', nom="Maison de soin", toit='#b8483e', evenement='soin_plaine'),
          dict(x=22, y=5, l=5, h=4, genre='boutique1', nom="Épicerie du moulin", toit='#3f6fb0', evenement='boutique_plaine'),
          dict(x=16, y=23, l=5, h=4, genre='maison', nom="Atelier du charpentier", toit='#9a5a2a', evenement='atelier_plaine'),
          dict(x=29, y=4, l=5, h=5, genre='decor', nom="Moulin", toit='#c8a060', dessin='moulin')]:
    P.m['batiments'].append(b)
    P.rect(b['x'], b['y'], b['x'] + b['l'] - 1, b['y'] + b['h'] - 1, '.')
    dx = b['x'] + b['l'] // 2
    if b['genre'] != 'decor':
        if b['y'] < 12:
            P.path([(dx, b['y'] + b['h']), (dx, 10)], '=', 1, keep='q')
        else:
            P.path([(dx, b['y'] + b['h']), (dx, b['y'] + b['h'] + 1), (14, b['y'] + b['h'] + 1), (14, 21)], '=', 1, keep='q=')
P.path([(21, 14), (21, 20)], '=', 1, keep='q=')
P.put(21, 12, 'Y'), P.put(14, 10, 'L'), P.put(28, 10, 'L'), P.put(26, 13, 'G')
P.put(17, 13, 'M'), P.put(19, 13, 'M')
P.rect(11, 25, 13, 27, 'H')
P.npc(22, 12, 'paysan', 'bas', 'meunier')
P.npc(32, 18, 'forgeron', 'gauche', 'charpentier')
P.npc(18, 12, 'marchande', 'bas', lines=["Des légumes frais de la plaine ! Depuis que la brume est partie, tout pousse deux fois plus vite.",
                                       "Les Monts Cendrelune ? Il faut passer le pont, à l'est. Enfin… quand il tiendra debout."])
P.npc(25, 13, 'enfant', 'bas', roam=2, lines=["Les Rongecrins ont mangé le pont ! Ils mangent tout, même les cailloux !"])
P.npc(6, 16, 'villageois', 'droite', lines=["Bienvenue dans la Plaine des Moulins !", "Le col de Sylvarque est enfin ouvert. On va pouvoir revoir les gens de la Vallée."])
P.boss(34, 22, 'rongecrin', 'rongecrins_chasses', 'rongecrins', si={"drapeau": "quete:pont"})
P.chest(46, 6, 'lanterne_argent', 2), P.chest(8, 30, 'potion', 3), P.chest(48, 28, 'ether', 2)
P.chest(26, 31, 'gemme_vie')
P.zone(0, 0, 36, H, 8, 12, 3, ['mulotin', 'piafouine', 'champichou', 'rongecrin', 'louvet'])
P.zone(39, 0, 17, H, 10, 13, 3, ['rongecrin', 'louvet', 'rocaillou', 'lucioline'])
P.warp(0, 16, 'vallee', 62, 32, 'gauche'), P.warp(0, 17, 'vallee', 62, 33, 'gauche')
P.warp(55, 16, 'cendrelune', 1, 33, 'droite'), P.warp(55, 17, 'cendrelune', 1, 34, 'droite')
# le pont : passage fermé tant qu'il n'est pas réparé (un passage vers sa propre case = une barrière)
P.m['passages'] += [{"x": 37, "y": 16, "vers": "plaine", "tx": 37, "ty": 16, "direction": "droite", "condition": {"drapeau": "pont_repare"},
                     "message": "Le pont est rongé de partout : il s'effondrerait sous vos pieds. Le charpentier de Moulinbourg pourrait le réparer."},
                    {"x": 37, "y": 17, "vers": "plaine", "tx": 37, "ty": 17, "direction": "droite", "condition": {"drapeau": "pont_repare"},
                     "message": "Le pont est rongé de partout : il s'effondrerait sous vos pieds. Le charpentier de Moulinbourg pourrait le réparer."}]
P.m['declencheurs'] = [{"x": 1, "y": 16, "l": 1, "h": 2, "evenement": "plaine_entree", "jusqua": "plaine_vue"}]
P.m['carte_monde'] = [46, 48]
r = P.reach((1, 16))
for p in [(16, 9), (24, 9), (18, 27), (22, 13), (31, 18), (35, 23), (8, 29), (45, 7)]:
    if p not in r and not any((p[0] + dx, p[1] + dy) in r for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
        P.show()
        sys.exit('plaine : case inaccessible %s' % (p,))
P.save()

V = carte.load('vallee')
for w in V['passages']:
    if w['vers'] == 'cendrelune':
        w['vers'], w['tx'], w['ty'] = 'plaine', 1, 16 + (w['y'] - 32)
carte.save('vallee', V)
C = carte.load('cendrelune')
for w in C['passages']:
    if w['vers'] == 'vallee':
        w['vers'], w['tx'], w['ty'] = 'plaine', 54, 16 + (w['y'] - 33)
carte.save('cendrelune', C)


def say(t): return {"action": "dire", "texte": t}


EV = {
    "plaine_entree": {"actions": [{"action": "drapeau", "nom": "plaine_vue"},
                                  say("Au-delà du col, une grande plaine dorée. Des moulins tournent au vent, et une rivière brille à l'est."),
                                  say("Les Monts Cendrelune fument à l'horizon.")]},
    "soin_plaine": {"actions": [{"action": "soigner"}, {"action": "reveil"}, say("Guérisseuse : Un bol de soupe et une bonne nuit… Voilà, vous êtes comme neufs !")]},
    "boutique_plaine": {"actions": [{"action": "boutique", "objets": ["potion", "superpotion", "ether", "remede", "plume", "lanterne", "lanterne_argent",
                                                                       "epee_fer", "baton_chene", "tunique_cuir", "amulette_vie", "gemme_vie"]}]},
    "atelier_plaine": {"actions": [say("L'atelier du charpentier. Ça sent la sciure et le vernis. Des planches neuves attendent contre le mur.")]},
    "meunier": {"pages": [
        {"si": {"drapeau": "pont_repare"}, "actions": [say("Meunier : Le pont tient bon, et la farine repart vers Forgeroc. La plaine revit !")]},
        {"actions": [say("Meunier : Bienvenue à Moulinbourg ! Avec la brume, plus personne ne passait le col. Vous êtes les premiers depuis des mois."),
                     say("Meunier : Mais si vous allez vers les Monts, ça ne va pas être simple : le pont de l'est est tout rongé."),
                     say("Meunier : Allez voir le charpentier, près de la rivière. Il saura quoi faire.")]}]},
    "charpentier": {"pages": [
        {"si": {"drapeau": "pont_repare"}, "actions": [say("Charpentier : Un pont tout neuf ! Il tiendra cent ans. Enfin, tant que les Rongecrins restent loin.")]},
        {"si": {"drapeau": "rongecrins_chasses"}, "actions": [
            say("Charpentier : Les Rongecrins sont partis ? Parfait ! Laissez-moi une petite heure…"),
            say("Le charpentier scie, cloue et chante. Bientôt, un pont tout neuf enjambe la rivière."),
            {"action": "drapeau", "nom": "pont_repare"}, {"action": "donner", "objet": "superpotion", "quantite": 2},
            say("Charpentier : Voilà ! Et prenez ça, pour la route."),
            {"action": "quete", "id": "pont", "fin": True}]},
        {"si": {"drapeau": "quete:pont"}, "actions": [say("Charpentier : La bande de Rongecrins niche au bord de la rivière, au sud du pont. Chassez-les !")]},
        {"actions": [
            say("Charpentier : Ce pont, je le répare tous les mois. Une bande de Rongecrins le ronge dès que j'ai le dos tourné !"),
            say("Charpentier : Si quelqu'un les chassait de la rivière, je pourrais le refaire pour de bon."),
            {"action": "quete", "id": "pont"},
            say("Charpentier : Ils nichent au sud du pont. Méfiez-vous du gros : il a des dents comme des burins.")]}]},
    "rongecrins": {"actions": [
        say("Une bande de Rongecrins grignote une poutre. Le plus gros vous montre des dents comme des burins !"),
        {"action": "combat", "nom": "Les Rongecrins", "ennemis": [{"espece": "rongecrin", "niveau": 11}, {"espece": "rongecrin", "niveau": 13, "pv": 3, "boss": True, "bouclier": 3},
                                                                  {"espece": "mulotin", "niveau": 11}],
         "renforts": [{"espece": "rongecrin", "niveau": 11}], "fuite": True, "capture": False,
         "victoire": [{"action": "drapeau", "nom": "rongecrins_chasses"}, say("Les Rongecrins détalent dans les hautes herbes. Le charpentier va pouvoir travailler.")]}]},
}
et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
Q = {"id": "pont", "nom": "Le pont de Moulinbourg", "lieu": "Plaine des Moulins",
     "etapes": [{"texte": "Chasser la bande de Rongecrins qui ronge le pont, au sud du pont de Moulinbourg."},
                {"si": {"drapeau": "rongecrins_chasses"}, "texte": "Les Rongecrins sont partis : prévenir le charpentier."}],
     "fin_texte": "Le pont de Moulinbourg est réparé."}
qt = open(D + 'quetes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in qt else '\n'
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
print('Plaine des Moulins écrite')
