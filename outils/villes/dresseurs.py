"""Dresseurs des routes (étape « 20 heures », 2026-10-20) : une trentaine de dresseurs le long des chemins de l'histoire,
comme dans Pokémon. Chacun regarde un chemin (`vue`), défie le joueur qui passe devant lui, puis donne de l'or (parfois
un objet) et une dernière réplique. Leur case est choisie ici : praticable, atteignable, entourée de cases praticables
(rien n'est bouché), loin des passages, des bâtiments, des énigmes et des autres habitants ; le script vérifie ensuite
que tout ce qui était accessible le reste. Équipe : créatures de la zone où il se tient (évoluées si leur niveau le permet), niveau de la zone.
Une seule fois (refuse si un dresseur « route_… » existe déjà).

    python outils/villes/dresseurs.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util as pu  # noqa: E402

D = carte.R
EV0 = json.load(open(D + 'evenements.json', encoding='utf-8'))
if any(k.startswith('route_') for k in EV0):
    sys.exit('Les dresseurs existent déjà.')
SP = {s['id']: s for s in json.load(open(D + 'especes.json', encoding='utf-8'))}

# carte : [(apparence, titre, réplique d'avant, réplique de défaite, réplique d'après, humain qui combat ou None, objet ou None)]
T = {
    'vallee': [('enfant', "Petit dresseur", "Hé ! Toi aussi tu as des créatures ? On se bat ? Allez, s'il te plaît !",
                "Ouin… Mon Mulotin a fait de son mieux.", "Quand je serai grand, je serai gardien, comme toi !", None, None)],
    'plaine': [('paysan', "Fermier", "Mes bêtes gardent les champs mieux que n'importe quel épouvantail. Voyons voir ce que valent les tiennes !",
                "Ça alors ! Tu ferais un sacré fermier.", "Les Rongecrins, eux, ne respectent ni les champs ni les ponts.", None, 'potion'),
               ('voyageuse', "Randonneuse", "La plaine est si belle depuis que la brume est partie ! Un petit combat pour fêter ça ?",
                "Quelle énergie ! Je vais m'asseoir un moment.", "D'ici, on voit fumer les Monts Cendrelune. On dit qu'un volcan y dort.", None, None)],
    'grotte': [('montagnard', "Spéléologue", "Chut ! L'écho réveille les chauves-souris… Trop tard, on se bat !",
                "Mon casque m'a sauvé la tête, mais pas le combat.", "Les cristaux de cette grotte chantent quand on les frappe. Écoutez !", None, 'ether')],
    'cendrelune': [('montagnard', "Mineur", "Ici, on cogne la roche toute la journée. Toi, je vais te cogner aussi !",
                    "Ouf. Tu as la tête plus dure que le granit.", "Forgeroc est juste au nord. Le meilleur forgeron du monde y travaille.", None, 'minerai'),
                   ('garde', "Garde de la route", "Halte ! Personne ne monte vers Forgeroc sans montrer sa force.",
                    "Passez. Et pardon pour le dérangement.", "Faites attention aux coulées de cendres, plus haut.", None, None)],
    'sylvenoire': [('voyageuse', "Herboriste", "Ces champignons sont à moi ! Enfin… à mes créatures. Et elles ont faim !",
                    "Bon, bon. Prenez quelques herbes, au moins.", "La forêt est plus calme depuis que les bandits sont partis.", None, 'remede')],
    'mines': [('montagnard', "Contremaître", "Les mines sont fermées ! Et ceux qui insistent passent par mes créatures.",
               "D'accord, d'accord. Mais ne touchez pas aux étais !", "Un troll s'est installé au fond. Personne n'ose plus y descendre.", None, 'minerai')],
    'col': [('montagnard', "Berger", "Mes bêtes n'aiment pas les inconnus. Moi non plus, d'ailleurs.",
             "Hmpf. Tu as du cran, je te l'accorde.", "La Harpie de givre fond sur les troupeaux. Méfiance, là-haut.", None, None),
            ('voyageuse', "Alpiniste", "J'ai gravi tous les sommets de Brumeval ! Il me manque juste… un bon combat.",
             "Le vent m'a déconcentrée. Si, si, c'est le vent.", "Au-delà du col, les Pics Givrés. Couvrez-vous bien.", None, 'superpotion')],
    'pics': [('mage_givre', "Apprentie du givre", "Mon maître dit que je dois m'entraîner sur des voyageurs. Désolée !",
              "Je vais devoir relire mes leçons…", "Le temple est tout en haut. Les chevaliers le gardent jour et nuit.", 'mage_givre', None)],
    'glacier': [('garde', "Chasseur de glace", "Je taille la glace pour les tavernes de Givreval. Et je taille aussi les intrus !",
                 "Glacé. Je suis glacé.", "Un colosse dort sous le glacier. Ne faites pas trop de bruit.", None, 'ether')],
    'val': [('ermite', "Ermite", "Les esprits me parlent la nuit. Ils disent que tu dois te battre. Maintenant.",
             "Les esprits se sont trompés. Ça arrive.", "Allumez les lanternes, et les esprits trouveront le chemin.", None, None),
            ('villageoise', "Veilleuse", "Je garde les lanternes du Val. Qui s'approche doit montrer patte blanche !",
             "Pardon. On n'est jamais trop prudents, ici.", "La nuit, les murmures du Val deviennent des chants.", None, 'superpotion')],
    'recif': [('pecheur', "Pêcheur de crevettes", "Ma pêche a été mauvaise. Alors je me défoule sur les passants !",
               "Même les crevettes se moquent de moi, maintenant.", "La mer se calme quand le vent tourne à l'ouest.", None, None)],
    'corail': [('pecheur', "Plongeur", "Je plonge chercher des perles. Et toi, tu vas plonger… dans la défaite !",
                "Glouglou. Je reprends mon souffle.", "Au fond de la Grotte des Marées dort un gardien très ancien.", None, 'superpotion'),
               ('villageoise', "Ramasseuse de coquillages", "Ne marche pas sur mes coquillages ! Mes créatures vont te le faire comprendre.",
                "Bon. Fais juste attention où tu mets les pieds.", "Les plus beaux coquillages sont du côté de la grotte.", None, None)],
    'marees': [('pecheur', "Chercheur de trésors", "Ce trésor est à moi ! Personne ne passera avant moi !",
                "Bah… De toute façon, je n'ai trouvé que des algues.", "La marée monte vite, ici. Ne traînez pas.", None, 'ether')],
    'ruines': [('ermite', "Archéologue", "Ne touchez à rien ! Ces ruines sont plus vieilles que Brumeval elle-même !",
                "Pardon… L'excitation de la découverte.", "Les inscriptions parlent d'un orage enfermé dans une tour.", None, 'elixir')],
    'epave': [('bandit', "Pilleur d'épaves", "Cette épave est à nous ! Va chercher ton or ailleurs !",
               "Aïe ! Bon, il en reste assez pour deux…", "On dit que le Léviathan dort encore dans la cale.", 'bandit', None)],
    'tour': [('mage_givre', "Mage de l'orage", "L'orage m'a appelée ici. Il veut que je t'arrête !",
              "L'orage… s'est tu, dans ma tête. Merci.", "Le Seigneur des Orages attend au sommet. Soyez prêts.", 'mage_givre', 'elixir'),
             ('chevalier', "Chevalier foudroyé", "Ma lame a été frappée par la foudre. Elle a soif de combat !",
              "Ma lame… se calme enfin.", "Montez. Et ne regardez pas en bas.", 'chevalier', None)],
    'lueurs': [('ermite', "Gardien des lueurs", "Les lueurs ont peur de vous. Je dois vous mettre à l'épreuve.",
                "Elles n'ont plus peur. Moi non plus.", "La nuit, les lueurs dansent au-dessus des étangs.", None, None),
               ('voyageuse', "Cartographe", "Je dessine la carte d'Aube. Il me manque un combat pour la légende !",
                "Je noterai : « Ici, une défaite mémorable ».", "Le Marais Pâle est au nord. Prenez des remèdes.", None, 'elixir')],
    'marais': [('docteur', "Botaniste", "Ces fleurs pâles soignent tout. Sauf la défaite. Défendez-vous !",
                "Je vais devoir me soigner moi-même.", "La reine du marais garde une larme de lumière.", None, 'elixir'),
               ('pecheur', "Pêcheur du marais", "Rien ne mord, ici. À part mes créatures !",
                "Ça mord pas, ça mord pas… Même pas toi.", "Les poissons d'Aube ont des écailles qui brillent la nuit.", None, None)],
    'gouffre': [('chevalier', "Sentinelle d'Aube", "Le gouffre est interdit ! Ilya l'a ordonné… je crois.",
                 "Peut-être qu'elle vous attendait, finalement.", "Les rochers du gouffre cachent une porte de lumière.", 'chevalier', 'elixir')],
    'citadelle': [('mage_givre', "Dernière apprentie", "Je suis la dernière apprentie des gardiens. Je ne reculerai pas !",
                   "Vous êtes ceux qu'on attendait, n'est-ce pas ?", "La Brume Noire attend au cœur de la citadelle. Que la lumière vous guide.", 'mage_givre', None)],
}


def evolved(sp, lvl):
    s = SP[sp]
    while s.get('evolution') and s['evolution'].get('niveau', 999) <= lvl and s['evolution'].get('espece') in SP:
        s = SP[s['evolution']['espece']]
    return s['id']


def zone_at(m, x, y):
    zs = m.get('zones', [])
    for z in zs:
        if z['x'] <= x < z['x'] + z['l'] and z['y'] <= y < z['y'] + z['h']:
            return [z]
    return sorted(zs, key=lambda z: z['niveau_max'])[:1]


def creatures(m, zs):
    out = []
    for z in zs:
        for c in z.get('creatures', []):
            if c not in out and c in SP:
                out.append(c)
    return out


def zone_levels(zs):
    return max(z['niveau_max'] for z in zs) if zs else 10


def pick_cells(m, n):
    R = m['tuiles']
    H, Wd = len(R), len(R[0])
    reach = pu.reachable(m)
    occ = pu.occupied(m)
    avoid = []  # (x, y, rayon)
    for w in m.get('passages', []):
        avoid.append((w['x'], w['y'], 6))
    for o in m.get('habitants', []):
        avoid.append((o['x'], o['y'], 5))
    for k in ('coffres', 'panneaux'):
        for o in m.get(k, []):
            avoid.append((o['x'], o['y'], 2))
    for b in m.get('batiments', []):
        avoid.append((b['x'] + b['l'] // 2, b['y'] + b['h'] // 2, 6 + max(b['l'], b['h']) // 2))
    for b in m.get('boss', []):
        avoid.append((b['x'], b['y'], 4))
    for t in m.get('declencheurs', []):
        avoid.append((t['x'], t['y'], 4))
    for y in range(H):
        for x in range(Wd):
            if R[y][x] in 'OPDk':
                avoid.append((x, y, 5))
    if m.get('enigme'):
        e = m['enigme']
        avoid.append((e.get('x', -99), e.get('y', -99), 5))
    has_path = any('=' in r for r in R)
    dirs = {'bas': (0, 1), 'haut': (0, -1), 'gauche': (-1, 0), 'droite': (1, 0)}
    cand = []
    for (x, y) in reach:
        if (x, y) in occ or R[y][x] in '=q' or not (2 <= x < Wd - 2 and 2 <= y < H - 2):
            continue
        if not all(R[y + dy][x + dx] in pu.W for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
            continue
        if any(abs(x - ax) + abs(y - ay) < r for ax, ay, r in avoid):
            continue
        for d, (dx, dy) in dirs.items():
            line = [(x + dx * k, y + dy * k) for k in (1, 2, 3)]
            if not all(0 <= a < Wd and 0 <= b < H and R[b][a] in pu.W for a, b in line):
                continue
            if has_path and not any(R[b][a] == '=' for a, b in line[:2]):
                continue
            cand.append((x, y, d))
            break
    cand.sort(key=lambda c: (c[1], c[0]))
    chosen = []
    for _ in range(n):
        best = None
        for c in cand:
            if any((c[0], c[1]) == (o[0], o[1]) for o in chosen):
                continue
            far = min([abs(c[0] - o[0]) + abs(c[1] - o[1]) for o in chosen] +
                      [abs(c[0] - w['x']) + abs(c[1] - w['y']) for w in m.get('passages', [])] + [99])
            if best is None or far > best[0]:
                best = (far, c)
        if best is None:
            sys.exit('%s : pas de place pour un dresseur' % m['nom'])
        chosen.append(best[1])
    return chosen


def say(t): return {"action": "dire", "texte": t}


EV = {}
k = 0
for mid, trainers in T.items():
    m = carte.load(mid)
    before = pu.reachable(m)
    cells = pick_cells(m, len(trainers))
    for j, ((app, titre, avant, defaite, apres, humain, objet), (x, y, d)) in enumerate(zip(trainers, cells)):
        zs = zone_at(m, x, y)
        lvl = zone_levels(zs)
        pool = creatures(m, zs) or creatures(m, m.get('zones', []))
        k += 1
        tid = 'route_%s%d' % (mid, j + 1)
        a = pool[(k * 3) % len(pool)]
        b = pool[(k * 3 + 1) % len(pool)]
        c = pool[(k * 3 + 2) % len(pool)]
        team = [{"espece": humain, "niveau": lvl + 1}] if humain else [{"espece": evolved(a, lvl + 1), "niveau": lvl + 1}]
        team.append({"espece": evolved(b, lvl), "niveau": lvl})
        renf = [{"espece": evolved(c, lvl), "niveau": lvl}]
        if lvl >= 30:
            team.append({"espece": evolved(a if humain else c, lvl), "niveau": lvl})
        win = [{"action": "drapeau", "nom": tid}, say(titre + " : " + defaite), {"action": "or", "quantite": 20 * lvl + 40}]
        if objet:
            win.append({"action": "donner", "objet": objet, "quantite": 1})
        EV[tid] = {"pages": [
            {"si": {"sans_drapeau": tid}, "actions": [
                say(titre + " : " + avant),
                {"action": "combat", "nom": titre, "ennemis": team, "renforts": renf, "fuite": False, "capture": False, "victoire": win}]},
            {"actions": [say(titre + " : " + apres)]}]}
        m['habitants'].append({"x": x, "y": y, "apparence": app, "direction": d, "evenement": tid, "vue": 3, "vue_jusqua": tid})
        print('%-10s %-28s (%2d, %2d) %-6s N.%d %s' % (mid, titre, x, y, d, lvl, [e['espece'] for e in team + renf]))
    after = pu.reachable(m)
    lost = before - after - set((x, y) for x, y, _ in cells)
    if lost:
        sys.exit('%s : des cases deviennent inaccessibles %s' % (mid, sorted(lost)[:5]))
    carte.save(mid, m)

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
print('%d dresseurs écrits' % len(EV))
