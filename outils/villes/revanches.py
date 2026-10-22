"""Revanches des dresseurs (étape « 20 heures », 2026-10-20) : une fois battu, un dresseur propose une revanche plus
tard dans l'histoire (les dresseurs des actes 1 et 2 après le Seigneur des Orages, ceux d'Aube après la fin de
l'acte 3), avec une équipe plus forte (créatures évoluées, un renfort de plus), de l'or et un élixir. On lui parle :
il demande « Accepter la revanche ? ». Drapeau `<drapeau du premier combat>_revanche`. Traductions anglaises
fabriquées d'après le titre traduit de chaque dresseur. Une seule fois (refuse si une revanche existe déjà).

    python outils/villes/revanches.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

D = carte.R
EV = json.load(open(D + 'evenements.json', encoding='utf-8'))
if any('_revanche' in json.dumps(v, ensure_ascii=False) for v in EV.values()):
    sys.exit('Les revanches existent déjà.')
SP = {s['id']: s for s in json.load(open(D + 'especes.json', encoding='utf-8'))}
EN = json.load(open(D + 'langues/en.json', encoding='utf-8'))
AUBE = ('route_lueurs', 'route_marais', 'route_gouffre', 'route_citadelle')
IDS = ['braconnier', 'bandit1', 'bandit2', 'chevalier1', 'chevalier2', 'gardien_brume1', 'gardien_brume2', 'dresseur_recif1', 'dresseur_recif2'] + \
      sorted(k for k in EV if k.startswith('route_'))

LINES = [  # (avant, question, défaite, après) : « Titre : » est ajouté devant
    ("Vous revoilà ! Je me suis entraîné depuis notre dernier combat. Cette fois, je ne perdrai pas.",
     "Back again! I've been training since our last battle. This time, I won't lose."),
    ("J'ai dit à tout le monde que vous m'aviez battu. Personne ne me croit. Une revanche, pour prouver que c'était de la chance ?",
     "I told everyone you beat me. Nobody believes me. A rematch, to prove it was luck?"),
    ("Mes créatures ont grandi, vous savez. Elles ont envie de vous montrer.",
     "My creatures have grown, you know. They want to show you."),
]
DEFEAT = [("Encore perdu… Vous êtes vraiment devenus des gardiens.", "Lost again… You really have become guardians."),
          ("Bon. Ce n'était pas de la chance.", "Fine. It wasn't luck."),
          ("Elles ont grandi, mais vous aussi.", "They've grown, but so have you.")]
AFTER = [("Je vais continuer à m'entraîner. Un jour, peut-être…", "I'll keep training. Someday, maybe…"),
         ("Je raconterai notre combat à mes petits-enfants. En changeant un peu la fin.", "I'll tell my grandchildren about our battle. With a slightly different ending."),
         ("Revenez quand vous voulez. Mais pas trop souvent.", "Come back whenever you like. But not too often.")]
ASK = ("Accepter la revanche ?", "Accept the rematch?")


def evolved(sp, lvl):
    s = SP[sp]
    while s.get('evolution') and s['evolution'].get('niveau', 999) <= lvl and s['evolution'].get('espece') in SP:
        s = SP[s['evolution']['espece']]
    return s['id']


def say(t): return {"action": "dire", "texte": t}


TR = {ASK[0]: ASK[1]}
out = []
for n, tid in enumerate(IDS):
    ev = EV[tid]
    first = ev['pages'][0]
    flag = first['si']['sans_drapeau']
    fight = next(a for a in first['actions'] if a['action'] == 'combat')
    intro = next(a['texte'] for a in first['actions'] if a['action'] == 'dire')
    titre = intro.split(' : ', 1)[0]
    lv0 = max(e['niveau'] for e in fight['ennemis'])
    aube = tid.startswith(AUBE)
    cond, lvl = ('aube_fin', 74 + max(0, lv0 - 59) // 2) if aube else ('boss5', 50 + lv0 // 8)
    team = [{"espece": evolved(e['espece'], lvl), "niveau": lvl + (1 if i == 0 else 0)} for i, e in enumerate(fight['ennemis'])]
    renf = [{"espece": evolved(e['espece'], lvl), "niveau": lvl} for e in fight.get('renforts', [])]
    renf.append({"espece": evolved(fight['ennemis'][-1]['espece'], lvl), "niveau": lvl})
    rev = flag + '_revanche'
    k = n % 3
    l_avant, l_def, l_apres = (titre + ' : ' + LINES[k][0], titre + ' : ' + DEFEAT[k][0], titre + ' : ' + AFTER[k][0])
    en_t = EN.get(titre) or titre
    TR[l_avant] = en_t + ': ' + LINES[k][1]
    TR[l_def] = en_t + ': ' + DEFEAT[k][1]
    TR[l_apres] = en_t + ': ' + AFTER[k][1]
    TR[rev] = rev
    p_done = {"si": {"drapeau": rev}, "actions": [say(l_apres)]}
    p_rev = {"si": {"drapeau": [flag, cond]}, "actions": [
        say(l_avant),
        {"action": "question", "texte": ASK[0], "oui": [
            {"action": "combat", "nom": fight.get('nom', titre), "ennemis": team, "renforts": renf, "fuite": False, "capture": False,
             "victoire": [{"action": "drapeau", "nom": rev}, say(l_def), {"action": "or", "quantite": 40 * lvl}, {"action": "donner", "objet": "elixir", "quantite": 1}]}],
         "non": []}]}
    ev['pages'] = ev['pages'][:1] + [p_done, p_rev] + ev['pages'][1:]
    out.append((tid, titre, cond, lvl, [e['espece'] for e in team + renf]))
    if not EN.get(titre):
        print('  (titre sans traduction : %s)' % titre)

et = open(D + 'evenements.json', encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in et else '\n'
lines = et.split(nl)
# réécrit seulement les entrées touchées, à leur place (le reste du fichier ne bouge pas)
for tid in IDS:
    key = '  ' + json.dumps(tid, ensure_ascii=False) + ': '
    i = next(j for j, l in enumerate(lines) if l.startswith(key))
    j = i
    depth = 0
    while True:  # fin de l'entrée : accolades équilibrées
        depth += lines[j].count('{') - lines[j].count('}')
        if depth == 0 and j >= i:
            break
        j += 1
    comma = lines[j].rstrip().endswith(',')
    txt = key + carte.pretty(EV[tid], 2, len(key.encode('utf-8'))) + (',' if comma else '')
    lines[i:j + 1] = txt.split('\n')
open(D + 'evenements.json', 'w', encoding='utf-8', newline='').write(nl.join(lines))
assert json.load(open(D + 'evenements.json', encoding='utf-8')) == EV
json.dump(TR, open(os.path.join(os.environ.get('TRAD', '.'), 'revanches_en.json'), 'w', encoding='utf-8'), ensure_ascii=False)
for o in out:
    print('%-20s %-26s après %-9s N.%d %s' % o)
print('%d revanches écrites' % len(out))
