"""Cinq quêtes de plus pour les actes 2 et 3 (étape « 20 heures », 2026-10-20), qui en manquaient : les éclats du
phare et l'enfant du marais (Havre-d'Aube), le spectre du gouffre (garde de Havre-d'Aube), les pages de l'orage
(Ruines englouties → Tour → Grotte des Marées) et la carapace de Corail (montrer une créature). Une seule fois
(refuse si la quête « phare_aube » existe). Vérifie que rien de déjà accessible ne devient inaccessible.

    python outils/villes/quetes_actes.py
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
if any(q['id'] == 'phare_aube' for q in QT):
    sys.exit('Déjà fait.')

maps, before = {}, {}


def M(mid):
    if mid not in maps:
        maps[mid] = carte.load(mid)
        before[mid] = pu.reachable(maps[mid])
    return maps[mid]


def npc(mid, x, y, look, ev, d='bas', si=None, cache=None):
    m = M(mid)
    p = pu.near_reach(m, x, y, ring=1)
    assert p, (mid, x, y)
    o = {"x": p[0], "y": p[1], "apparence": look, "direction": d, "evenement": ev}
    if si:
        o["si"] = si
    if cache:
        o["cache_si"] = cache
    m['habitants'].append(o)
    return p


def chest(mid, x, y, item, q=1):
    m = M(mid)
    p = pu.near_reach(m, x, y, ring=1)
    assert p, (mid, x, y)
    m['coffres'].append({"x": p[0], "y": p[1], "objet": item, "quantite": q})
    return p


def boss(mid, x, y, sp, fl, ev, si):
    m = M(mid)
    ok, occ = pu.reachable(m), pu.occupied(m)
    z = m.get('enigme', {}).get('zone')
    if z:  # jamais dans la salle d'une énigme : il gênerait les rochers
        occ |= {(a, b) for a in range(z['x'], z['x'] + z['l']) for b in range(z['y'], z['y'] + z['h'])}
    best = None
    for (xx, yy) in ok:
        cells = [(xx + dx, yy + dy) for dx in range(-1, 3) for dy in range(-1, 3)]
        if any(c not in ok or c in occ for c in cells):
            continue
        d = abs(xx - x) + abs(yy - y)
        if best is None or d < best[0]:
            best = (d, xx, yy)
    assert best, (mid, x, y)
    m['boss'].append({"x": best[1], "y": best[2], "espece": sp, "drapeau": fl, "evenement": ev, "si": si})
    return best[1:]


def say(t): return {"action": "dire", "texte": t}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}
def take(i, q=1): return {"action": "retirer", "objet": i, "quantite": q}
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
def done(q): return {"drapeau": "quete:%s:fin" % q}


EV, QS, ITEMS = {}, [], []

# 1. Les éclats du phare (Havre-d'Aube) : trois éclats de lueur, dans le Bois des Lueurs, le Marais Pâle et le Gouffre
ITEMS.append({"id": "eclat_lueur", "nom": "Éclat de lueur", "icone": "ic_gemme_cristal",
              "description": "Un éclat de cristal qui garde un peu de lumière, même dans le noir. Le gardien du phare d'Aube en cherche.", "prix": 0, "important": True})
ITEMS.append({"id": "amulette_aube", "nom": "Amulette d'Aube", "icone": "ic_acc_7",
              "description": "Le cristal du vieux phare, serti d'argent. Il réchauffe le cœur de qui le porte.",
              "prix": 5000, "equipement": "accessoire", "bonus": {"pv": 60, "magie": 8, "resistance": 6}})
npc('aube', 33, 22, 'ermite', 'quete_phare_aube', 'gauche')
chest('lueurs', 30, 26, 'eclat_lueur')
chest('marais', 24, 20, 'eclat_lueur')
chest('gouffre', 18, 22, 'eclat_lueur')
EV['quete_phare_aube'] = {"pages": [
    page([say("Gaspard : Le phare brille de nouveau. Les bateaux rentrent au port sans se perdre. Merci, mes enfants.")], done('phare_aube')),
    page([take("eclat_lueur", 3), say("Gaspard : Trois éclats de lueur ! Regardez comme ils brillent…"),
          say("Gaspard : Le phare d'Aube va se rallumer. Prenez ceci : c'était le cristal du vieux phare. Il n'a plus d'usage, mais il porte bonheur."),
          give("amulette_aube"), gold(4000), qend('phare_aube')],
         {"drapeau": "quete:phare_aube", "objet_min": {"eclat_lueur": 3}}),
    page([say("Gaspard : Il me faut trois éclats de lueur. Un dans le Bois des Lueurs, un dans le Marais Pâle, un au fond du Gouffre.")], {"drapeau": "quete:phare_aube"}),
    page([say("Gaspard : Je garde le phare de Havre-d'Aube depuis cinquante ans. Mais son cristal s'est éteint quand la brume noire est montée."),
          say("Gaspard : Avec trois éclats de lueur, je pourrais le rallumer. Mais mes vieilles jambes ne me portent plus si loin."),
          ask("Chercher trois éclats de lueur ?", [qstart('phare_aube'), say("Gaspard : Merci ! On les trouve dans de vieux coffres : au Bois des Lueurs, au Marais Pâle et au Gouffre.")],
              [say("Gaspard : Le phare attendra. Il a l'habitude.")])])]}
QS.append({"id": "phare_aube", "nom": "Le phare d'Aube", "lieu": "Havre-d'Aube",
           "etapes": [{"texte": "Rapporter trois éclats de lueur à Gaspard, le gardien du phare de Havre-d'Aube (Bois des Lueurs, Marais Pâle, Gouffre)."},
                      {"si": {"objet_min": {"eclat_lueur": 3}}, "texte": "Vous avez les trois éclats : les apporter à Gaspard."}],
           "fin_texte": "Le phare de Havre-d'Aube brille de nouveau."})

# 2. L'enfant du marais (Havre-d'Aube → Marais Pâle) : un enfant égaré, entouré de créatures
npc('aube', 8, 18, 'villageoise', 'quete_enfant_marais', 'droite')
npc('marais', 30, 26, 'enfant', 'enfant_marais', 'bas', si={"drapeau": "quete:enfant_marais"}, cache="enfant_retrouve")
npc('aube', 10, 18, 'enfant', 'enfant_rentre', 'gauche', si={"drapeau": "enfant_retrouve"})
EV['quete_enfant_marais'] = {"pages": [
    page([say("Lise : Nilo ne va plus jamais seul au marais. Il me l'a promis. Enfin… il a promis.")], done('enfant_marais')),
    page([say("Lise : Nilo ! Mon petit Nilo !"), say("Lise : Merci, merci… Je n'ai pas grand-chose, mais prenez tout ce que j'ai."),
          give("elixir", 3), give("gemme_soin"), gold(2500), qend('enfant_marais')], {"drapeau": ["quete:enfant_marais", "enfant_retrouve"]}),
    page([say("Lise : Nilo voulait voir les fleurs pâles du marais. Il est parti vers le sud, j'en suis sûre.")], {"drapeau": "quete:enfant_marais"}),
    page([say("Lise : Mon fils Nilo n'est pas rentré hier soir. Il parlait sans arrêt des fleurs pâles du Marais…"),
          ask("Chercher Nilo dans le Marais Pâle ?", [qstart('enfant_marais'), say("Lise : Merci ! Il a un bonnet rouge. Faites vite, la nuit, le marais est dangereux.")],
              [say("Lise : Je… je vais attendre encore un peu.")])])]}
EV['enfant_marais'] = {"actions": [
    say("Nilo : Au secours ! Ces bêtes ne veulent pas me laisser partir !"),
    {"action": "combat", "nom": "Les créatures du marais", "ennemis": [{"espece": "crapale", "niveau": 61}, {"espece": "vasepent", "niveau": 62}, {"espece": "crapale", "niveau": 61}],
     "renforts": [{"espece": "aubespectre", "niveau": 61}], "fuite": False, "capture": False,
     "victoire": [flag("enfant_retrouve"), say("Nilo : Merci ! Je rentre tout de suite chez maman, promis !")]}]}
EV['enfant_rentre'] = {"actions": [say("Nilo : Les fleurs pâles brillent la nuit, vous saviez ? Mais je n'y retourne plus. Pas tout seul.")]}
QS.append({"id": "enfant_marais", "nom": "L'enfant du marais", "lieu": "Havre-d'Aube",
           "etapes": [{"texte": "Retrouver Nilo, le fils de Lise, perdu dans le Marais Pâle."},
                      {"si": {"drapeau": "enfant_retrouve"}, "texte": "Nilo est rentré : prévenir Lise, à Havre-d'Aube."}],
           "fin_texte": "Nilo est rentré chez sa mère."})

# 3. Le spectre du gouffre (garde de Havre-d'Aube) : un boss de carte sous condition
npc('aube', 30, 25, 'garde', 'quete_spectre', 'bas')
boss('gouffre', 8, 17, 'aubespectre', 'spectre_gouffre', 'spectre_gouffre', {"drapeau": "quete:spectre"})
EV['quete_spectre'] = {"pages": [
    page([say("Garde : Les rondes sont calmes, depuis. Je dors enfin la nuit.")], done('spectre')),
    page([say("Garde : Le spectre est parti ? Pour de bon ? Alors la garde d'Aube vous doit une fière chandelle."),
          give("gemme_esprit"), gold(3500), qend('spectre')], {"drapeau": ["quete:spectre", "spectre_gouffre"]}),
    page([say("Garde : Le spectre rôde dans le Gouffre, dans la grande salle de l'ouest. Méfiez-vous : il frappe les esprits.")], {"drapeau": "quete:spectre"}),
    page([say("Garde : Chaque nuit, un spectre monte du Gouffre et hante nos rondes. Mes hommes n'osent plus sortir."),
          ask("Chasser le spectre du Gouffre ?", [qstart('spectre'), say("Garde : Vous êtes braves. Il se cache dans la grande salle de l'ouest du Gouffre.")],
              [say("Garde : Je comprends. Personne ne veut y aller.")])])]}
EV['spectre_gouffre'] = {"actions": [
    say("Une forme pâle se lève du sol. Le spectre pousse un cri qui glace le sang !"),
    {"action": "combat", "nom": "Le spectre du Gouffre", "ennemis": [
        {"espece": "aubespectre", "niveau": 64, "pv": 4, "boss": True, "bouclier": 8,
         "phases": [{"pv": 0.5, "message": "Le spectre se dédouble !", "renforts": [{"espece": "aubespectre", "niveau": 60}]}]},
        {"espece": "nacrelin", "niveau": 60}], "boss": True, "fuite": False, "capture": False,
     "victoire": [flag("spectre_gouffre"), say("Le spectre se dissout dans un soupir. Le Gouffre semble plus clair, tout à coup.")]}]}
QS.append({"id": "spectre", "nom": "Le spectre du Gouffre", "lieu": "Havre-d'Aube",
           "etapes": [{"texte": "Chasser le spectre qui hante le Gouffre d'Aube, dans la grande salle de l'ouest."},
                      {"si": {"drapeau": "spectre_gouffre"}, "texte": "Le spectre est vaincu : prévenir le garde de Havre-d'Aube."}],
           "fin_texte": "Le spectre du Gouffre ne hante plus les rondes."})

# 4. Les pages de l'orage (Ruines englouties) : trois pages d'un vieux livre, dans les Ruines, la Tour et la Grotte des Marées
ITEMS.append({"id": "page_orage", "nom": "Page de l'orage", "icone": "ic_livre",
              "description": "Une page arrachée, couverte d'une écriture ancienne. L'érudite des Ruines englouties en cherche.", "prix": 0, "important": True})
ITEMS.append({"id": "recueil_orages", "nom": "Recueil des orages", "icone": "ic_livre",
              "description": "Le vieux livre des gardiens, enfin complet. Ses pages crépitent sous les doigts.",
              "prix": 3200, "equipement": "accessoire", "bonus": {"magie": 12, "resistance": 6, "vitesse": 3}})
npc('ruines', 10, 26, 'docteur', 'quete_pages_orage', 'droite')
chest('ruines', 28, 8, 'page_orage')
chest('tour', 8, 15, 'page_orage')
chest('marees', 22, 26, 'page_orage')
EV['quete_pages_orage'] = {"pages": [
    page([say("Érudite : Le livre parle d'une île à l'est, où les gardiens sont nés. Je me demande si elle existe vraiment…")], done('pages_orage')),
    page([take("page_orage", 3), say("Érudite : Les trois pages ! Le livre est complet !"),
          say("Érudite : « Quand l'orage sera enfermé, la brume protégera le monde. » Alors c'était ça…"),
          say("Érudite : Gardez le livre. Vous en ferez meilleur usage que moi : sa magie protège encore."),
          give("recueil_orages"), gold(2500), qend('pages_orage')],
         {"drapeau": "quete:pages_orage", "objet_min": {"page_orage": 3}}),
    page([say("Érudite : Trois pages manquent : une ici, dans les Ruines, une dans la Tour de l'Orage, une dans la Grotte des Marées.")], {"drapeau": "quete:pages_orage"}),
    page([say("Érudite : J'étudie le livre des anciens gardiens. Mais trois pages ont été arrachées et emportées par la mer."),
          ask("Retrouver les trois pages ?", [qstart('pages_orage'), say("Érudite : Merci ! Cherchez dans les Ruines, dans la Tour de l'Orage et dans la Grotte des Marées.")],
              [say("Érudite : Tant pis. Le livre gardera ses secrets.")])])]}
QS.append({"id": "pages_orage", "nom": "Les pages de l'orage", "lieu": "Ruines englouties",
           "etapes": [{"texte": "Retrouver trois pages du livre des gardiens pour l'érudite des Ruines (Ruines, Tour de l'Orage, Grotte des Marées)."},
                      {"si": {"objet_min": {"page_orage": 3}}, "texte": "Vous avez les trois pages : les apporter à l'érudite."}],
           "fin_texte": "Le livre des gardiens est complet."})

# 5. La carapace de Corail (montrer un Coquillon et un Perlin à un enfant)
npc('corail', 12, 22, 'enfant', 'quete_carapace', 'droite')
EV['quete_carapace'] = {"pages": [
    page([say("Pépin : Quand je serai grand, j'aurai un Coquillon. Et un Perlin. Et un bateau !")], done('carapace')),
    page([say("Pépin : Waouh ! Un vrai Coquillon ! Et un Perlin ! Je peux toucher ? … Ça pique !"),
          say("Pépin : Tiens, c'est mon trésor. Je l'ai trouvé sur la plage. Il est pour toi."), give("gemme_rempart"), gold(1500), qend('carapace')],
         {"drapeau": "quete:carapace", "membre": ["coquillon", "perlin"]}),
    page([say("Pépin : Tu me montres un Coquillon et un Perlin ? Ensemble dans ton équipe, hein !")], {"drapeau": "quete:carapace"}),
    page([say("Pépin : Mon grand frère dit que les Coquillons et les Perlins, ça n'existe pas. Moi je sais que si !"),
          ask("Montrer un Coquillon et un Perlin à Pépin ?", [qstart('carapace'), say("Pépin : Youpi ! On en trouve dans les herbes de l'île et dans la mer, je crois.")],
              [say("Pépin : Personne ne me croit jamais…")])])]}
QS.append({"id": "carapace", "nom": "Coquillon et Perlin", "lieu": "Île de Corail",
           "etapes": [{"texte": "Avoir ensemble un Coquillon et un Perlin dans l'équipe, et les montrer à Pépin, sur l'île de Corail."}],
           "fin_texte": "Pépin a vu un Coquillon et un Perlin."})

# ---------------------------------------------------------------------------------------------- vérification et écriture
for mid, m in maps.items():
    added = set()
    for k in ('habitants', 'coffres'):
        for o in m[k]:
            added.add((o['x'], o['y']))
    for b in m['boss']:
        for dx in (0, 1):
            for dy in (0, 1):
                added.add((b['x'] + dx, b['y'] + dy))
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
k = [o['id'] for o in OBJ].index('collier_corail') + 1
for i2, o in enumerate(ITEMS):
    OBJ.insert(k + i2, o)
open(D + 'objets.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(OBJ) + '\n')
for mid, m in maps.items():
    print(mid, [(o['x'], o['y'], o.get('apparence', o.get('objet'))) for o in m['habitants'][-3:] + m['coffres'][-1:]], m['boss'][-1:] if mid == 'gouffre' else '')
print('5 quêtes écrites')
