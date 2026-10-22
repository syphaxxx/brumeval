"""Les musiques de Brumeval (2026-10-19, étape « 20 heures »), lues par compose.py.

Chaque morceau : tempo, grille d'accords (un accord par mesure), mélodie (une chaîne par mesure : 8 cases en 4 temps,
6 en 3 temps), et l'accompagnement fabriqué d'après les accords. Les mélodies posent les notes de l'accord sur les
temps forts et passent par des notes voisines entre eux ; les phrases vont par 4 mesures (question, réponse).
"""

MORCEAUX = {
    # Écran titre : thème de Brumeval. La mineur qui s'ouvre vers do majeur (l'aventure), pont dramatique en ré mineur
    'titre': {
        'tempo': 96, 'onde': 'carree', 'rapport': .5, 'volume': .14, 'vibrato': .2,
        'accords': 'Am F C G Am F G E | F G C Am F G C C | Dm Am Dm E Dm Am F E',
        'melodie': [
            'A4 . . . C5 . E5 .', 'F5 . E5 . C5 . A4 .', 'G4 . . . C5 . E5 .', 'D5 . . . . . - -',
            'E5 . D5 C5 B4 . C5 .', 'A4 . . . F4 . A4 .', 'B4 . C5 . D5 . G5 .', 'G#4 . . . . . - -',
            'C5 . . A4 C5 . F5 .', 'D5 . . B4 D5 . G5 .', 'E5 . . . G5 . E5 .', 'C5 . . . A4 . . .',
            'F5 . E5 . D5 . C5 .', 'B4 . C5 . D5 . . .', 'C5 . . . G4 . E4 .', 'C5 . . . . . - -',
            'D5 . F5 . A5 . F5 .', 'E5 . . . C5 . A4 .', 'D5 . F5 . A5 . C6 .', 'B5 . . . G#5 . E5 .',
            'F5 . E5 . D5 . F5 .', 'E5 . C5 . A4 . C5 .', 'A4 . C5 . F5 . A5 .', 'G#5 . . . B5 . . .',
        ],
        'contre': ['C4 . . . . . . .', 'A3 . . . . . . .', 'E4 . . . . . . .', 'D4 . . . . . . .', 'C4 . . . . . . .', 'A3 . . . . . . .', 'D4 . . . . . . .', 'B3 . . . . . . .', 'A3 . . . . . . .', 'B3 . . . . . . .', 'C4 . . . . . . .', 'C4 . . . . . . .', 'A3 . . . . . . .', 'B3 . . . . . . .', 'E4 . . . . . . .', 'E4 . . . . . . .', 'F3 . . . . . . .', 'E3 . . . . . . .', 'F3 . . . . . . .', 'G#3 . . . . . . .', 'F3 . . . . . . .', 'E3 . . . . . . .', 'A3 . . . . . . .', 'G#3 . . . . . . .'],
        'onde_contre': 'triangle', 'volume_contre': .12, 'attaque_contre': .2,
        'arpege': 'brise', 'octave_arpege': 4, 'volume_arpege': .045,
        'basse': 'quinte', 'batterie': 'marche', 'volume_batterie': .6,
        'echo': {'delai': .31, 'retour': .25, 'melange': .18},
    },
    # Vallée : pastorale en fa majeur, flûte (triangle) et petite harpe
    'vallee': {
        'tempo': 108, 'onde': 'carree', 'rapport': .25, 'volume': .12, 'vibrato': .15,
        'accords': 'F C Dm Bb F Bb C C | Bb C Am Dm Gm C F F',
        'melodie': [
            'C5 . A4 . F4 . A4 .', 'G4 . . . E4 . G4 .', 'F4 . A4 . D5 . C5 .', 'D5 . . . Bb4 . . .',
            'A4 . C5 . F5 . E5 .', 'D5 . C5 . Bb4 . D5 .', 'C5 . . . E5 . G5 .', 'E5 . . . . . - -',
            'F5 . . D5 Bb4 . D5 .', 'E5 . . C5 G4 . C5 .', 'E5 . D5 . C5 . A4 .', 'D5 . . . A4 . . .',
            'Bb4 . A4 . G4 . D5 .', 'C5 . Bb4 . A4 . G4 .', 'A4 . . . C5 . . .', 'F4 . . . . . - -',
        ],
        'contre': ['A3 . . . . . . .', 'G3 . . . . . . .', 'F3 . . . . . . .', 'F3 . . . . . . .',
                   'A3 . . . . . . .', 'F3 . . . . . . .', 'E3 . . . . . . .', 'G3 . . . . . . .',
                   'D4 . . . . . . .', 'E4 . . . . . . .', 'C4 . . . . . . .', 'A3 . . . . . . .',
                   'Bb3 . . . . . . .', 'G3 . . . . . . .', 'A3 . . . . . . .', 'A3 . . . . . . .'],
        'onde_contre': 'triangle', 'volume_contre': .13, 'attaque_contre': .15,
        'arpege': 'monte', 'octave_arpege': 4, 'onde_arpege': 'triangle', 'volume_arpege': .09,
        'basse': 'quinte', 'batterie': 'calme', 'volume_batterie': .7,
    },
    # Forêt : ré dorien à trois temps, mystérieux ; harpe qui monte et descend, écho
    'foret': {
        'tempo': 96, 'mesure': 3, 'onde': 'triangle', 'volume': .26, 'vibrato': .22,
        'accords': 'Dm C Dm Am Dm C G Am | Bb C Dm Am Bb C Dm Dm',
        'melodie': [
            'D5 . . . A4 .', 'C5 . . . G4 .', 'F4 . A4 . D5 .', 'E5 . . . . .',
            'F5 . E5 . D5 .', 'E5 . . . C5 .', 'B4 . . . D5 .', 'A4 . . . . .',
            'D5 . . . F5 .', 'E5 . . . C5 .', 'D5 . F5 . A5 .', 'G5 . E5 . C5 .',
            'F5 . D5 . Bb4 .', 'C5 . E5 . G5 .', 'F5 . . . E5 .', 'D5 . . . . .',
        ],
        'arpege': 'monte_descend', 'octave_arpege': 3, 'onde_arpege': 'sinus', 'volume_arpege': .12, 'extinction_arpege': .5,
        'basse': 'pedale', 'volume_basse': .26,
        'echo': {'delai': .38, 'retour': .35, 'melange': .3},
    },
    # Monts Cendrelune : mi phrygien, lourd (le volcan)
    'cendres': {
        'tempo': 116, 'onde': 'carree', 'rapport': .25, 'volume': .12, 'vibrato': .25,
        'accords': 'Em F Em Dm Em F G F | Am F Em Dm Am F Em Em',
        'melodie': [
            'E4 . . . G4 . B4 .', 'C5 . . . A4 . F4 .', 'G4 . B4 . E5 . D5 .', 'A4 . . . F4 . . .',
            'B4 . . . E5 . G5 .', 'F5 . E5 . C5 . A4 .', 'B4 . D5 . G5 . F5 .', 'E5 . . . . . - -',
            'E5 . . C5 A4 . C5 .', 'F5 . . C5 A4 . F4 .', 'G4 . . E4 B4 . G4 .', 'F4 . E4 . D4 . F4 .',
            'A4 . C5 . E5 . A5 .', 'G5 . F5 . E5 . C5 .', 'B4 . . . G4 . . .', 'E4 . . . . . - -',
        ],
        'arpege': 'brise', 'octave_arpege': 3, 'volume_arpege': .05,
        'basse': 'octaves', 'volume_basse': .3, 'batterie': 'marche', 'volume_batterie': .9,
    },
    # Grottes : la mineur, notes rares qui résonnent
    'grotte': {
        'tempo': 76, 'onde': 'sinus', 'volume': .3, 'vibrato': .12,
        'accords': 'Am Am F E Am Dm E E | F G Em Am Dm Am E E',
        'melodie': [
            'A4 . . . - - E5 .', 'C5 . . . - - B4 .', 'A4 . . . - - C5 .', 'B4 . . . . . - -',
            'E5 . . . - - D5 .', 'F5 . . . - - E5 .', 'G#4 . . . B4 . . .', 'E4 . . . . . - -',
            '- - C5 . A4 . F4 .', '- - D5 . B4 . G4 .', '- - E5 . B4 . G4 .', 'A4 . . . . . - -',
            '- - F5 . D5 . A4 .', '- - E5 . C5 . A4 .', 'B4 . . . G#4 . . .', 'E5 . . . . . - -',
        ],
        'arpege': 'brise', 'octave_arpege': 3, 'onde_arpege': 'triangle', 'volume_arpege': .1, 'extinction_arpege': .6,
        'basse': 'pedale', 'volume_basse': .26,
        'echo': {'delai': .42, 'retour': .45, 'melange': .4},
    },
    # Neige : si mineur vers ré majeur, clochettes (sinus qui s'éteint) et écho
    'neige': {
        'tempo': 92, 'onde': 'sinus', 'volume': .26, 'extinction': .9,
        'accords': 'Bm G D A Bm G Em F# | G A D Bm G A Bm Bm',
        'melodie': [
            'F#5 . . . D5 . B4 .', 'G5 . . . D5 . B4 .', 'A5 . . . F#5 . D5 .', 'E5 . . . C#5 . . .',
            'D5 . C#5 . B4 . F#5 .', 'G5 . F#5 . E5 . D5 .', 'E5 . . . G5 . B5 .', 'A#5 . . . . . - -',
            'B5 . . . G5 . D5 .', 'C#6 . . . A5 . E5 .', 'D6 . . . A5 . F#5 .', 'F#5 . . . D5 . . .',
            'G5 . F#5 . E5 . D5 .', 'E5 . F#5 . G5 . A5 .', 'B5 . . . F#5 . . .', 'B4 . . . . . - -',
        ],
        'contre': ['D4 . . . . . . .', 'B3 . . . . . . .', 'F#4 . . . . . . .', 'E4 . . . . . . .', 'D4 . . . . . . .', 'B3 . . . . . . .', 'G4 . . . . . . .', 'F#4 . . . . . . .', 'D4 . . . . . . .', 'E4 . . . . . . .', 'F#4 . . . . . . .', 'D4 . . . . . . .', 'B3 . . . . . . .', 'C#4 . . . . . . .', 'D4 . . . . . . .', 'D4 . . . . . . .'],
        'onde_contre': 'triangle', 'volume_contre': .13, 'attaque_contre': .3,
        'arpege': 'monte', 'octave_arpege': 4, 'onde_arpege': 'triangle', 'volume_arpege': .07,
        'basse': 'fondamentale', 'volume_basse': .25,
        'echo': {'delai': .33, 'retour': .35, 'melange': .3},
    },
    # Intérieurs : valse douce en sol majeur
    'maison': {
        'tempo': 100, 'mesure': 3, 'onde': 'triangle', 'volume': .26, 'vibrato': .15,
        'accords': 'G G C D G Em Am D | G G7 C Cm G D G G',
        'melodie': [
            'D5 . . . B4 .', 'G4 . . . B4 .', 'C5 . E5 . G5 .', 'F#5 . . . . .',
            'G5 . . . D5 .', 'E5 . . . B4 .', 'C5 . A4 . E5 .', 'D5 . . . . .',
            'B4 . D5 . G5 .', 'F5 . . . D5 .', 'E5 . . . C5 .', 'Eb5 . . . C5 .',
            'D5 . . . B4 .', 'A4 . . . F#4 .', 'G4 . . . . .', '- - - - - -',
        ],
        'arpege': 'valse', 'octave_arpege': 4, 'volume_arpege': .05,
        'basse': 'fondamentale', 'volume_basse': .26, 'batterie': 'valse', 'volume_batterie': .5, 'mesures_batterie': 4,
    },
    # Combat : la mineur, rapide
    'combat': {
        'tempo': 156, 'onde': 'carree', 'rapport': .25, 'volume': .12, 'vibrato': .15,
        'accords': 'Am F G E Am F Dm E | F G Em Am F G E E',
        'melodie': [
            'A4 . C5 . E5 . A5 .', 'G5 . F5 . E5 . C5 .', 'D5 . . B4 G4 . B4 .', 'E5 . . . G#4 . B4 .',
            'C5 . A4 . E5 . C5 .', 'F5 . E5 . C5 . F5 .', 'D5 . F5 . A5 . F5 .', 'E5 . . . . . - -',
            'F5 . . E5 F5 . A5 .', 'G5 . . F5 G5 . B5 .', 'E5 . . D5 E5 . G5 .', 'A5 . . . E5 . C5 .',
            'F5 . E5 . D5 . C5 .', 'D5 . C5 . B4 . D5 .', 'E5 . . . G#5 . B5 .', 'E5 . D5 . C5 . B4 .',
        ],
        'arpege': 'monte', 'octave_arpege': 4, 'volume_arpege': .05,
        'basse': 'galop', 'volume_basse': .3, 'batterie': 'combat',
    },
    # Boss : ré mineur, scie, grosse batterie
    'boss': {
        'tempo': 164, 'onde': 'scie', 'volume': .1, 'vibrato': .2,
        'accords': 'Dm Bb C A Dm Bb Gm A | Gm Dm Bb A Gm Dm Bb A',
        'melodie': [
            'D5 . . . A4 . D5 .', 'F5 . . . D5 . F5 .', 'E5 . . . C5 . G5 .', 'E5 . C#5 . A4 . . .',
            'D5 . F5 . A5 . F5 .', 'Bb5 . A5 . F5 . D5 .', 'G5 . F5 . D5 . Bb4 .', 'A4 . . . C#5 . E5 .',
            'G5 . . . Bb5 . . .', 'A5 . . . F5 . . .', 'F5 . G5 . A5 . Bb5 .', 'A5 . . . . . - -',
            'D6 . C6 . Bb5 . G5 .', 'A5 . F5 . D5 . A4 .', 'Bb4 . D5 . F5 . A5 .', 'E5 . C#5 . A4 . E4 .',
        ],
        'contre': ['F4 . . . . . . .', 'D4 . . . . . . .', 'E4 . . . . . . .', 'C#4 . . . . . . .', 'F4 . . . . . . .', 'D4 . . . . . . .', 'D4 . . . . . . .', 'C#4 . . . . . . .', 'D4 . . . . . . .', 'F4 . . . . . . .', 'D4 . . . . . . .', 'C#4 . . . . . . .', 'D4 . . . . . . .', 'F4 . . . . . . .', 'F4 . . . . . . .', 'E4 . . . . . . .'],
        'onde_contre': 'carree', 'volume_contre': .06, 'rapport_contre': .5,
        'arpege': 'monte', 'octave_arpege': 4, 'volume_arpege': .045,
        'basse': 'octaves', 'volume_basse': .32, 'batterie': 'orage',
    },
    # Port d'Écume : chanson de marins en ré majeur, à trois temps
    'port': {
        'tempo': 112, 'mesure': 3, 'onde': 'carree', 'rapport': .5, 'volume': .12, 'vibrato': .18,
        'accords': 'D D G A D Bm Em A | D G D A G A D D',
        'melodie': [
            'A4 . D5 . D5 E5', 'F#5 . E5 . D5 .', 'B4 . . . D5 .', 'C#5 . A4 . . .',
            'A4 . D5 . F#5 .', 'F#5 . E5 . D5 .', 'G5 . . . E5 .', 'A5 . . . . .',
            'A5 . F#5 . D5 .', 'B5 . G5 . D5 .', 'A5 . F#5 . D5 .', 'E5 . . . C#5 .',
            'D5 . E5 . F#5 .', 'E5 . D5 . C#5 .', 'D5 . . . . .', '- - - - - -',
        ],
        'arpege': 'valse', 'octave_arpege': 4, 'volume_arpege': .05,
        'basse': 'quinte', 'volume_basse': .28, 'batterie': 'marin', 'volume_batterie': .8,
    },
    # Récif et Ruines englouties : do mineur sous l'eau, beaucoup d'écho
    'abysses': {
        'tempo': 84, 'onde': 'triangle', 'volume': .24, 'vibrato': .25, 'vitesse_vibrato': 4,
        'accords': 'Cm Ab Bb Gm Cm Ab Fm G | Ab Bb Eb Cm Fm Gm Ab G',
        'melodie': [
            'G4 . . . C5 . Eb5 .', 'C5 . . . Ab4 . . .', 'D5 . . . F5 . Bb4 .', 'D5 . . . . . - -',
            'Eb5 . D5 . C5 . G4 .', 'Ab4 . C5 . Eb5 . C5 .', 'F5 . Eb5 . C5 . Ab4 .', 'B4 . . . . . - -',
            'C5 . . . Eb5 . Ab5 .', 'D5 . . . F5 . Bb5 .', 'G5 . . . Eb5 . Bb4 .', 'C5 . . . . . - -',
            'Ab4 . C5 . F5 . Ab5 .', 'G5 . F5 . D5 . Bb4 .', 'C5 . . . Eb5 . . .', 'D5 . . . B4 . . .',
        ],
        'arpege': 'monte_descend', 'octave_arpege': 3, 'onde_arpege': 'sinus', 'volume_arpege': .12, 'extinction_arpege': .5,
        'basse': 'pedale', 'volume_basse': .26,
        'echo': {'delai': .45, 'retour': .45, 'melange': .38},
    },
    # Tour de l'Orage : sol mineur, tendu
    'orage': {
        'tempo': 132, 'onde': 'carree', 'rapport': .25, 'volume': .12, 'vibrato': .2, 'ondulation': .15,
        'accords': 'Gm Eb F D Gm Eb Cm D | Eb F Gm Gm Cm D Eb D',
        'melodie': [
            'G4 . Bb4 . D5 . G5 .', 'G5 . . . Eb5 . . .', 'F5 . A5 . C6 . A5 .', 'F#5 . . . . . - -',
            'D5 . . G5 Bb5 . A5 .', 'G5 . Eb5 . Bb4 . G4 .', 'C5 . Eb5 . G5 . Eb5 .', 'D5 . . . F#5 . A5 .',
            'Bb5 . . . G5 . Eb5 .', 'A5 . . . F5 . C5 .', 'D5 . G5 . Bb5 . D6 .', 'D6 . . . . . - -',
            'C6 . Bb5 . G5 . Eb5 .', 'D5 . F#5 . A5 . C6 .', 'Bb5 . G5 . Eb5 . Bb4 .', 'A4 . . . F#4 . D4 .',
        ],
        'arpege': 'brise', 'octave_arpege': 4, 'volume_arpege': .045,
        'basse': 'galop', 'volume_basse': .28, 'batterie': 'orage', 'volume_batterie': .85,
        'echo': {'delai': .23, 'retour': .2, 'melange': .15},
    },
    # Dernier combat (Seigneur des Orages) : do mineur, le plus rapide
    'boss_final': {
        'tempo': 170, 'onde': 'scie', 'volume': .11, 'vibrato': .22,
        'accords': 'Cm Ab Fm G Cm Ab Bb G | Ab Bb Cm Cm Ab Bb G G',
        'melodie': [
            'C5 . . G4 C5 . Eb5 .', 'Eb5 . . C5 Eb5 . Ab5 .', 'F5 . . . Ab5 . C6 .', 'B5 . . . G5 . D5 .',
            'G5 . F5 . Eb5 . C5 .', 'Ab5 . G5 . F5 . Eb5 .', 'D5 . F5 . Bb5 . D6 .', 'B5 . . . . . - -',
            'C6 . . . Ab5 . Eb5 .', 'D6 . . . Bb5 . F5 .', 'Eb6 . D6 . C6 . G5 .', 'C6 . . . . . - -',
            'Eb5 . F5 . G5 . Ab5 .', 'Bb5 . Ab5 . G5 . F5 .', 'G5 . . . B5 . D6 .', 'D6 . B5 . G5 . D5 .',
        ],
        'contre': ['Eb4 . . . . . . .', 'C4 . . . . . . .', 'Ab3 . . . . . . .', 'B3 . . . . . . .', 'Eb4 . . . . . . .', 'C4 . . . . . . .', 'D4 . . . . . . .', 'D4 . . . . . . .', 'C4 . . . . . . .', 'D4 . . . . . . .', 'Eb4 . . . . . . .', 'Eb4 . . . . . . .', 'C4 . . . . . . .', 'D4 . . . . . . .', 'D4 . . . . . . .', 'B3 . . . . . . .'],
        'onde_contre': 'carree', 'volume_contre': .07, 'rapport_contre': .5,
        'arpege': 'monte', 'octave_arpege': 4, 'volume_arpege': .05,
        'basse': 'octaves', 'volume_basse': .33, 'batterie': 'orage', 'volume_batterie': 1.1,
    },
    # Île d'Aube (2026-10-20) : mi majeur, lumineux, une harpe et une flûte au lever du soleil
    'aube': {
        'tempo': 100, 'onde': 'triangle', 'volume': .26, 'vibrato': .2,
        'accords': 'E B C#m A E B A B | C#m G#m A E F#m B E E',
        'melodie': [
            'B4 . E5 . G#5 . B5 .', 'A5 . G#5 . F#5 . D#5 .', 'E5 . . . C#5 . E5 .', 'C#5 . . . A4 . . .',
            'G#4 . B4 . E5 . G#5 .', 'F#5 . E5 . D#5 . F#5 .', 'E5 . C#5 . A4 . C#5 .', 'D#5 . . . . . - -',
            'E5 . . G#5 C#6 . B5 .', 'B5 . . . G#5 . D#5 .', 'C#6 . . . A5 . E5 .', 'E5 . . . B4 . . .',
            'A5 . G#5 . F#5 . C#5 .', 'D#5 . E5 . F#5 . A5 .', 'G#5 . . . B4 . . .', 'E5 . . . . . - -',
        ],
        'contre': ['G#4 . . . . . . .', 'F#4 . . . . . . .', 'E4 . . . . . . .', 'E4 . . . . . . .',
                   'G#4 . . . . . . .', 'F#4 . . . . . . .', 'E4 . . . . . . .', 'F#4 . . . . . . .',
                   'E4 . . . . . . .', 'D#4 . . . . . . .', 'E4 . . . . . . .', 'G#4 . . . . . . .',
                   'F#4 . . . . . . .', 'F#4 . . . . . . .', 'G#4 . . . . . . .', 'G#4 . . . . . . .'],
        'onde_contre': 'sinus', 'volume_contre': .1, 'attaque_contre': .25,
        'arpege': 'monte_descend', 'octave_arpege': 3, 'onde_arpege': 'sinus', 'volume_arpege': .12, 'extinction_arpege': .6,
        'basse': 'quinte', 'volume_basse': .26, 'batterie': 'calme', 'volume_batterie': .6,
        'echo': {'delai': .34, 'retour': .3, 'melange': .25},
    },
    # L'île des Échos (2026-10-20) : ré dorien, lent, chaque phrase revient en écho
    'echos': {
        'tempo': 84, 'onde': 'triangle', 'volume': .25, 'vibrato': .25,
        'accords': 'Dm G Dm C Dm G Am Am | F C G Dm F C Dm Dm',
        'melodie': [
            'D5 . F5 . A5 . . .', 'B5 . A5 . G5 . . .', 'A5 . F5 . D5 . . .', 'E5 . . . G5 . E5 .',
            'D5 . F5 . A5 . D6 .', 'B5 . . . A5 . G5 .', 'E5 . . . A5 . . .', 'A4 . . . . . - -',
            'C6 . A5 . F5 . A5 .', 'G5 . E5 . C5 . E5 .', 'D5 . G5 . B5 . D6 .', 'C6 . . . A5 . . .',
            'C6 . . . A5 . F5 .', 'E5 . G5 . C6 . . .', 'A5 . F5 . E5 . . .', 'D5 . . . . . - -',
        ],
        'contre': ['F4 . . . . . . .', 'D4 . . . . . . .', 'F4 . . . . . . .', 'E4 . . . . . . .',
                   'F4 . . . . . . .', 'D4 . . . . . . .', 'E4 . . . . . . .', 'C4 . . . . . . .',
                   'A4 . . . . . . .', 'G4 . . . . . . .', 'B4 . . . . . . .', 'A4 . . . . . . .',
                   'A4 . . . . . . .', 'G4 . . . . . . .', 'F4 . . . . . . .', 'F4 . . . . . . .'],
        'onde_contre': 'sinus', 'volume_contre': .1, 'attaque_contre': .3,
        'arpege': 'monte_descend', 'octave_arpege': 3, 'onde_arpege': 'sinus', 'volume_arpege': .1, 'extinction_arpege': .7,
        'basse': 'quinte', 'volume_basse': .24, 'batterie': 'calme', 'volume_batterie': .5,
        'echo': {'delai': .43, 'retour': .45, 'melange': .38},
    },
    # Plaine des Moulins (2026-10-20) : sol majeur, valse champêtre (flûte, moulins qui tournent)
    'plaine': {
        'tempo': 120, 'mesure': 3, 'onde': 'triangle', 'volume': .25, 'vibrato': .2,
        'accords': 'G D Em C G C D D | C D Bm Em Am D G G',
        'melodie': [
            'D5 . B4 . G4 .', 'A4 . . . F#4 .', 'G4 . B4 . E5 .', 'E5 . D5 . C5 .',
            'B4 . D5 . G5 .', 'G5 . E5 . C5 .', 'D5 . . . A4 .', 'D5 . . . - -',
            'E5 . G5 . E5 .', 'F#5 . A5 . F#5 .', 'D5 . . . B4 .', 'G5 . . . E5 .',
            'C5 . E5 . A5 .', 'F#5 . E5 . D5 .', 'B4 . . . D5 .', 'G4 . . . - -',
        ],
        'arpege': 'valse', 'octave_arpege': 4, 'volume_arpege': .05,
        'basse': 'fondamentale', 'volume_basse': .26, 'batterie': 'valse', 'volume_batterie': .45,
    },
    # Le Vieux Phare (2026-10-20) : la mineur, lent, une lumière qui tourne dans le noir ; beaucoup d'écho
    'phare': {
        'tempo': 76, 'onde': 'sinus', 'volume': .3, 'vibrato': .3,
        'accords': 'Am F Dm E Am F G E | F G Am Am Dm E Am Am',
        'melodie': [
            'A4 . . . C5 . E5 .', 'F5 . . . E5 . C5 .', 'D5 . . . F5 . A5 .', 'G#5 . . . . . - -',
            'E5 . . . A5 . . .', 'A5 . G5 . F5 . . .', 'D5 . . . G5 . B4 .', 'B4 . . . . . - -',
            'C5 . . . A4 . F4 .', 'B4 . . . D5 . G5 .', 'E5 . . . C5 . A4 .', 'A4 . . . . . - -',
            'F5 . . . D5 . A4 .', 'G#4 . . . B4 . E5 .', 'C5 . . . E5 . A5 .', 'A5 . . . . . - -',
        ],
        'arpege': 'brise', 'octave_arpege': 3, 'onde_arpege': 'triangle', 'volume_arpege': .1, 'extinction_arpege': .6,
        'basse': 'pedale', 'volume_basse': .24,
        'echo': {'delai': .5, 'retour': .45, 'melange': .4},
    },
    # Cartes des gardiens (2026-10-20) : fa majeur, un air de taverne qui sautille, pendant les parties de cartes
    'cartes': {
        'tempo': 126, 'onde': 'carree', 'volume': .2, 'vibrato': .1,
        'accords': 'F F C7 C7 F Bb C7 F | Bb Bb F Dm Gm C7 F F',
        'melodie': [
            'C5 . A4 . F4 . A4 .', 'C5 . F5 . A5 . F5 .', 'E5 . G5 . Bb5 . G5 .', 'E5 . C5 . . . - -',
            'A4 . C5 . F5 . A5 .', 'D5 . F5 . Bb5 . F5 .', 'G5 . E5 . C5 . Bb4 .', 'A4 . . . F4 . - -',
            'D5 . F5 . D5 . Bb4 .', 'F5 . D5 . . . F5 .', 'C5 . A4 . C5 . F5 .', 'D5 . A4 . F4 . A4 .',
            'G4 . Bb4 . D5 . G5 .', 'E5 . G5 . C6 . Bb5 .', 'A5 . F5 . C5 . A4 .', 'F4 . . . - - - -',
        ],
        'arpege': 'accords', 'octave_arpege': 4, 'onde_arpege': 'triangle', 'volume_arpege': .06,
        'basse': 'marche', 'volume_basse': .24, 'batterie': 'marche', 'volume_batterie': .4,
    },
    # Combats de l'archipel (2026-10-20) : mi mineur, houle à 6/8 rapide
    'combat_mer': {
        'tempo': 150, 'mesure': 3, 'onde': 'carree', 'rapport': .25, 'volume': .12, 'vibrato': .15,
        'accords': 'Em C D B Em C Am B | C D Em Em Am B Em B',
        'melodie': [
            'E5 . G5 . B5 .', 'C6 . B5 . G5 .', 'A5 . F#5 . D5 .', 'D#5 . . . B4 .',
            'E5 . . . G5 .', 'E5 . C5 . G4 .', 'A4 . C5 . E5 .', 'F#5 . . . D#5 .',
            'G5 . E5 . C5 .', 'A5 . F#5 . D5 .', 'B5 . G5 . E5 .', 'G5 . . . . .',
            'C6 . A5 . E5 .', 'D#5 . F#5 . B5 .', 'G5 . E5 . B4 .', 'D#5 . F#5 . A5 .',
        ],
        'arpege': 'monte_descend', 'octave_arpege': 4, 'volume_arpege': .045,
        'basse': 'quinte', 'volume_basse': .3, 'batterie': 'marin', 'volume_batterie': 1,
    },
    # Combats de l'île d'Aube : la majeur lumineux, héroïque
    'combat_aube': {
        'tempo': 158, 'onde': 'carree', 'rapport': .5, 'volume': .11, 'vibrato': .18,
        'accords': 'A F#m D E A F#m B E | D E C#m F#m D E A A',
        'melodie': [
            'A4 . C#5 . E5 . A5 .', 'F#5 . E5 . C#5 . A4 .', 'D5 . F#5 . A5 . F#5 .', 'E5 . . . G#4 . B4 .',
            'C#5 . E5 . A5 . C#6 .', 'A5 . F#5 . C#5 . F#5 .', 'D#5 . F#5 . B5 . A5 .', 'G#5 . . . . . - -',
            'F#5 . . E5 D5 . F#5 .', 'G#5 . . F#5 E5 . G#5 .', 'E5 . . C#5 G#4 . C#5 .', 'A5 . . . F#5 . C#5 .',
            'D5 . F#5 . A5 . D6 .', 'B5 . G#5 . E5 . B4 .', 'C#5 . E5 . A5 . . .', 'A5 . G#5 . E5 . C#5 .',
        ],
        'arpege': 'monte', 'octave_arpege': 4, 'volume_arpege': .05,
        'basse': 'galop', 'volume_basse': .3, 'batterie': 'combat',
    },
}

