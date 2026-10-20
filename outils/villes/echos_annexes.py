"""Contenu annexe de l'acte 4 (étape « 20 heures », 2026-10-20) : deux primes sur l'île des Échos (le Fossiloup
alpha dans la Forêt Pétrifiée, la Braisaure ancestrale dans le Cratère), la quête de la cloche du vieux phare (trois
fragments dans des coffres de la forêt, du cratère et du sanctuaire) et l'étude d'Oriane (montrer trois créatures de
l'île). Une seule fois (refuse si la quête « cloche » existe). Vérifie que rien ne devient inaccessible.

    python outils/villes/echos_annexes.py
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
if any(q['id'] == 'cloche' for q in QT):
    sys.exit('Déjà fait.')
maps, before = {}, {}


def M(mid):
    if mid not in maps:
        maps[mid] = carte.load(mid)
        before[mid] = pu.reachable(maps[mid])
    return maps[mid]


def spot2x2(mid, x, y):
    """Case haut-gauche d'un bloc 2x2 libre (et son tour), atteignable, la plus proche de (x, y)."""
    m = M(mid)
    ok, occ = pu.reachable(m), pu.occupied(m)
    z = m.get('enigme', {}).get('zone')
    if z:
        occ |= {(a, b) for a in range(z['x'], z['x'] + z['l']) for b in range(z['y'], z['y'] + z['h'])}
    best = None
    for (xx, yy) in ok:
        if any((xx + dx, yy + dy) not in ok or (xx + dx, yy + dy) in occ for dx in range(-1, 3) for dy in range(-1, 3)):
            continue
        d = abs(xx - x) + abs(yy - y)
        if best is None or d < best[0]:
            best = (d, xx, yy)
    assert best, (mid, x, y)
    return best[1], best[2]


def chest(mid, x, y, item):
    m = M(mid)
    p = pu.near_reach(m, x, y, ring=1)
    assert p, (mid, x, y)
    m['coffres'].append({"x": p[0], "y": p[1], "objet": item, "quantite": 1})


def npc(mid, x, y, look, ev, d='bas'):
    m = M(mid)
    p = pu.near_reach(m, x, y, ring=1)
    assert p, (mid, x, y)
    m['habitants'].append({"x": p[0], "y": p[1], "apparence": look, "direction": d, "evenement": ev})
    return p


def say(t): return {"action": "dire", "texte": t}
def page(acts, si=None):
    p = {}
    if si:
        p["si"] = si
    p["actions"] = acts
    return p


# Primes ---------------------------------------------------------------------------------------------------------------
fx, fy = spot2x2('pierres', 38, 26)
bx, by = spot2x2('cratere', 30, 26)
PR = [
    {"id": "fossiloup_alpha", "nom": "Le Fossiloup alpha", "etoiles": 5, "carte": "pierres", "x": fx, "y": fy, "espece": "fossiloup",
     "si": {"drapeau": "echos_vue"}, "lieu": "Forêt Pétrifiée", "description":
     "Un loup de pierre deux fois plus grand que les autres. Sa meute garde la forêt comme un trésor.",
     "arrivee": "Un hurlement fait trembler les arbres de pierre : le Fossiloup alpha et sa meute vous encerclent !",
     "combat": {"fuite": False, "ennemis": [{"espece": "fossiloup", "niveau": 88},
                                           {"espece": "fossiloup", "niveau": 92, "pv": 5, "boss": True, "bouclier": 10,
                                            "phases": [{"pv": 0.5, "message": "L'alpha hurle : la meute accourt !",
                                                        "renforts": [{"espece": "fossiloup", "niveau": 88}]}]},
                                           {"espece": "petrolin", "niveau": 88}]},
     "or": 12000, "objets": [{"id": "gemme_force"}, {"id": "elixir", "quantite": 2}]},
    {"id": "braisaure_ancestrale", "nom": "La Braisaure ancestrale", "etoiles": 5, "carte": "cratere", "x": bx, "y": by, "espece": "braisaure",
     "si": {"drapeau": "echos_fin"}, "lieu": "Cratère des Échos", "description":
     "Une Braisaure plus vieille que l'île elle-même. Depuis que l'écho s'est tu, elle sort de la lave pour chasser.",
     "arrivee": "La lave bouillonne : une Braisaure énorme en sort, couverte de roche en fusion !",
     "combat": {"fuite": False, "ennemis": [{"espece": "petrolin", "niveau": 86},  # 3e réglage : escorte sans feu
                                           {"espece": "braisaure", "niveau": 90, "pv": 4, "boss": True, "bouclier": 10,  # réglé après le joueur automatique
                                            "phases": [{"pv": 0.6, "message": "La Braisaure crache de la lave : l'air brûle !", "bonus": {"attaque": 1}},
                                                       {"pv": 0.3, "message": "Sa carapace de roche se fend… elle devient furieuse !",
                                                        "bonus": {"vitesse": 1}}]},
                                           {"espece": "fossiloup", "niveau": 86}]},
     "or": 20000, "objets": [{"id": "gemme_maitre"}, {"id": "elixir", "quantite": 3}]},
]

