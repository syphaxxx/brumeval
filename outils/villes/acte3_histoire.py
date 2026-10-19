"""Acte 3, deuxième partie : espèces d'Aube, objets, événements, conseils (après outils/villes/acte3.py).

    python outils/villes/acte3_histoire.py     # une seule fois (refuse si les espèces d'Aube existent déjà)
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R


def jload(rel):
    return json.load(open(D + rel, encoding='utf-8'))


def jsave(rel, j):
    open(D + rel, 'w', encoding='utf-8', newline='\n').write(carte.pretty(j) + '\n')


ESP = jload('especes.json')
if any(e['id'] == 'lueurine' for e in ESP):
    sys.exit("Les espèces d'Aube existent déjà.")


def esp(i, nom, types, base, forme, couleurs, apprend, talent, **k):
    e = {"id": i, "nom": nom, "sprite": i}
    if len(types) == 1:
        e["type"] = types[0]
    else:
        e["types"] = types
    e["base"] = dict(zip(["pv", "pm", "attaque", "defense", "magie", "resistance", "vitesse"], base))
    e.update({k2: v for k2, v in k.items() if k2 in ("esquive", "critique")})
    if "immunites" in k:
        e["immunites"] = k["immunites"]
    e["forme"] = forme
    e["couleurs"] = couleurs
    e["apprend"] = apprend
    e["talent"] = talent
    if "role" in k:
        e["role_combat"] = k["role"]
    if "taille" in k:
        e["taille"] = k["taille"]
    return e


NOUVELLES = [
    esp("lueurine", "Lueurine", ["lumiere", "vent"], [58, 32, 50, 50, 76, 66, 80], "insecte", ["#7fe0e8", "#f0ffff"],
        [[1, "lumiere"], [1, "rafale"], [1, "hate"], [1, "prisme"], [58, "lame_air"], [62, "nova"]], "vif", esquive=8),
    esp("loup_aube", "Loup d'aube", ["lumiere", "normal"], [76, 22, 82, 58, 40, 52, 74], "loup", ["#e8d8a0", "#fff8e0"],
        [[1, "mordant"], [1, "hurlement"], [1, "ruee"], [1, "eclair"], [60, "prisme"]], "intimidation", critique=10, role="chef",
        taille=1.15),
    esp("crapale", "Crapâle", ["poison", "eau"], [74, 28, 54, 64, 66, 62, 48], "grenouille", ["#a8d8b0", "#f0fff0"],
        [[1, "torrent"], [1, "acide"], [1, "nuage_toxique"], [1, "toxine"], [60, "trombe"]], "piquant"),
    esp("aubespectre", "Aubespectre", ["esprit", "lumiere"], [58, 40, 34, 50, 82, 74, 66], "fantome", ["#f0a0c0", "#fff0f8"],
        [[1, "onde"], [1, "cauchemar"], [1, "soins"], [1, "rayon"], [62, "hurlement_spectral"]], "furtif", role="soigneur"),
    esp("rocaube", "Rocaube", ["roche", "lumiere"], [80, 14, 76, 92, 36, 62, 30], "rocher", ["#c89098", "#ffe0e8"],
        [[1, "eboul"], [1, "avalanche"], [1, "carapace"], [1, "seisme"], [62, "prisme"]], "carapace", role="protecteur"),
    esp("vasepent", "Vasepent", ["poison", "plante"], [64, 22, 74, 54, 58, 52, 72], "serpent", ["#7a9a4a", "#e0f0a0"],
        [[1, "venin"], [1, "etreinte"], [1, "acide"], [1, "pollen"], [60, "toxine"]], "piquant"),
    esp("aubelle", "Aubelle", ["vent", "lumiere"], [60, 30, 66, 48, 62, 54, 86], "oiseau", ["#d08060", "#fff0d0"],
        [[1, "fleche_vent"], [1, "lame_air"], [1, "lumiere"], [1, "ouragan"]], "vif", esquive=6),
    esp("nacrelin", "Nacrelin", ["eau", "lumiere"], [72, 36, 70, 64, 74, 70, 68], "lezard", ["#b8d8f0", "#ffffff"],
        [[1, "vague"], [1, "rayon"], [1, "trombe"], [1, "prisme"], [64, "nova"]], "meditation"),
    esp("gardien_lueurs", "Gardien des Lueurs", ["plante", "lumiere"], [105, 44, 70, 72, 62, 64, 44], "boss", ["#3a9a88", "#d0fff0"],
        [[1, "etreinte"], [1, "prisme"], [1, "tempete"], [1, "rayon"]], "cuirasse", immunites=["sommeil"]),
    esp("reine_marais", "Reine du Marais", ["poison", "eau"], [110, 50, 60, 62, 74, 66, 52], "boss", ["#8a6ab8", "#f0e0ff"],
        [[1, "nuage_toxique"], [1, "trombe"], [1, "etreinte"], [1, "toxine"], [1, "pollen"]], "piquant", immunites=["sommeil"]),
    esp("brume_noire", "Brume Noire", ["ombre", "esprit"], [120, 70, 70, 66, 82, 74, 60], "boss", ["#5a1a2a", "#ff9aa8"],
        [[1, "eclipse"], [1, "cauchemar"], [1, "lame_ombre"], [1, "hurlement_spectral"], [1, "foudre_noire"]], "esprit_clair",
        immunites=["sommeil"]),
]
ESP += NOUVELLES
jsave('especes.json', ESP)

OBJ = jload('objets.json')
k = [o['id'] for o in OBJ].index('coeur_orage') + 1
for i, o in enumerate([
    {"id": "larme_aube", "nom": "Larme d'Aube", "icone": "ic_gemme_perle", "description": "Une larme de lumière, tiède comme un matin d'été. Elle ouvre la porte du Gouffre.", "prix": 0},
    {"id": "seve_lueurs", "nom": "Sève des Lueurs", "description": "Trophée : une goutte de sève qui brille dans le noir, donnée par le Gardien des Lueurs.", "prix": 900, "trophee": True},
    {"id": "couronne_nenuphar", "nom": "Couronne de nénuphar", "description": "Trophée : la couronne de la Reine du Marais, toujours fraîche.", "prix": 1100, "trophee": True},
    {"id": "coeur_aube", "nom": "Cœur d'Aube", "description": "Trophée : ce qui reste de la Brume Noire, une perle de lumière rose.", "prix": 2000, "trophee": True},
    {"id": "lame_aube", "nom": "Lame d'Aube", "icone": "ic_epee_5", "description": "L'épée du Premier Gardien. Elle brille comme le soleil qui se lève.", "prix": 5000, "equipement": "arme",
     "bonus": {"attaque": 19, "magie": 6, "critique": 8}},
]):
    OBJ.insert(k + i, o)
jsave('objets.json', OBJ)

R = jload('regles.json')
R['conseils'] += [
    {"drapeau": "gardien_lueurs", "boss": "gardien_lueurs", "lieu": "le nord du Bois des Lueurs", "gemme": "gemme_pyro",
     "texte": "À mi-vie, il appelle une Lueurine et se renforce contre la magie : abattez-la aussitôt."},
    {"drapeau": "reine_marais", "boss": "reine_marais", "lieu": "l'îlot du Marais Pâle", "gemme": "gemme_foudre",
     "texte": "Elle empoisonne toute l'équipe : gardez des Remèdes. À 30 % de PV, un Vasepent vient l'aider."},
    {"drapeau": "aube_fin", "boss": "brume_noire", "lieu": "le trône de la Citadelle des gardiens", "gemme": "gemme_maitre",
     "texte": "Trois phases, de plus en plus rapides ; un Aubespectre la soigne : visez-le. Ilya vous soigne juste avant."},
]
jsave('regles.json', R)


# ----------------------------------------------------------------------------------------------- événements
def say(t): return {"action": "dire", "texte": t}
def flag(f): return {"action": "drapeau", "nom": f}
def give(i, q=1): return {"action": "donner", "objet": i, "quantite": q}
def page(acts, si=None):
    p = {}
    if si:
        p["si"] = si
    p["actions"] = acts
    return p


EV = {
    "ysolde": {"pages": [
        page([say("Ysolde : Le ciel est dégagé ! Pour la première fois, on voit l'île d'Aube à l'horizon."),
              say("Ysolde : Ma barque vous attend au bout de la jetée de l'est. On dit que les premiers gardiens venaient de là-bas.")],
             {"drapeau": "boss5"}),
        page([say("Ysolde : Je suis navigatrice. Mais tant que l'orage gronde au nord, aucune barque ne passe au large."),
              say("Ysolde : Il paraît qu'au-delà de l'orage, il y a une île où le soleil se lève deux fois plus beau.")])]},
    "ysolde_aube": {"actions": [say("Ysolde : Quand vous voudrez rentrer, la jetée vous ramènera au Port d'Écume.")]},
    "aube_arrivee": {"actions": [
        flag("aube_vue"),
        say("Une île baignée de lumière rose. Des fleurs pâles partout, et au nord, des arbres qui brillent."),
        say("Sur la place du village, une jeune femme en robe bleue vous fait signe.")]},
    "ilya": {"pages": [
        page([say("Ilya : La brume est partie pour de bon. Les gardiens n'ont plus de prison à garder… seulement des créatures à protéger."),
              say("Ilya : Revenez quand vous voulez. Aube sera toujours votre île.")], {"drapeau": "aube_fin"}),
        page([say("Ilya : Vous avez la Larme d'Aube, et le Gardien vous laisse passer. Le Gouffre, au nord du bois, mène à la Citadelle."),
              say("Ilya : Je vous y attendrai, devant le trône.")], {"drapeau": ["gardien_lueurs", "reine_marais"]}),
        page([say("Ilya : Le Gardien des Lueurs garde le nord du bois. La Reine du Marais, à l'est, garde la Larme d'Aube."),
              say("Ilya : Il vous faudra les deux pour atteindre la Citadelle.")], {"drapeau": "ilya_parle"}),
        page([say("Ilya : Vous venez de l'autre côté de l'orage ? Alors c'est vous qui l'avez vaincu…"),
              say("Ilya : Je suis Ilya, la dernière des gardiens d'Aube. Mon peuple a enfermé l'orage sous la brume, il y a mille ans."),
              say("Ilya : Le premier d'entre nous, le Premier Gardien, s'est fondu dans la brume pour la garder fermée."),
              say("Ilya : Quand l'orage est tombé, la brume qui restait s'est rassemblée ici. Elle a pris une forme : la Brume Noire."),
              say("Ilya : Elle veut refermer la prison sur le monde entier. Cette fois, avec toutes les créatures dedans."),
              say("Lior : Alors on va l'arrêter. Comment atteindre la Citadelle ?"),
              say("Ilya : Par le Gouffre, au nord du Bois des Lueurs. Mais le Gardien des Lueurs chasse tout ce qui sent la brume… et vous en venez."),
              say("Ilya : Et la porte du Gouffre ne s'ouvre qu'avec la Larme d'Aube. La Reine du Marais la garde, à l'est du village."),
              flag("ilya_parle")])]},
    "aube_soin": {"actions": [{"action": "soigner"}, {"action": "reveil"},
                              say("Prêtresse : Que la lumière d'Aube vous rende vos forces. Voilà, tout le monde est en pleine forme !")]},
    "aube_boutique": {"actions": [{"action": "boutique", "objets": [
        "potion", "superpotion", "ether", "remede", "elixir", "plume", "lanterne_or", "lame_ecume", "baton_orage", "armure_ecaille",
        "robe_orage", "amulette_maree", "bottes_ailees", "gemme_vol", "gemme_briseur", "gemme_orage", "gemme_maitre"]}]},
    "aube_maison": {"actions": [say("La maison d'Ilya. Une carte de l'île est punaisée au mur : le Bois des Lueurs au nord, le Marais Pâle à l'est, et tout au nord, la Citadelle.")]},
    "lueurs_entree": {"actions": [
        flag("lueurs_vu"),
        say("Les arbres brillent d'une lumière douce. Des milliers de lucioles dansent entre les branches."),
        say("Au nord, un chant grave fait trembler les feuilles. Quelque chose d'immense veille sur le bois.")]},
    "gardien_lueurs": {"actions": [
        say("Un arbre immense, couvert de lucioles, ouvre deux yeux d'ambre."),
        say("Gardien des Lueurs : Vous sentez la brume… Les enfants de la brume ne passeront pas !"),
        {"action": "question", "texte": "Affronter le Gardien des Lueurs ?", "oui": [
            {"action": "combat", "ennemis": [
                {"espece": "lueurine", "niveau": 56},
                {"espece": "gardien_lueurs", "niveau": 60, "pv": 5, "boss": True, "bouclier": 9,
                 "phases": [{"pv": 0.5, "message": "Le Gardien appelle les lucioles du bois !", "bonus": {"resistance": 1},
                             "renforts": [{"espece": "lueurine", "niveau": 57}]}]},
                {"espece": "lueurine", "niveau": 56}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [flag("gardien_lueurs"), give("seve_lueurs"),
                          say("Gardien des Lueurs : … Vous ne venez pas de la brume. Vous l'avez combattue. Pardonnez-moi."),
                          say("Gardien des Lueurs : Allez. Le Gouffre est au nord. Que la lumière vous guide.")]}],
         "non": [say("Le Gardien referme les yeux, mais ses racines barrent toujours le chemin.")]}]},
    "reine_marais": {"actions": [
        say("Sur l'îlot, une fleur géante s'ouvre. En son cœur, une larme brille comme le soleil levant."),
        say("Reine du Marais : Ma larme ! Personne ne la touchera. Le marais vous avalera !"),
        {"action": "question", "texte": "Affronter la Reine du Marais ?", "oui": [
            {"action": "combat", "ennemis": [
                {"espece": "crapale", "niveau": 58},
                {"espece": "reine_marais", "niveau": 63, "pv": 5.5, "boss": True, "bouclier": 10,
                 "phases": [{"pv": 0.6, "message": "La Reine s'enfonce dans la vase : sa magie gonfle !", "bonus": {"magie": 1}},
                            {"pv": 0.3, "message": "La Reine appelle ses serpents !", "renforts": [{"espece": "vasepent", "niveau": 60}]}]},
                {"espece": "vasepent", "niveau": 58}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [flag("reine_marais"), give("larme_aube"), give("couronne_nenuphar"),
                          say("La Reine se referme en bouton. La Larme d'Aube roule à vos pieds, tiède comme un matin d'été.")]}],
         "non": [say("La Reine ferme ses pétales et vous observe.")]}]},
    "ilya_citadelle": {"pages": [
        page([say("Ilya : C'est fini… Je n'arrive pas à y croire. Rentrons au village.")], {"drapeau": "aube_fin"}),
        page([say("Ilya : La Brume Noire est là-haut, sur le trône du Premier Gardien. Elle se nourrit de peur."),
              say("Ilya : Laissez-moi vous aider."), {"action": "soigner"}, {"action": "reveil"},
              say("Ilya pose la main sur vos épaules. Une chaleur douce vous rend toutes vos forces.")])]},
    "brume_noire": {"actions": [
        say("Sur le trône de cristal, une silhouette de fumée noire se lève. Des milliers d'yeux s'ouvrent dans la brume."),
        say("Brume Noire : Vous avez brisé ma prison… Alors j'en bâtirai une nouvelle. Plus grande. Pour le monde entier."),
        say("Lior : Personne n'enfermera plus les créatures. Ni l'orage, ni la brume, ni toi !"),
        {"action": "question", "texte": "Affronter la Brume Noire ?", "oui": [
            {"action": "combat", "musique": "boss_final", "ennemis": [
                {"espece": "aubespectre", "niveau": 62},
                {"espece": "brume_noire", "niveau": 68, "pv": 6.5, "boss": True, "bouclier": 14,
                 "phases": [{"pv": 0.7, "message": "La brume s'épaissit : la Brume Noire appelle ses ombres !", "bonus": {"magie": 1},
                             "renforts": [{"espece": "aubespectre", "niveau": 63}]},
                            {"pv": 0.4, "message": "La Brume Noire dévore la lumière !", "bouclier": 14, "bonus": {"vitesse": 1}},
                            {"pv": 0.15, "message": "Dernier sursaut des ténèbres !", "bonus": {"attaque": 1, "magie": 1}}]},
                {"espece": "aubespectre", "niveau": 62}],
             "boss": True, "fuite": False, "capture": False,
             "victoire": [flag("aube_fin"), give("coeur_aube"), give("lame_aube"),
                          say("La Brume Noire se déchire comme un voile. Au cœur, une silhouette de lumière : un vieil homme en armure ancienne."),
                          say("Premier Gardien : Merci… Mille ans que j'étais prisonnier de ma propre prison."),
                          say("Premier Gardien : Le monde n'a plus besoin de brume pour se protéger. Il a besoin de gardiens. De vrais gardiens."),
                          say("Premier Gardien : Lior… tu n'es plus un apprenti. Prends mon épée. Elle a assez dormi."),
                          say("Le Premier Gardien sourit, puis se dissout dans la lumière du matin. Partout sur l'île, les créatures se mettent à chanter."),
                          {"action": "fin", "texte": "La dernière brume s'est levée sur l'île d'Aube. Les créatures sont libres, et les gardiens veillent sur elles. Fin de l'acte 3 : merci d'avoir joué !"}]}],
         "non": [say("La Brume Noire rit, d'un rire qui ressemble au vent dans les ruines.")]}]},
    "tom_aube": {"pages": [
        page([say("Tom : Je vous avais dit qu'on se croiserait au-delà de l'orage ! J'ai déjà vu trois créatures que personne ne connaît."),
              say("Tom : Il paraît qu'un Nacrelin vit dans le Gouffre. Je l'attraperai avant vous !")])]},
}

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
body = et.rstrip()[:-1].rstrip()
for k2, v in EV.items():
    key = '  ' + json.dumps(k2, ensure_ascii=False) + ': '
    body += ',' + nl + key + carte.pretty(v, 2, len(key.encode('utf-8'))).replace('\n', nl)
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(body + nl + '}' + nl)
print("acte 3 : espèces, objets, conseils et événements écrits")
