"""Écrit les musiques de data/sons.json à partir de partitions courtes (outils/musique/morceaux.py).

    python outils/musique/compose.py            # récrit les morceaux décrits dans morceaux.py (les autres restent)
    python outils/musique/compose.py --liste    # ce que chaque morceau contient (voix, mesures)

Un morceau y est décrit comme un musicien l'écrirait : tonalité implicite, une grille d'accords (un accord par
mesure, ou deux séparés par « / »), une mélodie mesure par mesure, et des choix d'accompagnement : basse
(« fondamentale », « quinte », « octaves », « marche », « pedale »), arpège (« monte », « monte_descend »,
« brise », « accords »), batterie (« calme », « marche », « combat », « valse », « orage », « aucune »). Le script
fabrique les voix (cases d'une croche ou d'une double croche) au format de sons.json : la basse et l'arpège
suivent toujours les accords, donc rien ne sonne faux.

Accords : C, Cm, C7, Cmaj7, Cm7, Cdim, Csus4, Cadd9, avec dièse ou bémol (F#m, Bb). Notes de la mélodie : A4, C#5,
Bb3, « . » (prolonger), « - » (silence). Mesure de 4 temps (8 cases) ou de 3 temps (6 cases, « mesure »: 3).
"""
import json
import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
SONS = os.path.join(os.path.dirname(os.path.dirname(ICI)), 'data', 'sons.json')
sys.path.insert(0, ICI)

NOMS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
DEMI = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
QUALITES = {'': [0, 4, 7], 'm': [0, 3, 7], '7': [0, 4, 7, 10], 'maj7': [0, 4, 7, 11], 'm7': [0, 3, 7, 10],
            'dim': [0, 3, 6], 'sus4': [0, 5, 7], 'sus2': [0, 2, 7], 'add9': [0, 4, 7, 14], 'm9': [0, 3, 7, 10, 14],
            '5': [0, 7], 'aug': [0, 4, 8], 'm6': [0, 3, 7, 9], '6': [0, 4, 7, 9]}