# Quête : la cloche du vieux phare -------------------------------------------------------------------------------------
ITEMS = [{"id": "fragment_cloche", "nom": "Fragment de cloche", "icone": "ic_minerai", "prix": 0, "important": True,
          "description": "Un morceau de bronze vert-de-gris. Quand on le frappe, il sonne encore. Le sonneur de Port-Écho en cherche."},
         {"id": "cloche_echo", "nom": "Clochette d'écho", "icone": "ic_acc_3", "prix": 6000, "equipement": "accessoire",
          "description": "Une clochette fondue avec le bronze de la vieille cloche. Son tintement donne du courage à qui la porte.",
          "bonus": {"pv": 50, "resistance": 10, "vitesse": 4}}]
npc('echos', 34, 21, 'montagnard', 'quete_cloche', 'gauche')
chest('pierres', 36, 8, 'fragment_cloche')
chest('cratere', 12, 26, 'fragment_cloche')
chest('sanctuaire_echos', 20, 16, 'fragment_cloche')
EV = {"quete_cloche": {"pages": [
    page([say("Sonneur : Écoutez… La cloche du vieux phare sonne de nouveau. Les bateaux la suivent dans la brume.")], {"drapeau": "quete:cloche:fin"}),
    page([{"action": "retirer", "objet": "fragment_cloche", "quantite": 3}, say("Sonneur : Les trois fragments ! Je vais refondre la cloche cette nuit."),
          say("Sonneur : Avec le bronze qui reste, j'ai fait cette clochette. Prenez-la : elle porte chance aux voyageurs."),
          {"action": "donner", "objet": "cloche_echo", "quantite": 1}, {"action": "or", "quantite": 5000}, {"action": "quete", "id": "cloche", "fin": True}],
         {"drapeau": "quete:cloche", "objet_min": {"fragment_cloche": 3}}),
    page([say("Sonneur : Il me manque trois fragments : un dans la Forêt Pétrifiée, un dans le Cratère, un dans le Sanctuaire.")], {"drapeau": "quete:cloche"}),
    page([say("Sonneur : Autrefois, la cloche du phare guidait les bateaux. Quand l'écho est devenu une ombre, elle s'est brisée en trois."),
          {"action": "question", "texte": "Chercher les trois fragments de la cloche ?",
           "oui": [{"action": "quete", "id": "cloche"}, say("Sonneur : Merci ! Ils ont roulé jusque dans la forêt, le cratère et le sanctuaire.")],
           "non": [say("Sonneur : Le phare restera muet, alors.")]}])]},
    # Quête : l'étude d'Oriane (montrer trois créatures de l'île)
    "etude_oriane": {"pages": [
        page([say("Élève d'Oriane : Le carnet de ma maîtresse est complet. Merci de m'avoir laissée les approcher.")], {"drapeau": "quete:etude_oriane:fin"}),
        page([say("Élève d'Oriane : Un Pétrolin, une Échoplume et un Spectrécho… Ils vous suivent comme de vieux amis."),
              say("Élève d'Oriane : Prenez ceci. C'était la gemme de mon maître ; elle sera mieux entre vos mains."),
              {"action": "donner", "objet": "gemme_maitre", "quantite": 1}, {"action": "or", "quantite": 6000},
              {"action": "quete", "id": "etude_oriane", "fin": True}],
             {"drapeau": "quete:etude_oriane", "membre": ["petrolin", "echoplume", "spectrecho"]}),
        page([say("Élève d'Oriane : Montrez-moi un Pétrolin, une Échoplume et un Spectrécho, tous les trois dans votre équipe.")], {"drapeau": "quete:etude_oriane"}),
        page([say("Élève d'Oriane : Les créatures de l'île sont nées de l'écho. Je voudrais les étudier de près, mais elles me fuient."),
              {"action": "question", "texte": "Aider l'élève d'Oriane ?",
               "oui": [{"action": "quete", "id": "etude_oriane"}, say("Élève d'Oriane : Un Pétrolin, une Échoplume et un Spectrécho. Ensemble, dans votre équipe.")],
               "non": [say("Élève d'Oriane : Une autre fois, peut-être.")]}])]},
}
npc('echos', 9, 13, 'voyageuse', 'etude_oriane', 'droite')
QS = [{"id": "cloche", "nom": "La cloche du vieux phare", "lieu": "Port-Écho",
       "etapes": [{"texte": "Rapporter trois fragments de cloche au sonneur de Port-Écho (Forêt Pétrifiée, Cratère, Sanctuaire)."},
                  {"si": {"objet_min": {"fragment_cloche": 3}}, "texte": "Vous avez les trois fragments : les apporter au sonneur."}],
       "fin_texte": "La cloche du vieux phare sonne de nouveau."},
      {"id": "etude_oriane", "nom": "Les créatures de l'écho", "lieu": "Port-Écho",
       "etapes": [{"texte": "Avoir ensemble un Pétrolin, une Échoplume et un Spectrécho dans l'équipe, et les montrer à l'élève d'Oriane, à Port-Écho."}],
       "fin_texte": "L'étude des créatures de l'écho est terminée."}]

