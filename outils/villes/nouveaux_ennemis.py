"""Nouvelles créatures d'après les monstres restants de la planche (plan de l'utilisateur du 2026-10-15) :
Tisseline (araignée, Grotte), Osselet (squelette, Ruines et Tour), Gobelard et Brutor (pillards de Sylve-Noire),
Draconnet (rare, volcan) et Coffrelin (coffre piégé des Ruines)."""
import json
import sys
sys.path.insert(0, '.')
import carte


def sp(id, nom, sprite, types, base, forme, couleurs, apprend, **extra):
    e = {'id': id, 'nom': nom, 'sprite': sprite}
    if len(types) == 1:
        e['type'] = types[0]
    else:
        e['types'] = types
    e['base'] = dict(zip(['pv', 'pm', 'attaque', 'defense', 'magie', 'resistance', 'vitesse'], base))
    e.update({k: v for k, v in extra.items() if k in ('esquive', 'critique')})
    e['forme'] = forme
    e['couleurs'] = couleurs
    e['apprend'] = apprend
    e.update({k: v for k, v in extra.items() if k not in ('esquive', 'critique')})
    return e


NEW = [
    sp('tisseline', 'Tisseline', 'araignee', ['poison', 'ombre'], [50, 18, 56, 44, 44, 42, 62], 'insecte', ['#4a3a4a', '#e05a3a'],
       [[1, 'venin'], [1, 'coup_bas'], [14, 'acide']], esquive=5, talent='piquant'),
    sp('osselet', 'Osselet', 'squelette', ['ombre', 'esprit'], [46, 28, 54, 40, 60, 52, 60], 'fantome', ['#e8e0d0', '#8a7a6a'],
       [[1, 'coup_bas'], [1, 'onde'], [44, 'cauchemar']], talent='furtif'),
    sp('gobelard', 'Gobelard', 'gobelin', ['normal'], [50, 10, 60, 42, 24, 34, 64], 'souris', ['#5a8a3a', '#c8b07a'],
       [[1, 'ruee'], [1, 'coup_bas'], [12, 'hurlement']], critique=10, talent='vif'),
    sp('brutor', 'Brutor', 'orc', ['normal', 'roche'], [70, 8, 72, 58, 20, 38, 34], 'golem', ['#4a7a3a', '#8a6a4a'],
       [[1, 'ruee'], [1, 'hurlement'], [14, 'seisme']], talent='rage', role_combat='protecteur'),
    sp('draconnet', 'Draconnet', 'dragonnet', ['feu', 'vent'], [52, 22, 56, 46, 52, 44, 58], 'lezard', ['#c83a2a', '#ffb050'],
       [[1, 'flam'], [1, 'crocs'], [14, 'brasier']], talent='vif'),
    sp('coffrelin', 'Coffrelin', 'mimique', ['metal'], [80, 10, 78, 80, 30, 60, 40], 'rocher', ['#8a5a2a', '#ffd34d'],
       [[1, 'crocs'], [1, 'coup_acier'], [1, 'blindage'], [40, 'eclats_acier']], talent='carapace'),
]

p = carte.R + 'especes.json'
d = json.load(open(p, encoding='utf-8'))
ids = {e['id'] for e in NEW}
d = [e for e in d if e['id'] not in ids] + NEW
open(p, 'w', encoding='utf-8', newline='\n').write(carte.pretty(d) + '\n')


def add_pool(name, pred, new):
    m = carte.load(name)
    for z in m['zones']:
        if pred(z):
            for c in new:
                if c not in z['creatures']:
                    z['creatures'].append(c)
    return m


m = add_pool('grotte', lambda z: True, ['tisseline'])
carte.save('grotte', m)
m = add_pool('sylvenoire', lambda z: (z['x'], z['y']) == (0, 0), ['gobelard'])
for z in m['zones']:
    if (z['x'], z['y']) == (0, 40):
        z['creatures'] += [c for c in ('gobelard', 'brutor') if c not in z['creatures']]
carte.save('sylvenoire', m)
m = add_pool('ruines', lambda z: True, ['osselet'])
# le coffre piégé : un « boss » de carte dans la grande salle (on le prend pour un coffre)
m['boss'] = [b for b in m['boss'] if b['espece'] != 'coffrelin']
m['boss'].append({'x': 30, 'y': 18, 'espece': 'coffrelin', 'drapeau': 'coffrelin_ruines', 'evenement': 'coffrelin'})
carte.save('ruines', m)
m = add_pool('tour', lambda z: True, ['osselet'])
carte.save('tour', m)
m = add_pool('cendrelune', lambda z: z['niveau_min'] >= 15, ['draconnet'])
carte.save('cendrelune', m)

e = json.load(open(carte.R + 'evenements.json', encoding='utf-8'))
e['coffrelin'] = {'actions': [
    {'action': 'dire', 'texte': "Un vieux coffre, à moitié enfoui… Il a des dents !"},
    {'action': 'combat', 'ennemis': [{'espece': 'coffrelin', 'niveau': 48, 'boss': True}], 'fuite': True, 'capture': False,
     'victoire': [{'action': 'drapeau', 'nom': 'coffrelin_ruines'}, {'action': 'donner', 'objet': 'lanterne_or', 'quantite': 3},
                  {'action': 'dire', 'texte': 'Le Coffrelin recrache trois lanternes d\'or avant de se refermer pour de bon.'}]}]}
# ajouté à la fin du fichier sans toucher à la mise en page du reste
txt = open(carte.R + 'evenements.json', encoding='utf-8').read()
if '"coffrelin":' not in txt:
    end = txt.rstrip().rfind('}')
    key = '  "coffrelin": '
    txt = txt[:end].rstrip() + ',
' + key + carte.pretty(e['coffrelin'], 2, len(key.encode())) + '
}
'
    open(carte.R + 'evenements.json', 'w', encoding='utf-8', newline='
').write(txt)
print('ok')
