"""Cartes au trésor (étape « 20 heures », 2026-10-20) : Corbin, un vieux chasseur de trésors du port (un habitant qui
n'avait rien à dire), donne la première carte (quête « tresors ») ; chaque carte indique dans sa description où creuser,
et le trésor caché là (un coffre avec « si » : invisible tant qu'on n'a pas la carte) contient la carte suivante. Huit
cartes, dans les lieux de l'acte 1 et du début de l'acte 2 ; la dernière mène à la Boussole de Corbin. Corbin récompense
la moitié du chemin et la fin. La cache est la case accessible la plus éloignée à pied des passages et des bâtiments
(un coin perdu) ; l'indice dit dans quelle partie de la carte elle est. Une seule fois (refuse si la quête « tresors »
existe). Vérifie que rien de déjà accessible ne devient inaccessible.

    python outils/villes/tresors.py
"""
import json
import os
import sys
from collections import deque

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util as pu  # noqa: E402

D = carte.R
QT = [json.loads(l.strip().rstrip(',')) for l in open(D + 'quetes.json', encoding='utf-8') if l.strip().startswith('{')]
if any(q['id'] == 'tresors' for q in QT):
    sys.exit('Déjà fait.')

# Ordre de la chasse : de la Vallée au Récif (tout est ouvert après la Mère des Brumes, quand on arrive au port)
SPOTS = [('vallee', "Vallée de Brumeval"), ('sylvenoire', "Sylve-Noire"), ('plaine', "Plaine des Moulins"), ('cendrelune', "Monts Cendrelune"),
         ('col', "Col des Hurlevents"), ('pics', "Pics Givrés"), ('val', "Val des Murmures"), ('recif', "Récif d'Écume")]


def where(m, x, y):
    """« au nord-est », « à l'ouest », « au centre »… d'après la place de la case dans la carte."""
    W, H = len(m['tuiles'][0]), len(m['tuiles'])
    v = 'nord' if y < H / 3 else 'sud' if y > 2 * H / 3 else ''
    h = 'ouest' if x < W / 3 else 'est' if x > 2 * W / 3 else ''
    if v and h:
        return 'au ' + v + '-' + h
    if v or h:
        w = v or h
        return ("à l'" if w[0] in 'eo' else 'au ') + w
    return 'au centre'