# Vérification et écriture --------------------------------------------------------------------------------------------
for mid, m in maps.items():
    added = {(o['x'], o['y']) for k in ('habitants', 'coffres') for o in m[k]}
    lost = before[mid] - pu.reachable(m) - added
    if lost:
        sys.exit('%s : des cases deviennent inaccessibles %s' % (mid, sorted(lost)[:5]))
for mid, m in maps.items():
    carte.save(mid, m)
# primes.json est écrit à la main : chaque prime sur cinq lignes (identité, lieu, arrivée, combat, récompense)
pt = open(D + 'primes.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in pt else '\n'
end = pt.rstrip().rstrip('}').rstrip().rstrip(']').rstrip()


def j(v): return json.dumps(v, ensure_ascii=False)


for b in PR:
    head = ', '.join('%s: %s' % (j(k), j(b[k])) for k in ('id', 'nom', 'etoiles', 'carte', 'x', 'y', 'espece', 'si'))
    end += ',' + nl + '    {' + head + ',' + nl + '     "lieu": %s, "description": %s,' % (j(b['lieu']), j(b['description'])) + nl + \
        '     "arrivee": %s,' % j(b['arrivee']) + nl + '     "combat": %s,' % carte.inline(b['combat']) + nl + \
        '     "or": %d, "objets": %s}' % (b['or'], carte.inline(b['objets']))
open(D + 'primes.json', 'w', encoding='utf-8', newline='').write(end + nl + '  ]' + nl + '}' + nl)
assert json.load(open(D + 'primes.json', encoding='utf-8'))['primes'][-1]['id'] == 'braisaure_ancestrale'
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
k = [o['id'] for o in OBJ].index('coeur_echo') + 1
for i2, o in enumerate(ITEMS):
    OBJ.insert(k + i2, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
print('Annexes de l\'acte 4 écrites ; primes en', (fx, fy), (bx, by))