# Morceau propre à une carte (identifiant de carte) en plus de ceux des thèmes
LIEUX = {'port': 'port', 'recif': 'abysses', 'ruines': 'abysses', 'tour': 'orage', 'corail': 'port', 'marees': 'abysses', 'epave': 'abysses',
         'aube': 'aube', 'lueurs': 'aube', 'citadelle': 'grotte', 'abime1': 'grotte', 'abime2': 'grotte', 'abime3': 'grotte'}
# Musique des combats ordinaires d'une carte (« combat:<carte> »)
for _c in ('port', 'recif', 'ruines', 'corail', 'marees', 'epave', 'tour'):
    LIEUX['combat:' + _c] = 'combat_mer'
LIEUX.update({'echos': 'echos', 'pierres': 'echos', 'cratere': 'cendres', 'sanctuaire_echos': 'grotte'})
LIEUX.update({'plaine': 'plaine', 'phare1': 'phare', 'phare2': 'phare', 'phare3': 'phare', 'phare4': 'phare', 'phare5': 'phare'})
for _c in ('aube', 'lueurs', 'marais', 'gouffre', 'citadelle', 'abime1', 'abime2', 'abime3', 'echos', 'pierres', 'cratere', 'sanctuaire_echos'):
    LIEUX['combat:' + _c] = 'combat_aube'
