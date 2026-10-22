"""Le quiz du conteur (étape « 20 heures », 2026-10-20) : après l'acte 4, un conteur de la Vallée pose dix questions
(oui ou non) sur l'aventure. Une bonne réponse : de l'or et la question suivante ; une mauvaise : il faudra revenir
(la question reste la même). Les dix : une récompense. Drapeaux `quiz1`… `quiz10`, quête `quiz`. Une seule fois (refuse
si la quête existe).

    python outils/villes/quiz.py
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
if any(q['id'] == 'quiz' for q in QT):
    sys.exit('Déjà fait.')

# (question, bonne réponse : True = oui)
QUESTIONS = [
    ("Sylvarque fut-il le premier gardien que vous avez apaisé ?", True),
    ("Ignarok dormait-il dans les Pics Givrés ?", False),
    ("Brann travaille-t-il à la forge de Forgeroc ?", True),
    ("Sélène est-elle née à Port d'Écume ?", False),
    ("Le Trident d'écume ouvre-t-il les Ruines englouties ?", True),
    ("Le Seigneur des Orages vous attendait-il au fond de la Grotte des Marées ?", False),
    ("Ilya est-elle la dernière gardienne d'Aube ?", True),
    ("La Larme d'Aube ouvre-t-elle la porte du Gouffre ?", True),
    ("Le Colosse d'écho gardait-il un écho de flamme ?", False),
    ("Kael traquait-il les bandits de la forêt de Sylve-Noire ?", True),
]


def say(t): return {"action": "dire", "texte": t}


N = len(QUESTIONS)
pages = [{"si": {"drapeau": "quiz%d" % N}, "actions": [
    say("Conteur : Dix sur dix ! Vous connaissez votre histoire mieux que moi. Je n'ai plus qu'à l'écrire.")]}]
for k in range(N, 0, -1):
    q, yes = QUESTIONS[k - 1]
    right = [{"action": "drapeau", "nom": "quiz%d" % k}, {"action": "or", "quantite": 300 * k}, say("Conteur : Exact !")]
    if k == N:
        right += [say("Conteur : Les dix questions ! Tenez : mon plus beau trésor, et mon respect éternel."),
                  {"action": "donner", "objet": "gemme_maitre", "quantite": 1}, {"action": "donner", "objet": "elixir", "quantite": 3},
                  {"action": "quete", "id": "quiz", "fin": True}]
    else:
        right.append(say("Conteur : Revenez me voir pour la question suivante."))
    wrong = [say("Conteur : Hé non ! Réfléchissez bien, et revenez quand vous voudrez.")]
    prev = "quiz%d" % (k - 1) if k > 1 else "quete:quiz"
    pages.append({"si": {"drapeau": prev}, "actions": [
        say("Conteur : Question %d sur %d." % (k, N)),
        {"action": "question", "texte": q, "oui": right if yes else wrong, "non": wrong if yes else right}]})
pages.append({"si": {"drapeau": "echos_fin"}, "actions": [
    say("Conteur : J'écris l'histoire des gardiens de Brumeval. Mais ma mémoire n'est plus ce qu'elle était…"),
    say("Conteur : Si vous répondez à mes dix questions, je vous récompenserai. Oui ou non, c'est tout ce que je demande."),
    {"action": "quete", "id": "quiz"},
    say("Conteur : Parlez-moi encore pour la première question.")]})
pages.append({"actions": [say("Conteur : J'écris l'histoire des gardiens. Revenez quand elle sera finie… si elle finit un jour.")]})
EV = {"conteur_quiz": {"pages": pages}}
Q = {"id": "quiz", "nom": "Le quiz du conteur", "lieu": "Vallée de Brumeval",
     "etapes": [{"texte": "Répondre aux dix questions du conteur de la Vallée sur votre aventure."}] +
     [{"si": {"drapeau": "quiz%d" % k}, "texte": "%d bonnes réponses sur %d." % (k, N)} for k in (3, 6, 9)],
     "fin_texte": "Le conteur écrit votre histoire."}

m = carte.load('vallee')
before = pu.reachable(m)
p = pu.near_reach(m, 26, 20, ring=1)
m['habitants'].append({"x": p[0], "y": p[1], "apparence": "ermite", "direction": "bas", "evenement": "conteur_quiz"})
lost = before - pu.reachable(m) - {tuple(p)}
if lost:
    sys.exit('vallee : des cases deviennent inaccessibles %s' % sorted(lost)[:5])
carte.save('vallee', m)
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
open(D + 'quetes.json', 'w', encoding='utf-8', newline='').write(qt.rstrip()[:-1].rstrip() + ',' + nl + json.dumps(Q, ensure_ascii=False) + nl + ']' + nl)
print('Conteur en', p)
