"""Quêtes des héros (étape « 20 heures », 2026-10-20) : après l'acte 3 (`aube_fin`), chacun des six héros a sa quête,
comme les quêtes de loyauté ou les armes ultimes des jeux du genre : quelqu'un de son passé la propose (le héros doit
être dans l'équipe), un adversaire à vaincre, et une arme ou une armure que lui seul peut porter (champ « pour » des
objets). Lior : l'ancien de la chapelle et le Brumeroi de la Source ; Maëlle : sœur Agathe et la Sentinelle d'aurore du
Temple gelé ; Brann : maître Hrolf (forgeron de Forgeroc) et le Golem de magma des Mines ; Isra : Nadja (Grotte des
Échos) et l'Ombre des galeries (Grotte des Marées) ; Kael : la garde-forestière de Sylve-Noire et le braconnier Varn ;
Sélène : le vieux chevalier Aldric (Givreval) et le Chevalier de glace du Glacier. Quatre boss recolorés
(outils/sprites/variantes.json : relancer outils/sprites/decoupe.py ensuite). Une seule fois (refuse si la quête
« lien_lior » existe). Vérifie que rien de déjà accessible ne devient inaccessible.

    python outils/villes/liens.py
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
if any(q['id'] == 'lien_lior' for q in QT):
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
    entry = {"espece": sp, "drapeau": fl, "evenement": ev, "si": si}
    if not best:  # couloirs étroits (labyrinthe) : un cul-de-sac où il ne bouche rien
        for (xx, yy) in sorted(ok, key=lambda c: abs(c[0] - x) + abs(c[1] - y)):
            cells = [(xx + dx, yy + dy) for dx in (0, 1) for dy in (0, 1)]
            if any(c not in ok or c in occ for c in cells):
                continue
            m['boss'].append(dict(entry, x=xx, y=yy))
            if not (before[mid] - pu.reachable(m) - set(cells)):
                return xx, yy
            m['boss'].pop()
    assert best, (mid, x, y)
    m['boss'].append(dict(entry, x=best[1], y=best[2]))
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


def offer(hero): return {"drapeau": "aube_fin", "membre": [hero]}


def fight(name, foes, win, music=None):
    c = {"action": "combat", "nom": name, "ennemis": foes, "boss": True, "fuite": False, "capture": False, "victoire": win}
    if music:
        c["musique"] = music
    return c


def chief(sp, lv, msg, adds, pv=5.5):
    return {"espece": sp, "niveau": lv, "pv": pv, "boss": True, "bouclier": 8,
            "phases": [{"pv": 0.5, "message": msg, "renforts": adds}]}


EV, QS, ITEMS, SPECIES, VARIANTS = {}, [], [], [], []


def quest(qid, name, place, steps, end):
    QS.append({"id": qid, "nom": name, "lieu": place, "etapes": steps, "fin_texte": end})


# ------------------------------------------------------------------------------------------------ Lior : la première lanterne
ITEMS.append({"id": "lanterne_lame", "nom": "Lanterne-lame", "icone": "ic_epee_5", "prix": 12000, "equipement": "arme", "pour": "lior",
              "description": "L'épée de l'ancien, quand il était gardien. Une petite flamme brûle dans sa garde, et ne s'éteint jamais.",
              "bonus": {"attaque": 30, "magie": 10, "vitesse": 3, "critique": 8}})
SPECIES.append({"id": "brumeroi", "nom": "Brumeroi", "description": "Une bête de brume qui dort à la Source depuis quarante ans, une vieille épée plantée dans le dos.",
                "sprite": "brumeroi", "types": ["ombre", "esprit"],
                "base": {"pv": 118, "pm": 60, "attaque": 74, "defense": 70, "magie": 80, "resistance": 72, "vitesse": 58},
                "immunites": ["sommeil"], "forme": "boss", "couleurs": ["#7a8aa8", "#e8f0ff"],
                "apprend": [[1, "cauchemar"], [1, "lame_ombre"], [1, "voile"], [1, "suie"], [1, "regard"]], "talent": "esprit_clair"})
VARIANTS.append({"id": "brumeroi", "de": "mere_brumes", "teintes": [[-1, 360, 215, 0.55, 1.2, 0.25]]})
boss('source', 30, 24, 'brumeroi', 'lien_lior_vaincu', 'brumeroi', {"drapeau": "quete:lien_lior"})
EV['brumeroi'] = {"actions": [
    say("La brume se rassemble en une bête immense. Dans son dos, une vieille épée brille d'une petite flamme."),
    say("Lior : L'épée du maître… Elle brûle encore."),
    fight("Le Brumeroi", [chief("brumeroi", 88, "Le Brumeroi rugit et la brume s'épaissit !", [{"espece": "nebulis", "niveau": 84}, {"espece": "orbrume", "niveau": 84}]),
                         {"espece": "nebulis", "niveau": 84}],
          [flag("lien_lior_vaincu"), say("Le Brumeroi se dissout. La vieille épée tombe dans l'herbe, toujours allumée."),
           say("Lior : Je la rapporte au maître. Elle est à lui.")])]}
HEAL = [{"action": "soigner", "pv": False}, say("Une douce lumière emplit la chapelle. Les PM de toute l'équipe sont restaurés.")]
EV['chapelle'] = {"pages": [
    page(HEAL, done('lien_lior')),
    page(HEAL + [say("Ancien : Ma Lanterne… Tu l'as reprise au Brumeroi ? Toi, mon apprenti ?"),
                 say("Ancien : Quarante ans que je n'ose plus aller à la Source. Et toi, tu y vas et tu reviens avec le sourire."),
                 say("Ancien : Garde-la. Une épée de gardien doit servir à un gardien. Et le gardien, maintenant, c'est toi."),
                 say("Lior : Merci, maître. Je ne la laisserai jamais s'éteindre."),
                 give("lanterne_lame"), qend('lien_lior')], {"drapeau": ["quete:lien_lior", "lien_lior_vaincu"]}),
    page(HEAL + [say("Ancien : Le Brumeroi dort au sud-est de la Source des Brumes. Ne le sous-estime pas, Lior.")], {"drapeau": "quete:lien_lior"}),
    page(HEAL + [say("Ancien : Lior. Te voilà revenu de l'île d'Aube… Tu as grandi, mon garçon."),
                 say("Ancien : Il faut que je te dise. Avant d'être vieux, j'étais gardien, moi aussi. Mon épée s'appelait la Lanterne."),
                 say("Ancien : Je l'ai perdue à la Source des Brumes, plantée dans le dos d'une bête que je n'ai jamais pu vaincre : le Brumeroi."),
                 say("Ancien : Il dormait depuis quarante ans. La Brume Noire l'a réveillé. Je le sens, d'ici."),
                 ask("Reprendre la Lanterne au Brumeroi ?", [qstart('lien_lior'), say("Lior : J'y vais, maître. Vous me l'avez dit cent fois : un gardien finit ce qu'on a commencé.")],
                     [say("Ancien : Prends ton temps. Le Brumeroi attendra. Moi aussi.")])], offer('lior')),
    page(HEAL)]}
quest('lien_lior', "La première lanterne", "Vallée de Brumeval",
      [{"texte": "Vaincre le Brumeroi, au sud-est de la Source des Brumes, et reprendre l'épée de l'ancien."},
       {"si": {"drapeau": "lien_lior_vaincu"}, "texte": "Rapporter la Lanterne à l'ancien, dans la chapelle de la Vallée."}],
      "Lior porte la Lanterne de son maître.")

# ---------------------------------------------------------------------------------------------- Maëlle : le bâton d'Aurore
ITEMS.append({"id": "baton_aurore", "nom": "Bâton d'Aurore", "icone": "ic_baton_5", "prix": 12000, "equipement": "arme", "pour": "maelle",
              "description": "Le bâton de la première mage blanche de la vallée. Sa pierre rose s'allume quand quelqu'un souffre, tout près.",
              "bonus": {"magie": 30, "pm": 30, "resistance": 8, "vitesse": 2}})
SPECIES.append({"id": "sentinelle_aurore", "nom": "Sentinelle d'aurore", "description": "Elle garde le bâton d'Aurore au fond du Temple gelé, et ne laisse passer que les cœurs purs.",
                "sprite": "sentinelle_aurore", "types": ["lumiere", "glace"],
                "base": {"pv": 112, "pm": 60, "attaque": 70, "defense": 76, "magie": 78, "resistance": 76, "vitesse": 50},
                "immunites": ["sommeil"], "forme": "boss", "couleurs": ["#e8a0c0", "#fff0f8"],
                "apprend": [[1, "prisme"], [1, "glaciation"], [1, "corne_givre"], [1, "nova"], [1, "rempart"]], "talent": "cuirasse"})
VARIANTS.append({"id": "sentinelle_aurore", "de": "gardien_lueurs", "teintes": [[-1, 360, 330, 0.7, 1.15]]})
npc('vallee_chapelle', 7, 4, 'villageoise', 'soeur_agathe', 'gauche', si={"drapeau": "aube_fin"})
boss('temple', 6, 24, 'sentinelle_aurore', 'lien_maelle_vaincu', 'sentinelle_aurore', {"drapeau": "quete:lien_maelle"})
EV['sentinelle_aurore'] = {"actions": [
    say("Une statue de glace rose s'anime. Un bâton de cristal flotte devant elle."),
    say("Maëlle : Le bâton d'Aurore… Elle le garde depuis tout ce temps ?"),
    fight("La Sentinelle d'aurore", [chief("sentinelle_aurore", 88, "La Sentinelle se couvre de givre !", [{"espece": "cristallin", "niveau": 84}]),
                                    {"espece": "givrelin", "niveau": 84}, {"espece": "cristallin", "niveau": 84}],
          [flag("lien_maelle_vaincu"), say("La Sentinelle s'incline et tend le bâton à Maëlle avant de fondre en pluie de lumière.")])]}
EV['soeur_agathe'] = {"pages": [
    page([say("Sœur Agathe : Aurore serait fière de toi, Maëlle. Moi, je le suis déjà.")], done('lien_maelle')),
    page([say("Sœur Agathe : Le bâton d'Aurore ! Il s'est allumé dès que tu es entrée… Il t'a choisie, Maëlle."),
          say("Maëlle : Je ne suis pas sûre de le mériter, ma sœur."),
          say("Sœur Agathe : C'est justement pour ça que tu le mérites. Aurore disait la même chose."),
          give("baton_aurore"), gold(5000), qend('lien_maelle')], {"drapeau": ["quete:lien_maelle", "lien_maelle_vaincu"]}),
    page([say("Sœur Agathe : Le bâton est au fond du Temple gelé, dans la salle de l'ouest. Que la lumière te garde.")], {"drapeau": "quete:lien_maelle"}),
    page([say("Sœur Agathe : Maëlle ! Je rentre de mon pèlerinage. Tu as tellement changé…"),
          say("Sœur Agathe : Tu connais l'histoire d'Aurore, la première mage blanche de la vallée ? Elle a caché son bâton au Temple gelé, avant de mourir."),
          say("Sœur Agathe : Une sentinelle le garde, et ne le donne qu'à une mage digne d'elle. J'ai essayé, à ton âge. Elle ne m'a pas trouvée digne."),
          ask("Chercher le bâton d'Aurore au Temple gelé ?", [qstart('lien_maelle'), say("Maëlle : J'y vais. Si elle refuse, au moins, j'aurai essayé.")],
              [say("Sœur Agathe : Le bâton a attendu cent ans. Il peut attendre encore un peu.")])], offer('maelle')),
    page([say("Sœur Agathe : Je prie pour vous tous, chaque soir. Même pour Brann.")])]}
quest('lien_maelle', "Le bâton d'Aurore", "Vallée de Brumeval",
      [{"texte": "Trouver le bâton d'Aurore au fond du Temple gelé, dans la salle de l'ouest, et vaincre sa sentinelle."},
       {"si": {"drapeau": "lien_maelle_vaincu"}, "texte": "Montrer le bâton à sœur Agathe, dans la chapelle de la Vallée."}],
      "Maëlle porte le bâton d'Aurore.")

# --------------------------------------------------------------------------------------------- Brann : le dernier marteau
ITEMS.append({"id": "coeur_magma", "nom": "Cœur de magma", "icone": "ic_gemme_feu",
              "description": "Une pierre rouge, brûlante, qui bat encore comme un cœur. Maître Hrolf en a besoin.", "prix": 0, "important": True})
ITEMS.append({"id": "hache_hrolf", "nom": "Hache de Hrolf", "icone": "ic_hache_4", "prix": 12000, "equipement": "arme", "pour": "brann",
              "description": "La dernière hache de maître Hrolf, forgée avec un cœur de magma. Elle chauffe quand Brann se met en colère.",
              "bonus": {"attaque": 34, "defense": 6, "critique": 8}})
SPECIES.append({"id": "golem_magma", "nom": "Golem de magma", "description": "Il est né au fond des mines, là où la roche fond. Son cœur bat comme une forge.",
                "sprite": "golem_magma", "types": ["feu", "roche"],
                "base": {"pv": 124, "pm": 40, "attaque": 82, "defense": 84, "magie": 60, "resistance": 62, "vitesse": 40},
                "immunites": ["sommeil"], "forme": "boss", "couleurs": ["#c04020", "#ffd080"],
                "apprend": [[1, "poing"], [1, "brasier"], [1, "avalanche"], [1, "eboul"], [1, "carapace"]], "talent": "rage"})
VARIANTS.append({"id": "golem_magma", "de": "golem_feu_grand", "teintes": [[-1, 360, 8, 1.25, 1.1]]})
fr = [h for h in M('cendrelune')['habitants'] if h.get('apparence') == 'forgeron' and 'evenement' not in h]
assert fr, 'forgeron de Forgeroc'
fr[0]['evenement'] = 'maitre_hrolf'
boss('mines', 30, 26, 'golem_magma', 'lien_brann_vaincu', 'golem_magma', {"drapeau": "quete:lien_brann"})
EV['golem_magma'] = {"actions": [
    say("La roche se fend. Un golem de lave se lève, la poitrine ouverte sur un cœur rouge qui bat."),
    say("Brann : Le voilà, ton cœur de magma, maître. Il ne va pas se laisser prendre gentiment."),
    fight("Le Golem de magma", [chief("golem_magma", 89, "Le Golem de magma déborde de lave !", [{"espece": "fumerole", "niveau": 84}]),
                               {"espece": "galetor", "niveau": 84}, {"espece": "fumerole", "niveau": 84}],
          [flag("lien_brann_vaincu"), give("coeur_magma"), say("Le golem s'effondre en pierres refroidies. Au milieu, le cœur de magma bat encore.")])]}
EV['maitre_hrolf'] = {"pages": [
    page([say("Maître Hrolf : Elle te va bien, cette hache. Mieux qu'à moi. Va, et reviens me raconter.")], done('lien_brann')),
    page([take("coeur_magma"), say("Maître Hrolf : Le cœur de magma ! Donne. Et recule-toi, gamin."),
          say("Le vieux forgeron frappe toute la nuit. Au matin, une hache rouge sombre repose sur l'enclume."),
          say("Maître Hrolf : Ma dernière. Après celle-là, je range le marteau. Prends-en soin, Brann."),
          say("Brann : … Merci, maître. Je ne sais pas quoi dire."),
          say("Maître Hrolf : Alors ne dis rien. C'est comme ça qu'on fait, à Forgeroc."),
          give("hache_hrolf"), qend('lien_brann')], {"drapeau": "quete:lien_brann", "objet": "coeur_magma"}),
    page([say("Maître Hrolf : Le golem de magma dort au fond des Mines, à l'est. Rapporte-moi son cœur, et je forge.")], {"drapeau": "quete:lien_brann"}),
    page([say("Maître Hrolf : Brann. Ta hache, c'est moi qui te l'ai forgée quand tu avais quinze ans. Regarde-la : elle a fait son temps."),
          say("Maître Hrolf : Je veux forger une dernière hache avant de ranger le marteau. Une vraie. Mais il me faut un cœur de magma."),
          say("Maître Hrolf : Il y a un golem de lave au fond des Mines. Son cœur bat comme une forge. C'est celui-là qu'il me faut."),
          ask("Rapporter le cœur du golem de magma ?", [qstart('lien_brann'), say("Brann : Un golem de lave ? Ça me va. J'avais froid.")],
              [say("Maître Hrolf : Pas pressé. Le feu de ma forge ne s'éteint pas.")])], offer('brann')),
    page([say("Maître Hrolf : J'ai appris le métier à Brann. Enfin, j'ai essayé. Il tape fort, mais il tape partout.")])]}
quest('lien_brann', "Le dernier marteau", "Forgeroc",
      [{"texte": "Vaincre le golem de magma au fond des Mines de Forgeroc et rapporter son cœur à maître Hrolf."},
       {"si": {"objet": "coeur_magma"}, "texte": "Apporter le cœur de magma à maître Hrolf, à Forgeroc."}],
      "Maître Hrolf a forgé sa dernière hache pour Brann.")

# ------------------------------------------------------------------------------------------- Isra : le grimoire des profondeurs
ITEMS.append({"id": "grimoire_profondeurs", "nom": "Grimoire des profondeurs", "icone": "ic_livre", "prix": 12000, "equipement": "arme", "pour": "isra",
              "description": "Le livre du maître d'Isra, écrit à la lueur d'une bougie au fond des grottes. Les sorts y attendent dans le noir.",
              "bonus": {"magie": 32, "pm": 24, "vitesse": 4}})
SPECIES.append({"id": "ombre_galeries", "nom": "Ombre des galeries", "description": "Elle vit là où aucune lumière n'arrive, et elle a lu le grimoire avant tout le monde.",
                "sprite": "ombre_galeries", "types": ["ombre", "eau"],
                "base": {"pv": 112, "pm": 70, "attaque": 64, "defense": 66, "magie": 86, "resistance": 74, "vitesse": 62},
                "immunites": ["sommeil"], "forme": "boss", "couleurs": ["#3a4a8a", "#c0d0ff"],
                "apprend": [[1, "foudre_noire"], [1, "trombe"], [1, "cauchemar"], [1, "voile"], [1, "suie"]], "talent": "esprit_clair"})
VARIANTS.append({"id": "ombre_galeries", "de": "leviathan", "teintes": [[-1, 360, 275, 0.9, 0.85, 0.3]]})
npc('grotte', 15, 19, 'voyageuse', 'nadja', 'haut', si={"drapeau": "aube_fin"})
boss('marees', 10, 10, 'ombre_galeries', 'lien_isra_vaincu', 'ombre_galeries', {"drapeau": "quete:lien_isra"})
EV['ombre_galeries'] = {"actions": [
    say("Au fond de la galerie, l'eau devient noire. Une forme se lève, un grand livre ouvert entre ses mains d'ombre."),
    say("Isra : Le grimoire du maître. Rends-le. Ce n'est pas à toi qu'il l'a écrit."),
    fight("L'Ombre des galeries", [chief("ombre_galeries", 88, "L'Ombre lit une page du grimoire !", [{"espece": "murenix", "niveau": 84}]),
                                  {"espece": "bastillon", "niveau": 84}, {"espece": "murenix", "niveau": 84}],
          [flag("lien_isra_vaincu"), say("L'Ombre se disperse comme de l'encre dans l'eau. Le grimoire tombe aux pieds d'Isra.")])]}
EV['nadja'] = {"pages": [
    page([say("Nadja : Le maître serait content. Il disait toujours que tu étais la meilleure de nous deux. Il avait raison, l'idiot.")], done('lien_isra')),
    page([say("Nadja : Le grimoire ! Tu l'as retrouvé ! Fais voir… C'est bien son écriture, toute penchée."),
          say("Nadja : Garde-le, Isra. Il l'avait écrit pour toi, tu sais. Moi, je n'ai jamais su lire dans le noir."),
          say("Isra : … Merci, Nadja. On ira boire un verre à sa santé. Un jour."),
          give("grimoire_profondeurs"), gold(5000), qend('lien_isra')], {"drapeau": ["quete:lien_isra", "lien_isra_vaincu"]}),
    page([say("Nadja : La Grotte des Marées, sur l'île de Corail. Le grimoire est au fond d'une galerie inondée, au nord-ouest.")], {"drapeau": "quete:lien_isra"}),
    page([say("Nadja : Isra ? C'est bien toi ? Ça fait des années ! On explorait les grottes ensemble, avec le vieux maître…"),
          say("Isra : Nadja. Tu es toujours là, à cartographier des trous ?"),
          say("Nadja : Toujours. Écoute : j'ai retrouvé la trace du grimoire du maître. Une ombre l'a emporté au fond de la Grotte des Marées."),
          ask("Reprendre le grimoire à l'ombre ?", [qstart('lien_isra'), say("Isra : Une ombre qui vole des livres. J'aurai tout vu. On y va.")],
              [say("Nadja : Bon. Il ne va pas s'envoler, ce grimoire. Enfin, j'espère.")])], offer('isra')),
    page([say("Nadja : Cette grotte n'a plus de secrets pour moi. Enfin, presque plus.")])]}
quest('lien_isra', "Le grimoire des profondeurs", "Grotte des Échos",
      [{"texte": "Reprendre le grimoire du maître d'Isra à l'ombre qui hante la Grotte des Marées (île de Corail)."},
       {"si": {"drapeau": "lien_isra_vaincu"}, "texte": "Montrer le grimoire à Nadja, dans la Grotte des Échos."}],
      "Isra garde le grimoire de son maître.")

# ------------------------------------------------------------------------------------------------ Kael : le dernier braconnier
ITEMS.append({"id": "arc_sylve", "nom": "Arc de Sylve", "icone": "ic_arc_4", "prix": 12000, "equipement": "arme", "pour": "kael",
              "description": "Taillé dans le vieux chêne de Sylve-Noire, qui n'avait jamais laissé personne le toucher. Il a laissé Kael.",
              "bonus": {"attaque": 30, "vitesse": 8, "precision": 10, "critique": 6}})
npc('sylvenoire', 40, 30, 'voyageuse', 'garde_foret', 'gauche', si={"drapeau": "aube_fin"})
npc('sylvenoire', 46, 44, 'chef_bandit', 'varn', 'gauche', si={"drapeau": "quete:lien_kael"}, cache="lien_kael_vaincu")
EV['varn'] = {"actions": [
    say("Varn : Kael ! L'archer qui a fait tomber mon frère. Tu tombes bien : j'ai de nouvelles cages à remplir."),
    say("Kael : Ton frère capturait des créatures pour les vendre. Toi aussi. Ça s'arrête ici."),
    fight("Varn le braconnier", [chief("chef_bandit", 89, "Varn siffle : « À moi, les gars ! »", [{"espece": "bandit", "niveau": 84}, {"espece": "braconnier", "niveau": 84}], 9),
                                {"espece": "braconnier", "niveau": 84}, {"espece": "crapoison", "niveau": 84}, {"espece": "bandit", "niveau": 84}],
          [flag("lien_kael_vaincu"), say("Varn : D'accord, d'accord ! Je pars ! Et je ne reviens pas !"),
           say("Kael : Les cages sont vides. Les créatures retournent dans la forêt.")])]}
EV['garde_foret'] = {"pages": [
    page([say("Garde-forestière : Les créatures reviennent près du camp. Elles n'ont plus peur. Grâce à toi, Kael.")], done('lien_kael')),
    page([say("Garde-forestière : Varn est parti ? Et toutes les cages ouvertes ? Kael, tu es incroyable."),
          say("Garde-forestière : Le vieux chêne t'a laissé une branche. Je l'ai taillée en arc. Il n'avait jamais laissé personne le toucher."),
          say("Kael : … Il m'a laissé, moi ? Alors je ferai attention à lui."),
          give("arc_sylve"), gold(5000), qend('lien_kael')], {"drapeau": ["quete:lien_kael", "lien_kael_vaincu"]}),
    page([say("Garde-forestière : Varn a installé son camp au sud-est de la forêt, près des cages. Sois prudent, Kael.")], {"drapeau": "quete:lien_kael"}),
    page([say("Garde-forestière : Kael ! Enfin. Les braconniers sont revenus. Leur nouveau chef s'appelle Varn : le frère de celui que tu as chassé."),
          say("Kael : Varn… Je m'en doutais. Il ne pardonne jamais rien."),
          say("Garde-forestière : Il remplit des cages de créatures, au sud-est. Seul, je ne peux rien faire."),
          ask("Chasser Varn de Sylve-Noire ?", [qstart('lien_kael'), say("Kael : Cette forêt m'a vu naître. Personne ne la vide de ses créatures.")],
              [say("Garde-forestière : Je comprends. Mais ne tarde pas trop.")])], offer('kael')),
    page([say("Garde-forestière : Je garde la forêt. Les arbres, les bêtes, et même les promeneurs perdus.")])]}
quest('lien_kael', "Le dernier braconnier", "Sylve-Noire",
      [{"texte": "Chasser Varn le braconnier, installé au sud-est de Sylve-Noire, et ouvrir ses cages."},
       {"si": {"drapeau": "lien_kael_vaincu"}, "texte": "Prévenir la garde-forestière de Sylve-Noire."}],
      "Les braconniers ont quitté Sylve-Noire pour de bon.")

# ------------------------------------------------------------------------------------------------ Sélène : le serment du père
ITEMS.append({"id": "armure_serment", "nom": "Armure du serment", "icone": "ic_armure_8", "prix": 12000, "equipement": "armure", "pour": "selene",
              "description": "L'armure du père de Sélène. Un serment est gravé à l'intérieur : « Garder ceux qui ont froid. »",
              "bonus": {"pv": 80, "defense": 28, "resistance": 16}})
npc('pics', 44, 36, 'chevalier', 'aldric', 'gauche', si={"drapeau": "aube_fin"})
npc('glacier', 30, 8, 'chevalier', 'chevalier_glace', 'bas', si={"drapeau": "quete:lien_selene"}, cache="lien_selene_vaincu")
EV['chevalier_glace'] = {"actions": [
    say("Un chevalier se tient immobile, couvert de givre. Une armure ancienne est posée à ses pieds."),
    say("Chevalier de glace : Nul ne touchera l'armure du serment. Je l'ai juré à son maître."),
    say("Sélène : Son maître était mon père. Je viens la reprendre."),
    fight("Le Chevalier de glace", [chief("chevalier", 89, "Le Chevalier de glace lève son bouclier !", [{"espece": "mage_givre", "niveau": 84}], 9),
                                   {"espece": "mage_givre", "niveau": 84}, {"espece": "cristallin", "niveau": 84}],
          [flag("lien_selene_vaincu"), say("Chevalier de glace : Tu frappes comme lui… Prends-la. Mon serment est tenu."),
           say("Le givre fond sur le chevalier. Il s'incline, puis disparaît dans un souffle froid.")])]}
EV['aldric'] = {"pages": [
    page([say("Aldric : Ton père serait fier, Sélène. Non : il EST fier. J'en suis sûr.")], done('lien_selene')),
    page([say("Aldric : L'armure de Théodric… Je la reconnaîtrais entre mille. Elle est à toi, Sélène. Elle l'a toujours été."),
          say("Sélène : « Garder ceux qui ont froid. » C'est ce qu'il me disait le soir."),
          say("Aldric : C'était son serment. Maintenant, c'est le tien."),
          give("armure_serment"), gold(5000), qend('lien_selene')], {"drapeau": ["quete:lien_selene", "lien_selene_vaincu"]}),
    page([say("Aldric : Le Glacier, à l'est des Pics. Le chevalier de glace garde l'armure dans la grande salle du nord.")], {"drapeau": "quete:lien_selene"}),
    page([say("Aldric : Sélène. J'ai servi avec ton père, Théodric. Il ne t'a jamais parlé de son armure ?"),
          say("Sélène : Il disait l'avoir laissée « là où elle servait encore ». Je n'ai jamais compris."),
          say("Aldric : Au fond du Glacier. Il y a laissé un chevalier de glace pour la garder, en attendant que quelqu'un soit digne de la porter."),
          ask("Reprendre l'armure du père de Sélène ?", [qstart('lien_selene'), say("Sélène : Alors je vais lui montrer que j'en suis digne.")],
              [say("Aldric : Le Glacier ne fond pas. L'armure t'attendra.")])], offer('selene')),
    page([say("Aldric : Les Pics sont plus doux, depuis que le Givrecorne dort. On en profite, nous, les vieux.")])]}
quest('lien_selene', "Le serment du père", "Givreval",
      [{"texte": "Reprendre l'armure du père de Sélène au chevalier de glace, au nord du Glacier de Givreval."},
       {"si": {"drapeau": "lien_selene_vaincu"}, "texte": "Retrouver Aldric, à Givreval."}],
      "Sélène porte l'armure du serment de son père.")

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
ALL = json.load(open(D + 'evenements.json', encoding='utf-8'))
ALL['chapelle'] = EV.pop('chapelle')
carte.rewrite_events(['chapelle'], ALL)
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
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
ESP += SPECIES
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
V = os.path.join(os.path.dirname(ICI), 'sprites', 'variantes.json')
vt = open(V, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in vt else '\n'
vt = vt.rstrip()[:-1].rstrip()
for v in VARIANTS:
    vt += ',' + nl + '  ' + json.dumps(v, ensure_ascii=False)
open(V, 'w', encoding='utf-8', newline='').write(vt + nl + ']' + nl)
for mid, m in maps.items():
    print(mid, [(o['x'], o['y'], o.get('apparence')) for o in m['habitants'][-2:]], m['boss'][-1:])
print('6 quêtes de héros écrites')
