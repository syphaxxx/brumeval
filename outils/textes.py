"""Rassemble toutes les phrases du jeu à traduire dans data/langues/<langue>.json (3.1).

    python outils/textes.py            # met à jour data/langues/en.json
    python outils/textes.py en --manque # liste seulement les phrases sans traduction

Les phrases déjà traduites sont gardées ; les nouvelles arrivent avec une traduction vide "" (le jeu
affiche alors le français). Sources : les T("…") du code, les tableaux de noms (états, statistiques…)
et les textes des données (noms, descriptions, dialogues, messages, quêtes).
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANG = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else 'en'

LIT = r'"((?:[^"\\]|\\.)*)"'
def unescape(s):
    return s.encode('utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8') if '\\' in s else s

texts = []  # dans l'ordre d'apparition (le fichier reste lisible)
seen = set()
def add(s):
    if s and s not in seen and re.search(r'[A-Za-zÀ-ÿ]', s):
        seen.add(s)
        texts.append(s)

# 1. Le code : T("…"), et les tableaux de noms traduits à l'usage
ARRAYS = ['STATUS_NAMES', 'STATUS_TAGS', 'STAGE_NAMES', 'ZONE_NAMES', 'PACE_NAMES', 'GEAR_SLOTS', 'GEAR_STATS', 'COLS', 'ROWS',
          'KEY_NAMES', 'KIND', 'CATS', 'names']
for f in sorted(glob.glob(os.path.join(ROOT, 'src', '*.cpp'))):
    if f.endswith('test.cpp') or f.endswith('procgen.cpp'): continue
    src = open(f, encoding='utf-8').read()
    for m in re.finditer(r'(?<![A-Za-z_])T\(' + LIT + r'\)', src): add(unescape(m.group(1)))
    for name in ARRAYS:
        for m in re.finditer(r'\b' + name + r'\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}', src):
            for s in re.findall(LIT, m.group(1)): add(unescape(s))

# 2. Les données
KEYS = {'nom', 'description', 'texte', 'message', 'dialogue', 'lieu', 'fin_texte', 'aide', 'role', 'aussi'}
SKIP_FILES = {'sons.json'}
def walk(j):
    if isinstance(j, dict):
        for k, v in j.items():
            if k in KEYS:
                if isinstance(v, str): add(v)
                elif isinstance(v, list):
                    for x in v:
                        if isinstance(x, str): add(x)
            walk(v)
    elif isinstance(j, list):
        for x in j: walk(x)
files = sorted(glob.glob(os.path.join(ROOT, 'data', '*.json'))) + sorted(glob.glob(os.path.join(ROOT, 'data', 'cartes', '*.json')))
for f in files:
    if os.path.basename(f) in SKIP_FILES: continue
    walk(json.load(open(f, encoding='utf-8')))

out = os.path.join(ROOT, 'data', 'langues', LANG + '.json')
old = json.load(open(out, encoding='utf-8')) if os.path.exists(out) else {}
if '--manque' in sys.argv:
    for t in texts:
        if not old.get(t): print(t)
    sys.exit(0)
new = {t: old.get(t, '') for t in texts}
for k, v in old.items():  # les traductions ajoutées à la main (phrases du monde généré…) sont gardées
    if k not in new and v: new[k] = v
with open(out, 'w', encoding='utf-8', newline='\n') as fp:
    fp.write('{\n' + ',\n'.join('  ' + json.dumps(k, ensure_ascii=False) + ': ' + json.dumps(v, ensure_ascii=False) for k, v in new.items()) + '\n}\n')
done = sum(1 for v in new.values() if v)
print(f'{out} : {len(new)} phrases, {done} traduites, {len(new) - done} à traduire')