def hideout(m):
    """La case accessible la plus loin à pied des passages, portes et habitants, avec un voisin accessible."""
    ok = pu.reachable(m)
    occ = pu.occupied(m)
    src = [(w['x'], w['y']) for w in m.get('passages', [])]
    for b in m.get('batiments', []):
        src.append((b['x'] + b['l'] // 2, b['y'] + b['h']))
    for n in m.get('habitants', []):
        src.append((n['x'], n['y']))
    dist = {}
    q = deque()
    for s in src:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                c = (s[0] + dx, s[1] + dy)
                if c in ok and c not in dist:
                    dist[c] = 0
                    q.append(c)
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c = (x + dx, y + dy)
            if c in ok and c not in dist:
                dist[c] = dist[(x, y)] + 1
                q.append(c)
    R = m['tuiles']
    for c, d in sorted(dist.items(), key=lambda e: -e[1]):
        x, y = c
        if c in occ or R[y][x] not in '.,=':  # sur l'herbe ou le chemin, pas sur un pont ni dans une grotte
            continue
        m['coffres'].append({"x": x, "y": y})  # ne bouche-t-il rien ?
        lost = ok - pu.reachable(m) - {c}
        m['coffres'].pop()
        if not lost:
            return x, y
    raise SystemExit('aucune cache')


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


maps, before, ITEMS, cells = {}, {}, [], []
for k, (mid, name) in enumerate(SPOTS, 1):
    m = carte.load(mid)
    maps[mid], before[mid] = m, pu.reachable(m)
    x, y = hideout(m)
    cells.append((mid, x, y))
    ITEMS.append({"id": "carte_tresor_%d" % k, "nom": "Carte au trésor n°%d" % k, "icone": "ic_livre", "prix": 0, "important": True,
                  "description": "Un vieux parchemin de Corbin. Une croix rouge : %s, %s de la carte, dans un coin où personne ne va." % (name, where(m, x, y))})
    nxt = "carte_tresor_%d" % (k + 1) if k < len(SPOTS) else "boussole_corbin"
    m['coffres'].append({"x": x, "y": y, "objet": nxt, "quantite": 1, "si": {"objet": "carte_tresor_%d" % k}})
ITEMS.append({"id": "boussole_corbin", "nom": "Boussole de Corbin", "icone": "ic_acc_6", "prix": 9000, "equipement": "accessoire",
              "description": "La boussole du vieux Corbin. Elle ne montre pas le nord : elle montre ce qu'on cherche.",
              "bonus": {"pv": 40, "vitesse": 6, "esquive": 6, "critique": 4}})
last = cells[-1]
found_all = {"drapeau": "coffre:%s:%d:%d" % last}
half = {"objet": "carte_tresor_5"}

port = carte.load('port')
maps['port'], before['port'] = port, pu.reachable(port)
cb = [h for h in port['habitants'] if h.get('apparence') == 'montagnard' and 'evenement' not in h]
assert cb, 'Corbin (port)'
cb[0]['evenement'] = 'corbin'
EV = {'corbin': {"pages": [
    page([say("Corbin : Ma boussole te va bien. Moi, je n'ai plus besoin de chercher : j'ai trouvé quelqu'un pour le faire à ma place.")],
         {"drapeau": "quete:tresors:fin"}),
    page([say("Corbin : Tu as trouvé le dernier ! Ma boussole ! Quarante ans que je l'avais cachée là, pour être sûr que personne ne la trouve…"),
          say("Corbin : … y compris moi, apparemment. Garde-la. Et prends ça : tu l'as bien mérité."),
          gold(12000), give("gemme_maitre"), qend('tresors')], dict(found_all, drapeau=["quete:tresors", found_all["drapeau"]])),
    page([say("Corbin : La carte n°5 ! Tu vas plus vite que moi, à ton âge. Tiens, pour la route."),
          give("elixir", 3), gold(5000), flag("tresor_mi"),
          say("Corbin : Continue : chaque trésor contient la carte du suivant. Le dernier est le plus beau.")],
         {"drapeau": "quete:tresors", "sans_drapeau": "tresor_mi", "objet": "carte_tresor_5"}),
    page([say("Corbin : Lis bien la carte que tu as (dans le sac) : la croix dit où creuser. Le trésor ne se voit que si tu as la carte.")],
         {"drapeau": "quete:tresors"}),
    page([say("Corbin : Hé, toi ! Tu as l'air d'aimer les coins où personne ne va. Moi aussi, à ton âge."),
          say("Corbin : J'ai caché des trésors partout, autrefois. Chacun contient la carte du suivant. Mais mes jambes ne me portent plus."),
          ask("Partir à la chasse aux trésors de Corbin ?", [qstart('tresors'), give("carte_tresor_1"),
                                                             say("Corbin : Voilà la première carte. La croix dit où chercher. Bonne chasse !")],
              [say("Corbin : Les trésors attendront. Ils ont l'habitude.")])])]}}
QS = [{"id": "tresors", "nom": "Les trésors de Corbin", "lieu": "Port d'Écume",
       "etapes": [{"texte": "Suivre les cartes au trésor de Corbin : chaque trésor contient la carte du suivant (lire la carte dans le sac)."},
                  {"si": {"objet": "carte_tresor_5", "sans_drapeau": "tresor_mi"}, "texte": "Montrer la carte n°5 à Corbin, au port."},
                  {"si": found_all, "texte": "Le dernier trésor est trouvé : retourner voir Corbin, au port."}],
       "fin_texte": "Tous les trésors de Corbin sont retrouvés."}]

for mid, m in maps.items():
    added = set((o['x'], o['y']) for o in m['coffres'])
    lost = before[mid] - pu.reachable(m) - added
    if lost:
        sys.exit('%s : des cases deviennent inaccessibles %s' % (mid, sorted(lost)[:5]))
for mid, m in maps.items():
    carte.save(mid, m)
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
body = qt.rstrip()[:-1].rstrip()
for q in QS:
    body += ',' + nl + json.dumps(q, ensure_ascii=False)
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)
OBJ = json.load(open(D + 'objets.json', encoding='utf-8'))
k = [o['id'] for o in OBJ].index('lanterne_veilleur') + 1
for i2, o in enumerate(ITEMS):
    OBJ.insert(k + i2, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
for (mid, x, y), it in zip(cells, ITEMS):
    print(mid, x, y, '|', it['description'])
print('8 trésors écrits')
