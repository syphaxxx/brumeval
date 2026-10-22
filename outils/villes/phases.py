"""Une phase de plus pour des combats qui n'en avaient pas (étape « 20 heures », 2026-10-20) : les neuf épreuves des
gardiens (à mi-vie, le gardien rassemble ses forces), le troll des Mines (des pierres tombent, un renfort) et la bande
de Rongecrins (le gros appelle la bande). Une seule fois (refuse si le troll a déjà une phase).

    python outils/villes/phases.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

EV = json.load(open(carte.R + 'evenements.json', encoding='utf-8'))


def bosses(x):
    if isinstance(x, dict):
        if x.get('action') == 'combat':
            for e in x.get('ennemis', []):
                if e.get('boss'):
                    yield e
        for v in x.values():
            yield from bosses(v)
    elif isinstance(x, list):
        for v in x:
            yield from bosses(v)


troll = next(bosses(EV['mines_boss']))
if troll.get('phases'):
    sys.exit('Déjà fait.')
troll['phases'] = [{"pv": 0.5, "message": "Le troll arrache un étai : des pierres dégringolent, et un Rocaillou roule dans la mêlée !",
                    "bonus": {"attaque": 1}, "renforts": [{"espece": "rocaillou", "niveau": 19}]}]
gros = next(bosses(EV['rongecrins']))
gros['phases'] = [{"pv": 0.5, "message": "Le gros Rongecrin siffle entre ses dents : la bande accourt !",
                   "renforts": [{"espece": "rongecrin", "niveau": 11}]}]
n = 0
for b in bosses(EV['epreuves']):
    b['phases'] = [{"pv": 0.5, "message": "L'esprit gardien rassemble ses dernières forces : il brille plus fort que jamais !",
                    "bonus": {"attaque": 1, "magie": 1}}]
    n += 1
carte.rewrite_events(['mines_boss', 'rongecrins', 'epreuves'], EV)
print('phases ajoutées : troll, Rongecrins, %d épreuves' % n)
