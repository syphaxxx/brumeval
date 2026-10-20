"""Formes régionales (étape « 20 heures », 2026-10-20) : sept créatures de plus, recolorées d'après les planches, qui
vivent dans les nouveaux lieux (Col des Hurlevents, Val des Murmures, Grotte des Marées, île de Corail). Plus de
créatures à capturer pour le grand bestiaire. Une seule fois (refuse si « bourrasquin » existe).

    python outils/villes/formes.py ; python outils/sprites/decoupe.py
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402
import place_util  # noqa: E402

D = carte.R
ESP = json.load(open(D + 'especes.json', encoding='utf-8'))
if any(e['id'] == 'bourrasquin' for e in ESP):
    sys.exit('Déjà fait.')
BASE = {e['id']: e for e in ESP}


def forme(i, nom, src, types, mult, couleurs, apprend, talent, teintes):
    b = BASE[src]
    e = {"id": i, "nom": nom, "sprite": i}
    e["types" if len(types) > 1 else "type"] = types if len(types) > 1 else types[0]
    e["base"] = {k: round(v * mult) for k, v in b['base'].items()}
    e["forme"] = b.get('forme', 'rocher')
    e["couleurs"] = couleurs
    e["apprend"] = apprend
    e["talent"] = talent
    if b.get('limite'):
        e["limite"] = b['limite']
    return e, {"id": i, "de": b['sprite'], "teintes": teintes}


NEW = [
    forme("bourrasquin", "Bourrasquin", "piafouine", ["vent", "glace"], 1.35, ["#c8e0f0", "#ffffff"],
          [[1, "rafale"], [1, "bec"], [1, "poudreuse"], [26, "lame_air"]], "vif", [[-1, 0, 200, 0.5, 1.25, 0.25]]),
    forme("givroc", "Givroc", "rocaillou", ["roche", "glace"], 1.35, ["#a0c8e0", "#e8f8ff"],
          [[1, "eboul"], [1, "gel"], [1, "carapace"], [28, "avalanche"]], "carapace", [[-1, 0, 205, 0.45, 1.3, 0.2]]),
    forme("lanternelle", "Lanternelle", "esprillon", ["esprit", "feu"], 1.4, ["#ffb060", "#fff0c0"],
          [[1, "onde"], [1, "feu"], [1, "cauchemar"], [38, "pyro"]], "furtif", [[180, 280, 32, 1.1, 1.05]]),
    forme("fongombre", "Fongombre", "champichou", ["plante", "ombre"], 1.45, ["#6a4a8a", "#d0b0f0"],
          [[1, "spore"], [1, "brume"], [1, "pluie_spores"], [38, "eclipse"]], "regeneration", [[-1, 40, 280, 0.8, 0.8], [300, 360, 280, 0.8, 0.8]]),
    forme("corailleur", "Corailleur", "grenouillon", ["eau", "roche"], 1.6, ["#ff8a8a", "#ffe0d8"],
          [[1, "bulle"], [1, "jet_pierres"], [1, "torrent"], [48, "seisme"]], "carapace", [[60, 200, 355, 0.9, 1.15]]),
    forme("perlin", "Perlin", "ecumet", ["eau", "lumiere"], 1.2, ["#f0f0ff", "#ffffff"],
          [[1, "jet"], [1, "lumiere"], [1, "vague"], [48, "rayon"]], "meditation", [[-1, 0, 250, 0.25, 1.35, 0.15]]),
    forme("murenix", "Murénix", "serpentin", ["eau", "poison"], 1.6, ["#2a7a8a", "#a0ffe0"],
          [[1, "venin"], [1, "jet"], [1, "acide"], [48, "trombe"]], "piquant", [[240, 330, 180, 1.0, 1.0]]),
]
for e, _ in NEW:
    ESP.append(e)
open(D + 'especes.json', 'w', encoding='utf-8', newline='\n').write(carte.pretty(ESP) + '\n')
V = os.path.join(os.path.dirname(ICI), 'sprites', 'variantes.json')
vt = open(V, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in vt else '\n'
body = vt.rstrip()[:-1].rstrip()
for _, v in NEW:
    body += ',' + nl + '  ' + json.dumps(v, ensure_ascii=False)
open(V, 'w', encoding='utf-8', newline='').write(body + nl + ']' + nl)

# Où elles vivent
def add(mid, sps):
    m = carte.load(mid)
    for z in m['zones']:
        z['creatures'] = z['creatures'] + [s for s in sps if s not in z['creatures']]
    carte.save(mid, m)


add('col', ['bourrasquin', 'givroc'])
add('val', ['lanternelle', 'fongombre'])
add('marees', ['corailleur', 'perlin', 'murenix'])
# l'île de Corail : des hautes herbes sur la plage, avec ses créatures
K = carte.load('corail')
rows = [list(r) for r in K['tuiles']]
n = 0
for y in range(len(rows)):
    for x in range(len(rows[0])):
        if rows[y][x] == '.' and (x < 9 or x > 31) and 3 < y < 26 and (x * 7 + y * 3) % 5:
            rows[y][x] = ','
            n += 1
K['tuiles'] = [''.join(r) for r in rows]
K['zones'] = [{"x": 0, "y": 0, "l": len(rows[0]), "h": len(rows), "niveau_min": 42, "niveau_max": 46, "max_ennemis": 3,
               "creatures": ['corailleur', 'perlin', 'ecumet', 'coquillon']}]
carte.save('corail', K)
print('7 formes ajoutées ;', n, 'cases d\'herbes sur Corail')
