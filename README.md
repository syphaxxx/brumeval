# Brumeval — Les gardiens de la vallée

Un petit RPG en C++ et SDL2. Lior, apprenti gardien, part chasser la brume qui
rend les créatures sauvages. Combats au tour par tour actif à la Final Fantasy 7
(jusqu'à trois combattants, jauges ATB, Limites, magie), créatures à capturer
pour agrandir l'équipe, quatre héros recrutables, deux régions et une grotte.

## Télécharger et jouer

Pas besoin de compiler : les versions prêtes à jouer pour **Windows, Mac et
Linux** sont sur la page
[Releases](https://github.com/syphaxxx/brumeval/releases/latest). Téléchargez le
fichier de votre système, décompressez-le et lancez le jeu ; le fichier
`LISEZ-MOI.txt` du dossier explique tout (y compris les avertissements de
Windows et de macOS, normaux pour un jeu non signé).

## Commandes

| Touche | Action |
|---|---|
| Flèches ou ZQSD | Se déplacer, naviguer dans les menus |
| Entrée ou Espace | Parler, valider, faire défiler les dialogues |
| Échap | Annuler, ouvrir le menu (équipe, tactiques, objets, magie, sauvegarde) |
| Tab (en combat) | Mode auto : les tactiques jouent à votre place, ou vous reprenez la main |
| F11 ou Alt+Entrée | Plein écran ou fenêtre (le choix est gardé pour la prochaine fois). L'image s'adapte à la forme de l'écran ou de la fenêtre, sans bandes noires |

Les touches se changent dans **Options** (écran titre, ou Échap > Options),
avec le volume de la musique et des effets sonores ; les flèches, Entrée et
Échap marchent toujours. **Manette** : branchez-la, même en cours de partie.
Croix ou stick gauche pour se déplacer, **A** valider, **B** annuler,
**Start** menu, **Select** (ou **Y**) mode auto en combat.

## Techniques rapides et lourdes

Chaque technique a un rythme, indiqué dans l'aide en bas de l'écran :

- **rapide** (Lame d'acier, Charge, Jet d'eau…) : moins de dégâts, mais la jauge
  ATB repart à 25 % et le personnage rejoue plus tôt ;
- **lourde** (Taillade éclair, Fracas, Charge héroïque) : un gros coup, mais la
  jauge repart en dessous de zéro (barre rouge) et le personnage rejoue plus
  tard ;
- **normale** : la jauge repart de zéro.

Les techniques faibles ont aussi souvent un effet en plus : Lame d'acier peut
augmenter la vitesse de Lior, Jet d'eau baisser l'attaque de l'ennemi,
Fouet-ronce sa vitesse. Les premières techniques apprises restent ainsi utiles,
par exemple pour achever un ennemi vite ou préparer le combat.

## Tactiques (combat automatique)

Comme les « gambits » de Final Fantasy XII, chaque membre de l'équipe a une
liste de règles « condition → action » (menu Échap > **Tactiques**), par
exemple :

1. Allié : K.O. → Réanimation
2. Allié : PV < 40 % → Meilleur soin
3. Soi : Limite prête → Limite
4. Ennemi : n'importe lequel → Meilleure attaque

En combat, quand la jauge d'un membre est pleine et que le **mode auto** est
actif (touche **Tab**, indiqué « Auto (Tab) » en bas de l'écran), la première
règle possible, de haut en bas, est jouée toute seule. Si aucune ne l'est, le
menu de commande s'ouvre comme d'habitude. Appuyer sur Tab pendant que le menu
attend un ordre laisse les tactiques jouer ce tour.

Les conditions visent un ennemi (le moins de PV, PV < x %, faible à l'action,
sans l'effet, boss…), un allié (PV < x %, K.O., avec un état, PM < x %…) ou le
membre lui-même. Les actions sont automatiques (meilleure attaque, attaque sans
PM, meilleur soin, réanimation, guérison, Limite), une technique ou un sort
précis, ou un objet du sac (potion, éther, lanterne de capture…). Chaque
personnage a 4 lignes au départ, puis une de plus tous les 6 niveaux (10 au
maximum). Une règle grise ne peut pas encore servir (sort pas encore appris),
une règle orange ne marchera pas (par exemple une attaque avec une condition
« Allié »).

## Mode Expédition (roguelite)

Depuis l'écran titre, **Expédition** lance une aventure où tout est généré à
partir d'une **graine** (un nombre, ou un mot via « Graine… ») : les techniques
de chaque type, les créatures, les héros, les dresseurs et les régions. La même
graine redonne exactement le même monde : on peut la noter et la partager.

1. Choisissez votre héros parmi plusieurs (classe, type, statistiques, Limite),
   puis une créature parmi trois.
2. Chaque région a un village (guérisseuse, boutique, habitants qui donnent des
   conseils, parfois un héros à recruter), des herbes à rencontres, des
   dresseurs, des coffres, et un **gardien** qui bloque la route. Une fois
   vaincu, la sortie à l'est mène à la région suivante, plus forte. Les régions
   sont sans fin : allez le plus loin possible.
3. Une défaite termine l'expédition. Elle rapporte des **éclats de brume**
   selon la distance parcourue (et les dresseurs battus), et le record de
   région est gardé.
4. Au **Camp**, les éclats achètent des améliorations permanentes : plus de héros
   et de créatures proposés au départ, or et objets de départ, niveau de
   départ, captures plus faciles, plus d'expérience.

L'expédition se sauvegarde (Échap > Sauvegarder, et à chaque nouvelle région)
dans un fichier à part, `expedition.txt` : la partie principale n'est jamais
touchée.

## Multijoueur : expédition à plusieurs, duels et échanges

Écran titre > **Multijoueur**, jusqu'à **4 joueurs**. Tous doivent avoir **la
même version du jeu** (indiquée en bas à droite de l'écran titre) et les mêmes
données.

1. L'un choisit **Héberger une partie** : le jeu affiche ses adresses.
2. Les autres choisissent **Rejoindre une partie** et tapent une de ces adresses.
3. Dans le salon, l'hôte lance une **expédition** ou un **duel**.

**Expédition à plusieurs.** L'hôte choisit Nouvelle, Graine… ou Continuer :
tout le monde part dans le même monde généré.

- Chacun choisit son héros et sa créature, puis joue **de son côté** : combats
  contre les créatures et les dresseurs, captures, coffres, boutique, son or et
  ses objets. On voit les autres joueurs sur la carte, avec leur pseudo.
- Le **gardien** de chaque région se combat **ensemble** : le premier arrivé
  attend devant lui les joueurs qui ne l'ont pas encore battu (Échap > Groupe
  pour voir où ils en sont). À deux, chacun envoie deux combattants ; à trois
  ou quatre, un chacun. Le gardien est plus robuste quand il y a quatre alliés.
  Chacun commande ses combattants et peut utiliser ses objets.
- Une défaite en solo ramène au village de la région, en perdant la moitié de
  son or. Seule une défaite contre le gardien, à plusieurs, arrête l'expédition
  (avec les éclats de brume gagnés, comme en solo).
- Chacun garde sa sauvegarde (`expedition_groupe_<pseudo>.txt` : gardez le même
  pseudo pour la reprendre). Un ami qui arrive en
  cours de route part avec un nouveau héros dans la région du groupe ; avec
  Continuer, chacun reprend la sienne.

**Duel** (à deux). Chacun combat avec les trois premiers membres de l'équipe de
sa partie principale (ou, sans partie, le héros et deux compagnons de départ).
L'hôte peut mettre tous les combattants au niveau 50 (« Niveaux : égaux »).
Chacun commande son équipe ; Tab active le mode auto (tactiques). Ni objets ni
fuite.

**Échanger des créatures.** Dans le salon (**Échanger**), avec les créatures de
sa partie principale ; pendant une expédition à plusieurs (Échap > Groupe >
**Échanger une créature**), avec celles de l'expédition. On choisit le joueur et
sa créature, puis :

- **Échange** : l'autre choisit la créature qu'il donne en retour, puis on
  accepte ou non ;
- **Cadeau** : l'autre la reçoit sans rien donner (il lui faut une place dans
  son équipe, 8 membres au plus).

La créature garde son niveau, son expérience et ses tactiques ; les héros ne
s'échangent pas. Chaque échange est aussitôt sauvegardé des deux côtés. Pendant
une expédition, on ne peut recevoir une créature que d'une région déjà atteinte.

**Jouer par Internet, chacun chez soi.** Le plus simple : installer tous une
appli gratuite de réseau privé, puis utiliser l'adresse qu'elle donne.

- **Tailscale** (https://tailscale.com) : créez un compte, installez-la sur les
  deux ordinateurs, invitez votre ami dans votre réseau ; l'adresse de l'hôte
  commence par `100.`.
- ou **Radmin VPN** (Windows) : l'un crée un réseau, l'autre le rejoint ;
  l'adresse commence par `26.`.

Sans VPN, l'hôte doit ouvrir le port **47474** (TCP) sur sa box et donner son
adresse publique. La première fois qu'on héberge, Windows demande s'il faut
autoriser Brumeval sur le réseau : cochez **privé et public** puis
**Autoriser**. Sur le même Wi-Fi, l'adresse locale (`192.168.…`) suffit.

## Installer et lancer sous Windows avec VS Code

1. Installez les outils (une seule fois) :
   - **Git** : https://git-scm.com
   - **CMake** : https://cmake.org/download (cochez « Add CMake to the PATH »)
   - **Build Tools pour Visual Studio** (le compilateur C++ de Microsoft) :
     https://visualstudio.microsoft.com/fr/downloads/ , section « Outils pour
     Visual Studio ». À l'installation, cochez la charge de travail
     **Développement Desktop en C++**.
   - Dans VS Code, les extensions **C/C++** et **CMake Tools** (VS Code les
     propose automatiquement à l'ouverture du projet).
2. Récupérez le projet :
   ```
   git clone https://github.com/syphaxxx/brumeval
   ```
3. Dans VS Code : **Fichier > Ouvrir le dossier…** et choisissez `brumeval`.
   CMake Tools vous demande un kit de compilation : choisissez celui de
   Visual Studio (amd64). La première configuration télécharge SDL2 : il faut
   Internet et quelques minutes.
4. **F7** pour compiler, puis **Maj+F5** pour lancer le jeu (ou le bouton ▶ de
   la barre d'état en bas).

## Outils

Depuis l'écran titre, **Outils** donne accès à :

- **Arène de combat** : composez deux équipes (espèces, niveaux, PV des boss,
  renforts, décor), puis **Combattre !** pour jouer vous-même ou **Simuler**
  pour faire jouer des dizaines de combats à l'ordinateur et voir le taux de
  victoire. **Modèles…** reprend tous les combats de l'histoire et les zones de
  rencontres ; **Journal** détaille le dernier combat coup par coup. Chaque
  allié a ses **Tactiques…**, et l'option **Alliés** choisit si les
  simulations suivent les tactiques ou l'IA de l'ordinateur.
- **Réglages** : modifiez depuis le jeu les règles (départ, rencontres, formules
  de combat, états, récompenses, butin, tactiques de départ et nombre de
  lignes), les espèces (types, statistiques,
  résistances, immunités, techniques apprises), les techniques, la table des
  types et les objets. Les changements s'appliquent tout de suite : essayez-les
  avec **Tester dans l'Arène**, puis **Enregistrer** (ou **Tout annuler**).
  Gauche/droite règle une valeur, Entrée ouvre un texte à saisir au clavier.
- **Éditeur de cartes** : peignez les tuiles à la souris (clic gauche, clic
  droit = pipette, molette = tuile suivante) ou au clavier (flèches + Entrée,
  Maj + flèches pour peindre en marchant), avec pinceau, rectangle ou
  remplissage. **Tab** passe aux calques Objets (habitants, coffres, panneaux,
  passages, boss, bâtiments) et Zones (rencontres, zones déclencheuses).
  **Échap** ouvre le menu : propriétés et taille de la carte, nouvelle carte,
  vérification, **Tester ici** (Échap > Fin du test), enregistrer
  (Ctrl+S). Ctrl+Z / Ctrl+Y annulent et rétablissent.

- **Éditeur d'histoire** : tous les événements (dialogues, scènes, boss,
  dresseurs, boutiques…). Un événement est une liste d'actions (dire, donner,
  combat, question, si…) ou plusieurs **pages** avec une condition (drapeau,
  objet, or, membre de l'équipe). Les actions qui en contiennent d'autres
  (réponses Oui/Non, victoire/défaite d'un combat, alors/sinon) s'ouvrent comme
  des sous-listes. **Jouer l'événement** le lance à l'endroit où il est utilisé,
  avec une équipe et des drapeaux de test ; un combat peut s'essayer dans
  l'Arène. Ctrl+Z / Ctrl+Y / Ctrl+S comme dans l'éditeur de cartes.

Les menus se manient aussi à la souris : survol, clic gauche pour valider, clic
droit pour revenir, molette pour défiler ou régler une valeur.

## Continuer le projet avec Claude Code

Ouvrez un terminal dans le dossier du projet (dans VS Code : **Terminal >
Nouveau terminal**) et lancez `claude`. Le fichier `CLAUDE.md` lui donne tout le
contexte : architecture, commandes, état du projet et idées pour la suite.

## Tester

```
build\Debug\brumeval.exe --test captures
```

(ou `build\brumeval.exe --test captures`, selon la configuration de CMake)

Le mode test vérifie les données, joue quelques scènes, simule des combats pour
l'équilibrage et enregistre des captures d'écran dans `captures/`.

## Organisation

- `src/` : le code du jeu (voir `CLAUDE.md` pour le rôle de chaque fichier)
- `data/` : tout le contenu du jeu (créatures, techniques, objets, cartes,
  dialogues, règles) en fichiers JSON modifiables, lus à chaque démarrage. Voir
  `data/LISEZMOI.md`.
- La sauvegarde est écrite dans `%APPDATA%\Brumeval\Brumeval\sauvegarde.txt`,
  le choix plein écran / fenêtre dans `options.txt` à côté
  (le mode test utilise `sauvegarde_test.txt` et l'efface à la fin)
