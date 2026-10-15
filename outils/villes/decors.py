"""Grands décors (« decor » + « dessin ») : la ferme au moulin près du col (Vallée), des pans de ruines dans la
grande salle des Ruines, le château de la Tour au-dessus de son entrée."""
import sys
sys.path.insert(0, '.')
import carte

# ---------------- Vallée : la ferme du moulin, à l'est de la route du col ----------------
m = carte.load('vallee')
G = carte.Grid(m['tuiles'])
G.rect(47, 34, 57, 42, '.')
G.rect(47, 35, 52, 35, '=')               # chemin de la ferme
G.rect(47, 37, 52, 37, 'H')               # champ clos (blé en fleurs)
G.rect(53, 37, 53, 40, 'H')
G.rect(48, 38, 52, 40, 'F')
G.put(50, 37, '.')                        # barrière ouverte
for x, y in [(56, 34), (47, 42), (57, 42), (56, 41)]:
    G.put(x, y, 'T')
G.put(52, 34, 'C'), G.put(57, 36, 'v')
m['tuiles'] = G.rows()
m['batiments'] = [b for b in m['batiments'] if b.get('genre') != 'decor']
m['batiments'].append({'x': 53, 'y': 34, 'l': 3, 'h': 2, 'genre': 'decor', 'nom': 'Moulin', 'toit': '#7a8a4a', 'dessin': 'moulin'})
m['habitants'] = [h for h in m['habitants'] if h['apparence'] not in ('paysan', 'vache', 'cheval')]
m['habitants'] += [
    {'x': 51, 'y': 36, 'apparence': 'paysan', 'direction': 'bas', 'dialogue': ["Le moulin tourne depuis trois générations.", "Sans la brume, la récolte serait deux fois plus belle !"]},
    {'x': 55, 'y': 39, 'apparence': 'vache', 'direction': 'gauche', 'dialogue': ['Meuh…']},
    {'x': 56, 'y': 37, 'apparence': 'cheval', 'direction': 'gauche', 'dialogue': ['Le cheval de la ferme hennit doucement.']},
]
carte.save('vallee', m)

# ---------------- Ruines : deux pans de murs écroulés dans la grande salle ----------------
m = carte.load('ruines')
m['batiments'] = [b for b in m['batiments'] if b.get('genre') != 'decor']
m['batiments'] += [{'x': 6, 'y': 18, 'l': 3, 'h': 2, 'genre': 'decor', 'nom': 'Ruines', 'toit': '#7a8a4a', 'dessin': 'ruines'},
                   {'x': 27, 'y': 21, 'l': 3, 'h': 2, 'genre': 'decor', 'nom': 'Ruines', 'toit': '#7a8a4a', 'dessin': 'ruines'}]
carte.save('ruines', m)

# ---------------- Tour : le château au-dessus du champ de neige (posé sur les murs) ----------------
m = carte.load('tour')
m['batiments'] = [b for b in m['batiments'] if b.get('genre') != 'decor']
m['batiments'].append({'x': 4, 'y': 22, 'l': 8, 'h': 4, 'genre': 'decor', 'nom': 'Tour', 'toit': '#7a8a4a', 'dessin': 'chateau'})
carte.save('tour', m)
print('ok')