def note(midi):
    return NOMS[midi % 12] + str(midi // 12 - 1)


def accord(nom):
    """« F#m7 » -> (fondamentale 0-11, intervalles)."""
    r = DEMI[nom[0]]
    i = 1
    if i < len(nom) and nom[i] in '#b':
        r += 1 if nom[i] == '#' else -1
        i += 1
    q = nom[i:]
    if q not in QUALITES:
        raise SystemExit(f'accord inconnu : {nom}')
    return r % 12, QUALITES[q]


def grille(texte, cases):
    """Un accord par case : « C G/Em Am F » (barres « | » permises)."""
    out = []
    for mes in texte.replace('|', ' ').split():
        parts = mes.split('/')
        n = cases // len(parts)
        for k, p in enumerate(parts):
            out += [accord(p)] * (n if k < len(parts) - 1 else cases - n * (len(parts) - 1))
    return out


def bas(r, octave):
    return 12 * (octave + 1) + r


def voix_basse(acc, cases, style, octave=2):
    out = []
    for m in range(0, len(acc), cases):
        mes = acc[m:m + cases]
        for k, (r, iv) in enumerate(mes):
            debut = k == 0 or mes[k - 1] != (r, iv)
            f = bas(r, octave)
            quinte = f + (iv[2] if len(iv) > 2 else 7)
            if style == 'pedale':
                out.append(note(f) if k == 0 else '.')
            elif style == 'fondamentale':
                out.append(note(f) if debut or k == cases // 2 else '.')
            elif style == 'quinte':
                pas = cases // 4 if cases % 4 == 0 else cases // 3
                t = k // pas
                if k % pas:
                    out.append('.')
                else:
                    out.append(note(f if t % 2 == 0 else quinte))
            elif style == 'octaves':
                out.append(note(f if k % 2 == 0 else f + 12))
            elif style == 'marche':  # basse qui marche : fondamentale, tierce, quinte, note de passage
                pas = 2
                if k % pas:
                    out.append('.')
                else:
                    t = (k // pas) % 4
                    nxt = acc[(m + cases) % len(acc)][0]
                    approche = bas(nxt, octave) + (1 if (bas(nxt, octave) < f) else -1)
                    out.append(note([f, f + iv[1], quinte, approche][t]))
            elif style == 'galop':  # combat : croches répétées, octave au temps
                out.append(note(f + (12 if k % 4 == 2 else 0)))
            else:
                raise SystemExit(f'basse inconnue : {style}')
    return out


def voix_arpege(acc, cases, style, octave=4, double=False):
    out = []
    for m in range(0, len(acc), cases):
        mes = acc[m:m + cases]
        for k, (r, iv) in enumerate(mes):
            base = 12 * (octave + 1) + r
            ton = [base + i for i in iv[:3]] + [base + 12]
            if style == 'monte':
                out.append(note(ton[k % 4]))
            elif style == 'monte_descend':
                seq = [0, 1, 2, 3, 2, 1]
                out.append(note(ton[seq[k % 6]]))
            elif style == 'brise':
                seq = [0, 2, 1, 2]
                out.append(note(ton[seq[k % 4]]))
            elif style == 'accords':  # un accord frappé sur les temps (piano), les autres cases tenues
                out.append(note(ton[(k // 2) % 3]) if k % 2 == 0 else '.')
            elif style == 'valse':  # 3 temps : rien sur le 1 (la basse), accord sur 2 et 3
                t = k % 6
                out.append('-' if t < 2 else (note(ton[1]) if t in (2, 4) else '.'))
            else:
                raise SystemExit(f'arpège inconnu : {style}')
    return out


BATTERIES = {  # par mesure de 8 cases : grosse caisse (k), caisse claire (s), charleston (h)
    'calme': {'k': 'x - - - - - - -', 'h': '- - x - - - x -'},
    'marche': {'k': 'x - - - x - - -', 's': '- - x - - - x -', 'h': 'x x x x x x x x'},
    'combat': {'k': 'x - - x x - - -', 's': '- - x - - - x -', 'h': 'x x x x x x x x'},
    'orage': {'k': 'x - x - x - x x', 's': '- - x - - - x x', 'h': 'x x x x x x x x'},
    'valse': {'k': 'x - - - - -', 'h': '- - x - x -'},
    'marin': {'k': 'x - - x - -', 's': '- - - - x -', 'h': 'x - x - x -'},
}


def partition(cases):
    lignes = []
    for i in range(0, len(cases), MESURE_CASES[0]):
        lignes.append(' '.join(cases[i:i + MESURE_CASES[0]]))
    return ' | '.join(lignes)


MESURE_CASES = [8]


def fabrique(nom, d):
    cases = 6 if d.get('mesure') == 3 else 8
    MESURE_CASES[0] = cases
    mel = []
    for mes in d['melodie']:
        t = mes.split()
        if len(t) != cases:
            raise SystemExit(f'{nom} : mesure de {len(t)} cases au lieu de {cases} : {mes}')
        mel += t
    nmes = len(mel) // cases
    acc = grille(d['accords'], cases)
    if len(acc) % (cases) or len(acc) // cases not in (nmes, nmes // 2, nmes // 4):
        raise SystemExit(f'{nom} : {len(acc) // cases} mesures d\'accords pour {nmes} de mélodie')
    voix = []
    m = {"onde": d.get('onde', 'carree'), "volume": d.get('volume', .15)}
    if m['onde'] == 'carree':
        m['rapport'] = d.get('rapport', .5)
    for k in ('vibrato', 'attaque', 'ondulation', 'extinction'):
        if k in d: m[k] = d[k]
    if 'vibrato' not in m and m['onde'] in ('carree', 'triangle', 'scie'):
        m['vibrato'] = .18
    m['notes'] = partition(mel)
    voix.append(m)
    if 'contre' in d:  # contre-chant (souvent plus court : il boucle)
        c = []
        for mes in d['contre']:
            c += mes.split()
        v = {"onde": d.get('onde_contre', 'carree'), "volume": d.get('volume_contre', .08)}
        if v['onde'] == 'carree':
            v['rapport'] = d.get('rapport_contre', .25)
        if 'vibrato_contre' in d: v['vibrato'] = d['vibrato_contre']
        if 'attaque_contre' in d: v['attaque'] = d['attaque_contre']
        v['notes'] = partition(c)
        voix.append(v)
    if d.get('arpege'):
        v = {"onde": d.get('onde_arpege', 'carree'), "volume": d.get('volume_arpege', .05)}
        if v['onde'] == 'carree':
            v['rapport'] = d.get('rapport_arpege', .125)
        if 'extinction_arpege' in d: v['extinction'] = d['extinction_arpege']
        v['notes'] = partition(voix_arpege(acc, cases, d['arpege'], d.get('octave_arpege', 4)))
        voix.append(v)
    if d.get('basse'):
        v = {"onde": d.get('onde_basse', 'triangle'), "volume": d.get('volume_basse', .3)}
        if v['onde'] == 'carree':
            v['rapport'] = .5
        v['notes'] = partition(voix_basse(acc, cases, d['basse'], d.get('octave_basse', 2)))
        voix.append(v)
    bat = d.get('batterie', 'aucune')
    if bat != 'aucune':
        pat = BATTERIES[bat]
        vol = d.get('volume_batterie', 1)
        sons = {'k': (180, .15, .08), 's': (1800, .09, .06), 'h': (7000, .035, .02)}
        for part in ('k', 's', 'h'):
            if part not in pat:
                continue
            f, v0, ext = sons[part]
            motif = pat[part].split()
            longueur = d.get('mesures_batterie', 4)
            notes = []
            for i in range(longueur):
                mm = list(motif)
                if part == 's' and i == longueur - 1 and bat in ('marche', 'combat', 'orage'):
                    mm[-3:] = ['x', 'x', 'x']  # roulement à la fin de la phrase
                notes += mm
            voix.append({"onde": "bruit", "volume": round(v0 * vol, 3), "frequence": f, "extinction": ext,
                         "notes": partition(notes)})
    out = {"tempo": d['tempo']}
    if cases == 6 or d.get('pas_par_temps'):
        out["pas_par_temps"] = d.get('pas_par_temps', 2)
    if 'echo' in d:
        out["echo"] = d['echo']
    out["voix"] = voix
    return out


def ecrire(sons):
    """sons.json lisible : une voix par bloc, ses notes sur une ligne."""
    def bloc(o, ind):
        p = ' ' * ind
        if isinstance(o, dict):
            if not o:
                return '{}'
            simple = all(not isinstance(v, (dict, list)) for v in o.values()) and len(json.dumps(o, ensure_ascii=False)) < 100
            if simple:
                return json.dumps(o, ensure_ascii=False)
            return '{\n' + ',\n'.join(p + '  ' + json.dumps(k, ensure_ascii=False) + ': ' + bloc(v, ind + 2) for k, v in o.items()) + '\n' + p + '}'
        if isinstance(o, list):
            if all(not isinstance(v, (dict, list)) for v in o) and len(json.dumps(o, ensure_ascii=False)) < 100:
                return json.dumps(o, ensure_ascii=False)
            return '[\n' + ',\n'.join(p + '  ' + bloc(v, ind + 2) for v in o) + '\n' + p + ']'
        return json.dumps(o, ensure_ascii=False)
    with open(SONS, 'w', encoding='utf-8', newline='\n') as f:
        f.write(bloc(sons, 0) + '\n')


def main():
    import morceaux
    sons = json.load(open(SONS, encoding='utf-8'))
    for nom, d in morceaux.MORCEAUX.items():
        sons['musiques'][nom] = fabrique(nom, d)
        if '--liste' in sys.argv:
            m = sons['musiques'][nom]
            print(nom, m['tempo'], 'bpm', [(v['onde'], len(v['notes'].split('|'))) for v in m['voix']])
    for lieu, nom in getattr(morceaux, 'LIEUX', {}).items():
        sons['lieux'][lieu] = nom
    if '--liste' not in sys.argv:
        ecrire(sons)
        print(f'{len(morceaux.MORCEAUX)} morceaux écrits dans {SONS}')


if __name__ == '__main__':
    main()
