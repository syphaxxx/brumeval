"""Créatures de la nuit (étape « 20 heures », 2026-10-20) : un champ « nuit » dans les zones de rencontre de onze cartes
du dehors ; quand il fait nuit, la moitié des rencontres viennent de cette liste (Game::encounter). Une seule fois
(refuse si la Vallée a déjà des créatures de nuit).

    python outils/villes/nuit.py
"""
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
import carte  # noqa: E402

NUIT = {'vallee': ['chauvenuit', 'brumelin'], 'plaine': ['chauvenuit', 'esprillon'], 'sylvenoire': ['brumelin', 'esprillon', 'chauvenuit'],
        'cendrelune': ['fumerole', 'osselet'], 'col': ['noctaile', 'esprillon'], 'pics': ['noctaile', 'brumeloup'],
        'val': ['osselet', 'noctaile'], 'corail': ['noyelin', 'nuageon'], 'lueurs': ['noctaile', 'aubespectre'],
        'marais': ['aubespectre', 'osselet'], 'pierres': ['spectrecho', 'noctaile']}
if any('nuit' in z for z in carte.load('vallee')['zones']):
    sys.exit('Déjà fait.')
for mid, pool in NUIT.items():
    m = carte.load(mid)
    zones = []
    for z in m['zones']:
        z2 = {}
        for k, v in z.items():
            z2[k] = v
            if k == 'creatures':
                z2['nuit'] = pool
        zones.append(z2)
    m['zones'] = zones
    carte.save(mid, m)
    print(mid, pool)
