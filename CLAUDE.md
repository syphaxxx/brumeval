# Brumeval — contexte pour Claude Code

Petit RPG 2D en C++17 avec SDL2, inspiré de Final Fantasy 7 (combat ATB à trois
combattants, Limites, magie) et de Pokémon (capture de créatures). Tout le contenu
est original. Le projet a été démarré dans l'app Claude puis transféré ici.

Voir aussi `docs/GUIDE.md` (vue d'ensemble, recettes pour ajouter du contenu,
pièges) et `docs/PLANNING.md` (la suite du projet).

## L'utilisateur

- Il est francophone et débute en programmation : réponds en français, explique
  simplement ce que tu changes et pourquoi, sans jargon inutile.
- Il travaille sous **Windows avec Visual Studio Code** (extensions CMake Tools et
  C/C++). Le code a été écrit sous Linux avec GCC, puis compilé et testé sous
  Windows avec MSVC (Build Tools 2022) : tous les tests passent.
- Tous les textes visibles par le joueur sont écrits en français, avec accents, et traduits en anglais
  (`data/langues/en.json`, voir Langue plus bas) : tout nouveau texte passe par `T("…")` dans le code, puis
  `python outils/textes.py` et sa traduction.

## Compiler et lancer

```bash
cmake -S . -B build              # la première fois, télécharge et compile SDL2 (Internet requis)
cmake --build build
./build/brumeval                 # Windows : build\Debug\brumeval.exe (selon le générateur)
```

Dans VS Code : CMake Tools configure le projet à l'ouverture ; F7 compile,
Maj+F5 lance sans débogueur. `.vscode/launch.json` contient une configuration
de débogage MSVC (`cppvsdbg`) et une GDB.

## Publier une version pour les joueurs

Procédure complète : compétence `publier-version` (`.claude/skills/`). Augmenter `BRUMEVAL_VERSION`
(`src/version.hpp`) à chaque version publiée. L'adresse du webhook Discord est dans le secret GitHub
`DISCORD_WEBHOOK` : ne jamais l'écrire dans le code ni dans la mémoire, le dépôt est public.

## Tester (à faire après chaque modification)

```bash
./build/brumeval --test captures
```

Le mode test (src/test.cpp) vérifie les données, joue des scènes (titre, village,
dialogue, recrutement, combat piloté par les menus, sauvegarde/chargement),
enregistre des captures `.bmp` dans `captures/` et, avec `BRUMEVAL_SIMULATIONS=1`
(2026-10-24 : sinon elles sont sautées, elles prenaient 6 des 7 minutes), simule des
combats pour l'équilibrage. Il doit finir par « TOUS LES TESTS PASSENT ». Les tirages
au hasard partent d'une graine fixe (`BRUMEVAL_GRAINE` pour en changer) : un échec se rejoue. Regarde les captures
après un changement visuel. Il tourne sans fenêtre (rendu logiciel).

Chaque image dessinée pendant les tests est vérifiée (`Gfx::checkLayout`,
gfx.cpp) : texte qui sort de sa fenêtre, textes qui se chevauchent, fenêtre
posée de travers sur une autre ou hors de l'écran, libellé de menu coupé. Les
problèmes sont listés avec la dernière capture (« après 05_equipe : … »). Le
« tour des menus » de test.cpp ouvre en plus la plupart des menus avec les noms
les plus longs : y ajouter tout nouvel écran.

### Joueur automatique (à lancer après un changement d'histoire, de cartes ou d'équilibrage)

```bash
cmake --build build-release --config Release
build-release/Release/brumeval --partie partie   # toute l'histoire, sans fenêtre, ~10 s
brumeval --demo                                  # la même chose dans la fenêtre (aussi Outils > Démo) ; Espace : pause, Échap : quitter
```

Fonctionnement, rapport et mémoire d'une partie à l'autre : compétence `joueur-auto-brumeval`. En démo, il passe par les menus
comme un joueur (équipement, gemmes, tactiques, formation, compagnons et réserve selon les créatures du lieu
(`Pilot::adaptTeam`, 2026-10-24), Journal, Bestiaire : `Pilot::visit`) ; `BRUMEVAL_DEMO=1 brumeval
--partie dossier` joue la démo sans fenêtre (capture de chaque menu visité). Un
nouveau genre d'interaction (objet à utiliser, énigme…) demande d'ajouter un objectif dans `Pilot::goals`.

## Revue par Codex (relecture seule)

Claude reste le développeur principal : lui seul écrit et modifie le code. OpenAI Codex sert de
relecteur indépendant : il analyse, signale les problèmes et propose des corrections, sans jamais
modifier de fichier.

**Quand** : de lui-même, sans attendre qu'on le demande, pour une nouvelle fonctionnalité importante,
une refactorisation, un bug complexe, ou un changement du combat ou de l'équilibrage, du réseau, du
format de sauvegarde ou de procgen. Pas pour une petite correction, un texte ou des données.

**Combien** : une seule revue par tâche. Une seconde revue seulement si la première a mené à des
corrections importantes, et jamais plus. Jamais de boucle entre les deux IA : Codex ne relance pas
Claude, et Claude ne renvoie pas les réponses de Codex à Codex.

**Commande** :

    git diff | codex.cmd exec --sandbox read-only --ephemeral --color never -C <dépôt> -o <scratchpad>/codex_avis.txt "<consigne>"

- Toujours `--sandbox read-only` : jamais `workspace-write`, `--full-auto` ni `--dangerously-…`.
- La consigne donne à Codex le but du changement, la liste des fichiers concernés et les pièges de
  CLAUDE.md qui s'y appliquent (par exemple : procgen identique sur tous les ordinateurs, `T("…")`,
  sauvegarde, `BRUMEVAL_VERSION`). Elle demande une liste de problèmes concrets (fichier:ligne,
  scénario qui casse, correction proposée), et lui dit de lire les fichiers avec
  `Get-Content -Encoding UTF8`.
- Ne jamais envoyer de secret à Codex (webhook Discord, fichiers `.env`).
- Si Codex échoue (erreur, quota atteint, délai dépassé) : prévenir l'utilisateur, ne pas réessayer
  en boucle, et continuer la tâche sans revue.

**Après la revue** :
1. Vérifier chaque remarque dans le code. Corriger celles qui sont justes, écarter les autres.
2. Lancer les tests (`--test captures`, et le joueur automatique si l'histoire, les cartes ou
   l'équilibrage ont changé).
3. Vérifier que `git status` ne montre que les changements de Claude.
4. Présenter à l'utilisateur un résumé : le travail fait, les remarques de Codex retenues ou écartées
   (avec la raison), et le résultat des tests.

Demander à l'utilisateur avant toute suppression, toute modification massive ou toute opération
irréversible, même quand l'idée vient de Codex.

## Architecture (src/)

Chaque `.hpp` commence par un commentaire qui explique son module (le `.cpp` du même nom contient le
code) ; `docs/GUIDE.md` les range par domaine.

### Principes à connaître

- Résolution logique 320x240, élargie à la forme de l'écran : l'image fait
  240 pixels de haut et `Gfx::fullW` de large (320 à 576 ; 384 en 16:10,
  428 en 16:9). Tout le code dessine dans la zone de 320x240 du milieu
  (`Gfx` ajoute le décalage `ox` dans `span`, `rect` et `textBig`) : menus,
  fenêtres et combattants ne bougent pas. Un décor qui doit remplir tout
  l'écran va de `g.left()` (négatif) à `g.right()` ; la carte, les combats,
  l'écran titre, la fin et les outils le font. Tout nouveau décor plein écran
  doit utiliser `g.left()`/`g.fullW` au lieu de 0/`SCREEN_W`.
- `Screen` (main.cpp) choisit cette largeur selon la fenêtre. Depuis la 3.3,
  l'image est dessinée directement k fois plus grande (k entier, de 2 à 6, juste
  au-dessus de la taille de l'écran ; `SDL_RenderSetScale`), puis ajustée en
  douceur à la fenêtre : le code dessine toujours en pixels de 320x240 (pixels
  carrés et nets), les sprites, deux fois plus fins, gardent leurs détails. Si la
  carte graphique refuse la grande image, k descend (jusqu'à dessiner dans la
  fenêtre). Le mode test dessine en 640x480 (captures deux fois plus grandes). La souris est ramenée en
  coordonnées de la zone du milieu par `Screen::toGame` (négatives à gauche).
  Le jeu tient compte du zoom de Windows (`SDL_HINT_WINDOWS_DPI_AWARENESS`) ; la
  fenêtre de départ a la forme de l'écran (`fitWindow`). Lancé par un
  double-clic, il ferme la console noire (`FreeConsole`).
- `BRUMEVAL_LARGEUR=384 brumeval --test dossier` prend les captures du mode
  test au format large (à vérifier après un changement de décor).
- Lancé depuis l'application Claude (application empaquetée), le jeu lit et
  écrit `%APPDATA%` dans un dossier privé de Claude
  (`%LOCALAPPDATA%\Packages\Claude_…\LocalCache\Roaming\`) : sauvegarde et
  options y sont séparées de celles du jeu lancé par un double-clic ou VS Code.
  Pour lancer le jeu pour l'utilisateur : `Start-Process explorer.exe
  -ArgumentList <chemin de brumeval.exe>`.
- `Script` : `say(texte, auto)` affiche un message (auto = 0 attend Entrée),
  `call(fn)` exécute du code, `wait(s)` fait une pause. Les étapes ajoutées
  *pendant* un `call` sont insérées juste après lui : c'est ce qui enchaîne les
  scènes et les tours de combat.
- `MenuStack` : pile de menus ; chaque `MenuItem` a un libellé, une valeur à
  droite, un texte d'aide, une action et un rappel `hover` (ex. curseur de cible),
  et une icône facultative `icon` (un sprite, ou un dessin de `menuGlyph` dans
  ui.cpp : patte, damier, viser, parchemin, bulle, carte, engrenage, aide, porte,
  pas, sablier, echange, duo… ; « #rrggbb » : une pastille de cette couleur, `typeDot` pour les techniques), posée par `setIcons(m, {{T("Équipe"), "ic_casque_1"}…})` : menu de
  pause, commandes du combat, écran titre ; le libellé passe 11 px plus à droite. `MenuItem::sep` : filet au-dessus
  (début d'un groupe) ; `MenuItem::notches` : un réglage de 0 à 10 dessiné en crans (volumes des Options). `MenuItem::bar` (0 à 1) : barre
  d'avancement sous la ligne (Progression). `BRUMEVAL_CAPTURES_TOUR=1 brumeval --test dossier` : une capture `tour_NN` par
  écran du tour des menus (pour les revoir tous). Tactiques d'un membre : les lignes libres et verrouillées tiennent
  chacune en une ligne (« + Nouvelle règle »).
  Un menu du dessous n'est pas dessiné si un sous-menu le chevauche sans être
  bien à l'intérieur (4 px de marge). Un libellé trop long est coupé avec « … »
  (signalé par le mode test, sauf `MenuItem::shrink`). Les panneaux à droite
  (Réglages, Arène) se cachent quand `menus.maxRight()` dépasse 158. Une
  fenêtre modale sur fond assombri appelle `g.newLayer()`. Largeur d'un
  caractère : 6 px ; un libellé commence 13 px après le bord du menu.
- Équipement : `Fighter::gear` (arme, armure, accessoire : identifiants d'objets
  avec `ItemDef::slot` et `bonus`), ajouté aux statistiques par
  `Fighter::recalc` ; `canEquip` (armes et armures : humains seulement). Menu
  `Game::gearMenu` > `gearSlots` > `gearPick` ; ligne `equipement` après
  `membre` dans la sauvegarde ; un échange emporte l'équipement de la créature.
- Compagnons (décision du 2026-10-09) : chaque héros combat avec une créature
  (`Fighter::companion`, faible lien vers une créature de l'équipe ; ligne
  `compagnon <rang>` après le héros dans la sauvegarde). `frontOf` (data.cpp)
  choisit les combattants : les 3 premiers héros valides, chacun suivi de son
  compagnon (choisi, sinon la première créature libre) ; une équipe sans héros
  combat comme avant. En combat (`Battle::companion`, `hasHero_`), le joueur
  commande le compagnon comme un héros (demandé le 2026-10-10 ; avant, il
  agissait seul) : ses tactiques en mode auto, sinon son menu ; sa jauge ATB
  va à `compagnon_atb` (0,5) ; jusqu'à 6 alliés sur le damier ; « Changer »
  remplace un héros par un héros. Menu
  `Game::companionsMenu`. Duel : les combattants gardent l'ordre envoyé
  (`s.allies = mine`) ; gardien à plusieurs : `myFighters` suit `front()`
  (un héros et son compagnon à deux joueurs).
- Équipe : `Game::team` (8 membres maximum par capture, les humains s'ajoutent
  toujours). Les combattants sont choisis par `Game::front()` (`frontOf` : héros
  et compagnons, voir Compagnons ci-dessus).
- Réserve (2026-10-10) : `Game::reserve`, les créatures en plus de l'équipe. Une
  capture (battle.cpp) ou un cadeau (`commitTrade`) avec l'équipe complète y va.
  Menu `Game::reserveMenu` (déposer, reprendre, échanger) ; `swapReserve(out,
  in)` : le héros dont le compagnon part prend celui qui arrive. Sauvegarde :
  ligne `reserve`, puis les lignes `membre` de la réserve (`Online::myTeam`
  s'arrête à `reserve`). En combat, « Changer » parle de **remplaçants**
  (membres de l'équipe hors combat) : ne pas confondre avec la réserve.
- Bestiaire : `Game::seen` / `caught` (espèces de créatures), lignes `vu` et
  `pris` de la sauvegarde ; `see()` dans `startBattle` et à l'entrée d'un
  renfort, `caught` à la capture, `noteBestiary()` compte l'équipe et la
  réserve. Menu `bestiaryMenu`, fiche `drawBestiaryPanel` (`panelMode_` 3 ; chaque coup d'un allié apprend l'effet de son type sur l'espèce, résistance
  comprise, et des coups physiques / magiques : clés `<espèce>:<type>`, `<espèce>:phys`, `:mag` de `Game::weak`, 2026-10-25 ; une fois l'espèce obtenue, sa
  `description` : `Species::desc`, écrites par `outils/villes/descriptions.py`). Page tactique (2026-10-25, Gauche/Droite
  ou un clic sur la fiche, `bestiaryPage_`, interceptés dans `Game::update` avant `menus.update`) : rôle et ce qu'il
  change, faiblesses découvertes (`knownMatchup`, battle.hpp : seulement `Game::weak` ; « ? » / « +? » pour le reste),
  absorptions découvertes, résistances et coups physiques/magiques (talent compris) une fois l'espèce obtenue (sinon
  « ? » : rien ne les enregistre en combat), attaques de zone apprises (petit damier 3x3 de la forme). Lieux : zones
  de jour et de nuit. L'Expédition repart d'un bestiaire vide.
- Rééquilibrage (2026-10-23) : `Game::encounter` : au moins `par_allie` ennemis par combattant allié (0,6 depuis le
  2026-10-24), meutes d'au plus 2 + une créature par tranche de `meute_par_niveau` (15) niveaux de l'équipe (2026-10-24 :
  les débuts se jouent à deux contre deux), créatures sauvages au plus `ecart_niveau` (3) sous la moyenne de l'équipe (sans dépasser la zone de `plafond_niveau`), gains de leur niveau
  d'origine (`Fighter::rewardLvl`) ; jamais en Expédition. Expérience : un adversaire plus de `exp_marge` niveaux sous
  l'équipe rapporte `exp_ecart` de moins par niveau (au moins `exp_min`), pas en Expédition ; un coup absorbé ou sans
  effet n'applique ni état ni poussée ; mesures : compétence `equilibrage`. Embuscade (`rencontres.embuscade`,
  `BattleSetup::ambush`, `Battle::ambush_`) : un tiers des créatures (au moins une) part du fond du camp allié, tournée vers l'équipe,
  sans Percée ; astuce « embuscade », aide de l'échiquier. `attackScore` : ×1,4 pour frapper un adversaire au contact dans
  son dos (se retourner) ; le joueur automatique répète une embuscade comme telle (`fight["embuscade"]`). Résistance négative d'une espèce (`resistances`) = elle
  absorbe ce type (`applyHit` soigne, « Absorbe ! ») ; résistances des créatures sauvages : `outils/villes/resistances.py`.
  Meutes (2026-10-24) : une créature sauvage n'apprend jamais d'attaque sur tous les ennemis (`outils/villes/meutes.py` la
  remplace par une attaque de zone d'un de ses types, sinon la retire ; les boss gardent les leurs).
- Dur mais tactique (2026-10-24, branche `jeu-tactique`). **Garde** : « Attendre » (`waitTurn`) met le combattant en
  garde (`Battle::guarding_`) : dégâts reçus × `combat.garde` (0,75) et riposte à tout coup au corps à corps (chance
  `combat.garde_riposte`, 1 ; même code que la gemme de riposte, ×0,7) jusqu'à ce que sa jauge soit pleine (`tickATB`) ;
  petit bouclier à côté de la tête (à gauche de la barre de PV chez un ennemi) ; astuce « garde ». **Intentions** (Into
  the Breach) : à `combat.intention` % (50 ; 70 au départ : trop tard, le menu du joueur s'ouvrait sans elles) de sa jauge, un ennemi (un boss aussi, sauf quand sa zone rouge attend
  (`announced`) ; ni brisé, ni en duel, ni chez l'invité) annonce ce qu'il fera (`declareIntent` : `planMove`, puis `think` depuis sa future case ; un
  soigneur annonce le soin que `roleHeal` jouerait ; rangé dans `intents_`) : icône à droite de sa barre de PV (épée,
  orbe de la couleur du type, croix verte pour un soin, sablier pour une garde), chevron orange au-dessus de chaque allié
  visé avec les dégâts prévus « ≈ lo-hi » (`preview`, la formule de `damageText`, additionnés), zone en orange au sol, « Prépare X sur Y » dans l'aide de la cible (`pickFoe`, `intentText`). À son tour
  (`enemyTurn`), il l'exécute si elle tient encore (case encore accessible, cibles encore au combat, portée, PM, soin
  encore utile ; listes « tout un camp » refaites), sinon « Déjoué ! » et il réfléchit à nouveau ; effacée par la Brisure
  (`breakFoe`). Réseau : la garde est le 11e nombre de chaque combattant de `etat`, les intentions la liste `i`
  (ennemi, technique, case, alliés visés) : les autres joueurs du gardien les voient comme le chef. **Ciblage** : quand un ennemi frappe « au hasard » (65 % des tours),
  la cible est tirée selon `lure` (soigneur ou soins connus +1, lanceur de sorts +0,5, blessé jusqu'à +1,5, +0,75 par
  intention déjà annoncée sur lui : la meute s'acharne) : couvrir ses soigneurs et ses blessés compte. Ennemi survolé
  au choix de la cible : un pointillé orange vers l'allié qu'il vise. **Menace** : `dangerAt(f, case)` réunit terrain qui blesse, zone rouge d'un boss et
  intention (visé au corps à corps : les cases hors de portée de l'ennemi sont sûres ; voisin visé par une zone : s'en
  écarter) ; `threatened`, `safeCell`, l'IA alliée et « Se mettre à l'abri » s'en servent ; `aimedBy(f)` compte les intentions
  qui le visent (meute, tactiques, ATH : bouclier de garde et chevron après le nom) ; frise de l'ordre des tours : un
  chevron orange sur le prochain tour d'un ennemi qui a annoncé une attaque (`drawTurnOrder`). **IA plus tactique** (2026-10-25) :
  un ennemi ne frappe pas au corps à corps un allié en garde (`attackScore` ×0,4 chez les ennemis, et `think` cherche une
  autre cible : il riposterait) ; un protecteur ennemi va se placer en croix d'un blessé des siens (`posValue` : +30 × PV
  perdus sous la moitié) et son intention le dit (« … (couvre X) », `intentText`) ; un soigneur ennemi (`soigneur`, soin
  connu) garde ses distances comme un lanceur de sorts (`posValue`). L'intention affichée reste celle qu'il exécute. Sons `garde`, `dejoue`, `vise` (une intention vise un allié ; au plus un par demi-seconde,
  `aimSoundT_`), `riposte`, `bouclier` (un allié en garde encaisse) et `brisure` (`breakFoe`) (data/sons.json). Astuces « intention » et « garde », aide de l'échiquier, capture `garde_intention`, mesures :
  compétence `equilibrage`.
  À l'œil (2026-10-25, dessin seul) : bouclier qui se déplie (`guardT_`) et qui encaisse, élan de la riposte (`bursts_`),
  intention qui pulse à l'exécution ou se brise si déjouée (`intentFx_`) ; capture `garde_effets`.
- Vente : boutique > « Vendre… » (`Game::sellMenu`), moitié du prix, sauf les
  objets importants.
- Boutique (2026-10-23) : un équipement survolé montre sous la liste ce qu'il changerait pour chaque membre qui peut le
  porter (`panelMode_` 5, `shopHover_`, `drawShopPanel`). Ordinateur : `posValue` craint davantage un adversaire qui
  pourrait le rejoindre par derrière (d'après le regard qu'il aura).
- Progression : `Game::flags` (`boss1`, `golem`, `boss2`, `maelle`, `brann`,
  `isra`, `pecheur`, `coffre:<carte>:<index>`).
- Sauvegarde : fichier texte `sauvegarde.txt` dans `SDL_GetPrefPath("Brumeval",
  "Brumeval")` (sous Windows : `%APPDATA%\Brumeval\Brumeval\`). Chaque ligne
  `membre` est suivie de `tactiques` (interrupteur) et des lignes `tactique` du
  membre ; `auto` garde le mode auto. Le mode test écrit dans
  `sauvegarde_test.txt` (`saveName_`) pour ne jamais toucher à la vraie partie.
  Sauvegarde automatique (2026-10-20, `Game::autoSave`) : à chaque passage et porte, si `options().autosave`
  (ligne `sauvegarde_auto` d'`options.txt`, Options > Sauvegarde auto) ; jamais en Expédition, en ligne, en démo ni
  en mode test (`saveName_` différent) ; « Sauvegarde… » un instant en bas à droite. Nouvelle partie avec une partie
  sauvegardée : confirmation d'abord.
- Expédition (expedition.hpp, procgen.hpp) : tout le contenu est généré depuis
  une graine (`procgen::generateBase`, puis `generateRegion(graine, k, contenu)`
  région par région : la région k ne dépend que de la graine et de k). Le
  contenu prend la place des données en mémoire (`dataDoc` + `rebuildData`,
  `maps()` = la seule région courante, `events()`), avec les types et objets du
  jeu ; `Game::titleMenu` appelle `Expedition::leave` qui remet tout en place.
  Identifiants générés : `x_<type>_p1`… (techniques), `xh1`… (héros), `xs1`…
  (créatures de départ), `x<k>c1`, `x<k>b` (gardien), `x<k>t1` (dresseurs),
  `x<k>r` (recrue), `lk_…` (apparences), `expedition_<k>` (carte). La sortie
  (derrière le gardien) est testée dans `Game::arrive` ; `Game::defeat` finit
  l'expédition. Sauvegarde : `expedition.txt` (ligne `expedition <graine>
  <région>` en premier : le chargement régénère le monde), progression du Camp
  dans `expedition_progres.txt` ; le mode test utilise des fichiers `_test`.
  Changer le générateur change le monde des graines déjà jouées (et donc les
  sauvegardes d'expédition en cours).
  À plusieurs, voir « Expédition à plusieurs » plus bas.
  `BRUMEVAL_MONDE=brume brumeval --test captures` affiche le monde de la graine
  « brume » (héros, créatures, gardiens et techniques des trois premières régions).
- Multijoueur (net, online, coop, trade, upnp) : protocole, duel, expédition à plusieurs, échanges,
  code de partie → compétence `multijoueur`. Augmenter `BRUMEVAL_VERSION` si les messages changent.
- Le monde généré (Expédition) doit être identique sur tous les ordinateurs : `-ffp-contract=off` (GCC,
  Clang), aucun `sin`/`pow`/`std::shuffle`/`unordered_map` dans procgen.cpp,
  et jamais deux tirages au hasard dans les arguments d'un même appel (Clang
  les fait de gauche à droite, MSVC et GCC de droite à gauche) ;
  le mode test compare l'empreinte de la graine 2026 à une valeur fixe (à
  mettre à jour si on change le générateur).
- Tactiques (tactics.hpp) : `Fighter::tactics` (liste de `Tactic` : condition,
  seuil, action automatique / technique / objet), `Fighter::tacticsOn`,
  `Game::tacticsAuto` (touche Tab en combat). Quand la jauge d'un allié est
  pleine, `Battle::tacticTurn` joue la première règle possible
  (`Battle::tacticPlan` choisit la cible et la technique) ; sinon le menu de
  commande s'ouvre. Ajouter une condition : l'entrée dans `CONDS`
  (tactics.cpp) et son test dans `holds` (battle.cpp). Conditions du 2026-10-23 : `ennemi_brise`, `ennemi_dos`,
  `ennemi_role`, `soi_dos_menace` ; (2026-10-24) `soi_vise` : une intention ennemie le vise, hors zone rouge (le profil
  Protecteur se met alors en garde : riposte), `allie_vise` : un allié visé (le Protecteur va le couvrir : `proteger`) ; actions `briser` (une faiblesse découverte), `garde` (attendre, `Plan::wait`) et
  `attaque_prudente` (la meilleure attaque qui laisse 40 % des PM, sinon une technique gratuite : le profil Soigneur).
  Profils (`tactiques.profils` de regles.json, `Rules::tacticProfiles`, « Profils… » dans l'éditeur, `profileRules(p, lignes,
  combattant)` : sans les règles de soin qu'il ne peut pas suivre, coupés à ses lignes en gardant la dernière règle) ;
  chaque profil réanime et soigne un allié en danger avant d'attaquer (2026-10-24 : avec un seul soigneur, l'équipe du
  joueur automatique perdait contre les meutes de fin de partie) ; `bestProfile(f)` : le profil conseillé (Soigneur,
  Protecteur, Briseur ou Combattant), marqué « (conseillé) » dans le menu et choisi par le joueur automatique
  (`Pilot::setupTactics`). Les simulations (`autoPlay`) n'utilisent les tactiques que si `simTactics` est vrai.
- Son (audio.hpp) : `audio::play("coup")` joue un bruitage, `audio::music("combat")`
  change de morceau (fondu ; le même morceau continue). `Game::updateMusic`
  choisit la musique à chaque image (lieu selon `themeName`, combat, boss,
  silence pendant la fanfare de fin). En combat, passer par `Battle::sound`
  (seulement le combat affiché, pas les simulations ; l'hôte ou le chef envoie
  `son` aux autres). Les menus font leurs bruits eux-mêmes (`MenuStack::update`).
  Le mode test n'ouvre pas de sortie son mais fabrique chaque morceau et chaque
  effet (`audio::render`) pour vérifier qu'ils s'entendent. Musiques (2026-10-19) : écrites par
  `outils/musique/compose.py` d'après `outils/musique/morceaux.py` (20 morceaux, dont `echos` pour l'acte 4, `plaine`, `phare` et `cartes` (parties de cartes) ; chacun : grille d'accords, mélodie,
  contre-chant, arpège, basse et batterie fabriqués d'après les accords) ; voix avec `vibrato`, `attaque`,
  `ondulation`, morceau avec `echo` (ligne à retard du `Mixer`) ; `lieux` accepte un identifiant de carte
  (`Game::updateMusic` : carte, puis thème ; `combat:<carte>` donne la musique des combats ordinaires de cette carte : `combat_mer` dans l'archipel, `combat_aube` sur Aube) ; `BattleSetup::music` (« musique » de l'action combat, vérifiée par
  `checkEvents`).
- Évolution (2.2, plan validé le 2026-10-12) : `Species::evolveTo` / `evolveLvl` (champ `evolution` : `espece`, `niveau`) et
  `size` (`taille`, dessin agrandi dans `drawCreature`) ; `canEvolve` / `evolve` (data.cpp : même niveau, équipement,
  tactiques, case, compagnon ; PV et PM en proportion). Après un combat gagné (hors Expédition, Arène, duel),
  `Game::update` file `offerEvolutions` après la suite de l'histoire : « Évoluer ? » Oui/Non pour chaque créature prête ;
  Non pose `Fighter::evolveLater` (ligne `evolution_plus_tard` de la sauvegarde) et l'entrée Échap > Évolution apparaît.
  22 lignées : 16 nouvelles espèces (statistiques ×1,18, couleurs décalées, `taille` 1,15, techniques de la base + 2,
  Limite `lim_<id>` +10 de puissance) et 6 créatures existantes devenues des évolutions (Rocaillou → Galetor…). Formes
  évoluées sauvages : Pics, Temple, Source. Le mode test coupe les propositions (`offerEvolve_`) sauf dans leur test ;
  ses simulations font évoluer les équipes (`BRUMEVAL_SANS_EVOLUTION=1` pour comparer). Le joueur automatique répond Oui
  (journal : « Évolution : … »). Réglage : statistiques ×1,18 rendaient les boss bien plus faciles (Ignarok 6 → 37 %,
  Givrecorne 33 → 78 %) ; ramenées à ×1,10 (Limite +5), puis boss relevés de niveau (plus de PV ne changeait presque
  rien) : Golem 24 → 25, gardien de cristal 37 → 39, Givrecorne 36 → 39, Mère des Brumes 40 → 44. Mesuré
  (BRUMEVAL_SIMULATIONS=4, sans évolution → avec, IA seule) : Sylvarque 7 → 6 %, Golem 57 → 67 %, Ronce-Mère 5 → 19 %,
  Ignarok 6 → 11 %, Givrecorne 33 → 31 %, gardien 22 → 37 %, Mère des Brumes 47 → 54 % ; avec l'équipement, Ignarok
  monte plus (17-43 → 52-74 %).
- Forge (2.1, plan validé le 2026-10-12) : armes et armures en +1/+2/+3. `rebuildData` (data.cpp) génère pour chaque arme et
  armure les objets `<id>+1`… (`ItemDef::forged`, `base` ; `forgedId`, `forgeable`), bonus + part arrondie au-dessus
  (`Rules::forge` : `bonus`, `or`, matériaux ; « trophee » = tout objet `trophy`), prix + or de la forge. Ils ne sont pas
  dans objets.json ni dans les Réglages. Matériaux : `minerai` (+1), `coeur_braise` (+2), `cristal_givre` et un trophée
  (+3) ; butin selon le type (`Rules::typeDrops`, `butin_type`, battle.cpp `victory`), coffres, et chaque boss donne son
  trophée (action `donner` après son drapeau). L'enclume de Forgeroc (événement `forge`, une fois `brann`) ouvre
  `Game::forgeMenu` : équipement porté puis sac ; Entrée forge aussitôt (comme un achat). Joueur automatique :
  `Pilot::forgeWanted` / `forging` (l'équipement porté, en gardant 150 or). Mode test : bloc « forgé au maximum
  possible » (`simForge`). Réglage : +1 = +30 %, +2 = +40 % (60 % rendait Ignarok trop facile : 17 → 64 %), +3 = +100 %.
  Mesuré (BRUMEVAL_SIMULATIONS=4, boutique → boutique et forge) : Sylvarque 33 → 37 %, Ronce-Mère 29 → 30 %, Golem 55 → 52 %,
  Ignarok 14 → 36 %, Givrecorne 75 → 68 %, gardien de cristal 70 → 85 %, Mère des Brumes 70 → 86 %.
- Vitesse du jeu (1.9) : `options().speed` (1 à 3, ligne `vitesse` d'`options.txt`), Options > Vitesse du jeu ou F2 (`Game::cycleSpeed`, grand
  « Vitesse ×2 » un instant, puis « ×2 » discret en haut à droite). main.cpp appelle `game.update(dt)` `Game::speed()` fois par
  image : tout va plus vite (marche, combats, textes) sans changer la logique ; la musique reste normale. Toujours ×1 en
  multijoueur (`Online::connected`).
- Aide et descriptions (1.9) : `Game::helpPage_` ≥ 0 = une page d'aide en plein écran ; le reste du jeu attend (`update`
  s'arrête après le réseau). `moveDetails(m, lanceur)` / `moveHelp` (battle.cpp) décrivent toute technique (genre, type,
  puissance, coût, cible et zone, portée, soin chiffré, effets, rythme), `itemDetails` / `itemHelp` tout objet (soins,
  capture de x % à y %, bonus, où l'utiliser) : le champ `description` des données n'est qu'une phrase d'ambiance placée
  avant. `matchupText` donne les faiblesses d'une espèce (aide du Bestiaire).
- Commandes : `Game::onKey` lit les touches choisies (`options().keys`) en plus
  des flèches, d'Entrée et d'Échap (toujours actives). Manette :
  `Game::onPad` / `onStick` (appelés par main.cpp, qui ouvre les manettes
  branchées) remplissent le même `Input` ; une direction tenue se répète
  (`padRepeat_`). Plein écran : `options().fullscreen`, appliqué par main.cpp.
- Animations de combat : `Battle::startAnim` (appelé par `useMove` et
  `useItem`) retient le lanceur, les cibles, l'effet (`Battle::Fx` : coup,
  sort, soin, statut, capture) et la couleur du type ; `offset` décale le dessin
  (élan, secousse quand on est touché) et `drawAnim` dessine l'effet. En ligne,
  l'hôte ou le chef envoie `anim` aux autres.
- Police : `gfx.cpp`, fonction `buildFont()`. Un caractère absent s'affiche « ? » ;
  ajoute son dessin si tu utilises un nouveau symbole.
- Données : tout le contenu est dans `data/` (voir `data/LISEZMOI.md`), chargé au
  démarrage par `loadData()`, `loadMaps()`, `loadEvents()`. Aucune donnée de jeu
  ne doit être écrite en dur dans le code : ajouter un champ au JSON et au
  chargeur. Les cartes sont désignées par leur identifiant (`MapDef::id`), pas
  par leur numéro (ordre alphabétique des fichiers).
- Intérieurs : thème `Theme::Interieur` (parquet, murs, meubles `p t h e o v`,
  dessinés dans sprites.cpp). Une porte de bâtiment avec `interieur` fait entrer
  dans cette carte (`Game::enterDoor`, appelé par `interact` avec Entrée et par
  `tryMove` quand on marche dans la porte ; case d'arrivée `interiorEntry` :
  au-dessus du passage de sortie) ; `checkMaps` vérifie la sortie et part de cette case
  pour l'accessibilité. Une carte moins haute que l'écran est centrée
  (`drawMap`). Les intérieurs des villages sont `data/cartes/<village>_<lieu>.json` (12 au départ ; 17 de plus pour
  Moulinbourg, le port, Havre-d'Aube, Corail et Port-Écho, `outils/villes/interieurs_nouveaux.py` : le service passe à un
  habitant de l'intérieur, la description d'une maison devient un déclencheur au premier pas).
- Quêtes annexes : `data/quetes.json` (lu par `loadEvents`, vérifié par
  `checkEvents`), action `quete` (drapeaux `quete:<id>` et `quete:<id>:fin`,
  `questFlag`), journal `Game::journalMenu` (en tête : « Progression », `Game::progressMenu` : quêtes, primes,
  dresseurs, revanches, coffres, Bestiaire, poissons, discussions, trésors cachés, joueurs de cartes, cartes, exploits, chromatiques, tout recalculé d'après les drapeaux). 20 quêtes (2026-10-19 : 14 de plus, ajoutées par un script de
  Claude) : `medaillon`, `braises`, `tisane`, `grimoire`, `cristal`, `perles`, puis `chat`, `naturaliste` (montrer
  des créatures : condition `membre`), `lettre`, `apprenti`, `caravane` (boss de carte sous condition), `champignons`,
  `oeuf` (donne un Draconnet), `cache_cache`, `bains`, `rival` (Tom, trois combats au fil de l'histoire),
  `capitaine`, `partitions`, `conte` (après la fin), `ruches` ; puis (outils/villes/quetes_lieux.py) `chien`, `collier`,
  `message`, `tournoi`, `etude_aube`, puis (outils/villes/quetes_actes.py, actes 2 et 3) `phare_aube`, `enfant_marais`,
  `spectre` (boss de carte sous condition dans le Gouffre), `pages_orage`, `carapace`, puis `echos`, `cloche`, `etude_oriane`
  (acte 4), `eleveuse` (Forgeroc, `outils/villes/eleveuse.py` : huit créatures évoluées à montrer, une par une,
  drapeaux `eleveuse1`…`8`), `quiz` (le conteur de la Vallée après `echos_fin`, dix questions oui/non,
  `outils/villes/quiz.py`), et celles des nouveaux lieux (`trident`, `esprits`, donjons, `epreuves`, `peche`, `bestiaire`). Habitants et boss de carte peuvent avoir `si` (présents
  seulement si la condition est vraie : `Npc::cond`, `BossSpot::cond`).
- Événements : habitants (`evenement`), portes des bâtiments (genre ou
  `evenement`) et boss lancent un événement de `data/evenements.json`. Les
  actions sont exécutées par `Game::execAction` (events.cpp) via le `Script` ;
  `Script::runNow` fait passer la suite (réponse à une question, fin de combat)
  avant les étapes déjà en attente.
- Combat : dégâts physiques = Attaque contre Défense, magiques = Magie contre
  Résistance ; `moveEff()` combine les deux types de la cible et ses
  `resistances` propres. Un seul état à la fois (`Fighter::status`), bonus/malus
  de -3 à +3 (`Fighter::stage`, effet = `etage` des règles), tout est effacé à la
  fin du combat (`clearBattle`). `Battle::think` choisit les actions de
  l'ordinateur (ennemis, et alliés en mode test) ; `Battle::log` garde un journal
  lisible (`BRUMEVAL_JOURNAL=Sylvarque brumeval --test captures` l'affiche pour
  la simulation qui contient ce mot).
- Écran de combat (1.7, refait en 3.2 et 3.3) : rien n'est écrit sur le damier. Panneau du bas (`PANEL_Y`, `dock`) :
  menu de commande à gauche (5 lignes, il défile ; depuis le 2026-10-23, « Limite » seulement prête, en or, « Duo » seulement
  possible, « Changer » seulement avec un remplaçant), à droite l'ATH en tableau (`Battle::drawHud` : une ligne par allié,
  titres PV, PM, ATB, Lim ; barre de PV verte/or/rouge ; compagnon en retrait ; mode Auto/Manuel), remplacé par le
  sous-menu ouvert (`MenuStack::shownDepth`). En haut, l'aide de la ligne survolée en deux lignes au plus (un message
  du combat : trois), puis la ligne de la météo, de l'ordre des tours et des renforts :
  en combat, `moveBrief` (une ligne compacte : genre, type, puissance, coût, portée, zone, effets, rythme) remplace
  `moveHelp`, et les objets n'ont que `itemDetails`. Météo à gauche de l'ordre des tours.
- Choix sur le damier (3.2, demandé le 2026-10-15) : la cible (`pickFoe`, `pickAlly`) et la case de « Se déplacer » se
  choisissent sur le damier (flèches : la case la plus proche dans cette direction ; souris : survol et clic ; Entrée,
  Échap). Le menu existe toujours mais `Menu::hidden` (pas dessiné ni cliquable : joueur automatique, mode test et aide
  le pilotent comme avant) ; `Battle::boardMenu` le pousse et retient `pick_` (camp, case de chaque ligne) ;
  `boardInput` lit flèches et souris avant `menus.update` (sauf joueur automatique). `drawZones` : la case choisie en
  or, le reste de la zone en orange, les autres choix en contour blanc ; « Se déplacer » montre le combattant en
  transparence sur la case. En survolant une technique (`previewMove`, `MenuItem::hover`), sa zone s'allume autour
  de la cible probable (`cursor`), les ennemis hors de portée du corps à corps sont barrés. Par-dessus les combattants :
  contour de la zone et flèche dorée au-dessus de la cible (`fighterTop`) ; le reste du camp visé est estompé (alpha 0,45).
- Sprites (3.2, refonte demandée le 2026-10-15) : planches « pixel art » de l'utilisateur dans `outils/sprites/planches/`,
  découpées par `python outils/sprites/decoupe.py` (liste `decoupe.json` : planche, boîte, taille ; recolorations
  `variantes.json`) en un atlas `data/sprites.png` + `data/sprites.json` (deux fois plus fin, `_echelle` 2 ; nom → x, y,
  largeur, hauteur dans l'atlas ; `SpriteRect` : `w`, `h` dans le jeu, `sx`… dans l'atlas, `sub` pour un morceau ; un
  atlas retourné d'avance sert aux sprites tournés vers la droite ; planche de
  contrôle `outils/sprites/apercu.png`, non suivie). Le jeu lit l'atlas avec stb_image (`src/ext/stb_image.h`,
  compilé seul dans image.cpp) ; `findSprite(nom)`, `Gfx::sprite(…)` (miroir, échelle, teinte, éclat). Champs :
  `sprite` des apparences (préfixe : `<p>_bas_0`, `_haut_k`, `_droite_k` ou `_gauche_k`, `_combat` ; un animal : un seul
  sprite) et des espèces (regard vers la gauche ; boss : `_grand`), `icone` des objets (dessinée devant le nom dans
  tout menu, `itemIcon` de ui.cpp). Ce qui n'a pas de sprite garde son dessin en code. Créatures au repos (`drawCreature`, 2026-10-20) : celles qui volent flottent (l'ombre rétrécit), les autres
  respirent (le haut du sprite descend d'un pixel, puis remonte ; pas en réduction sur la carte). Arbres `T` de la carte : le haut penche d'un pixel par moments (vent, `drawProp`). Papillons au-dessus des fleurs le jour et poissons qui sautent dans l'eau (`drawMapCritters`, après les personnages). Effets de combat (`spriteFx`) : images en fondu et étincelles du type. Personnages de la carte en
  16×24, combat en ~36 px ; `crisp` n'agrandit que d'un nombre entier de fois. Dialogues (2026-10-22) : « Nom : réplique » met le nom dans un onglet au-dessus de la fenêtre
  (`drawDialogue`). Lior a ses visages (`lior_visage_0..4`,
  portrait des dialogues selon la réplique). Effets des sorts (`spriteFx` : une suite d'images par type, soins),
  invocation au-dessus des cibles d'une Limite (`inv_…` selon le type), textures 32×32 (`texTile` : parquet des
  intérieurs, lave, neige profonde). La « Poké Ball » de la planche n'est pas découpée (marque de Pokémon).
- Cartes (3.2) : arbres, rochers, tonneaux et nouveaux objets (`hasProp`/`drawProp`) et bâtiments sont dessinés avec
  les personnages, triés de haut en bas (on passe derrière un toit ou un arbre). Bâtiments : sprite selon le genre
  (maison, taverne, boutique, église, entrepôt), la plus grande largeur 96/80/64 qui tient dans l'emprise, posé sur
  son bas, la porte du sprite alignée sur `doorX` ; variante de toit selon `toit` ; champ `dessin` pour en imposer un ;
  genre `decor` = sans porte ni événement (moulin, ruines, château). Nouvelles tuiles : `q` pavés (praticable), `H`
  clôture, `L` réverbère, `Y` puits, `C` caisses, `U` buisson, `Q` banc, `K` charrette, `M` étal, `X` feu de camp,
  `G` tableau d'annonces, `J` bannière. Villes redessinées par les scripts de `outils/villes/` (`carte.py` écrit au
  format de `writeJson`) : place pavée, marché et jardins dans la Vallée (+ ferme au moulin), rue pavée et forge à
  Forgeroc, place de Givreval, port, camp de Sylve-Noire ; habitants marchande, aubergiste, soigneur, forgeron,
  voyageuse, paysan et animaux. Nouvelles créatures (monstres restants des planches) : Tisseline (Grotte), Osselet
  (Ruines, Tour), Gobelard et Brutor (Sylve-Noire), Draconnet (volcan, rare), Coffrelin (coffre piégé des Ruines,
  « boss » de carte, événement `coffrelin`).
- Blender et Godot (3.4, demandé le 2026-10-18) : `outils/blender/personnages.py` construit des personnages chibi en 3D
  (fiches `personnages.json`), `pixeliser.py` les réduit en pixel art dans `outils/sprites/rendus/`. Les habitants
  rendus ainsi ont été jugés trop différents du reste du jeu (2026-10-19) : leurs poses de dos et de profil sont
  maintenant tirées de leur dessin de face par `outils/sprites/tourner.py` (entrées `tourner` de `decoupe.json` :
  visage changé en cheveux ou glissé de trois quarts, pieds décalés pour la marche ; options `chapeau`, `cheveux`,
  `tablier`, `tourne`), avant la réduction commune : même style que les planches. Blender reste pour s'exercer. `outils/godot/effets/rendu.gd` rend des effets en particules image par image (`gfx_<type>_k` : Métal,
  Normal, Plante et Esprit (2026-10-20) dans `spriteFx`, Percée et Brisure dans `Battle::bursts_` ; cercle d'invocation `gfx_cercle_k` sous les cibles
  d'une Limite, dans `Battle::draw`). Marche à suivre et pièges : compétence
  `sprites-blender-godot`.
- Améliorations (branche `ameliorations`, carte blanche du 2026-10-16) : petits détails du sol en sprites (touffes,
  fleurettes, cailloux selon `hsh`, dans `drawTile` '.'), hautes herbes `,`/`g` en touffes de la planche
  (`tallGrass`, qui ondulent, avec des rafales qui traversent les champs ; `front` : le bas des touffes redessiné sur les pieds du joueur), montagnes `m` en éboulis de rochers.
  Courir : Maj ou X de la manette maintenu (`Input::run`, `Game::running_`, pas à 13/s au lieu de 7,5, images
  `lior_course_k`). Intérieurs : `e` = une bibliothèque pour deux cases, `t` = table (sprite « banc »), `j` = chaise
  (tournée vers la table), script `outils/villes/interieurs.py`. Expédition : une créature générée sans sprite prend
  celui d'une créature de l'histoire de même forme (`borrowedSprite`, d'après son identifiant, teinte de `c1`), un
  personnage `lk_…` celui de la planche selon son arme (teinte de `top`) : dessin seul, le monde généré ne change pas.
  Poses de combat (`drawFighterPose`, Lior) : `lancer_k` pendant une capture (la lanterne vole vers la cible),
  `victoire` pendant la fenêtre des gains (à droite, du côté des ennemis vaincus), `ko` à terre.
  Dégâts prévus sur la cible visée (`Battle::damageText`, même formule que `applyHit` sans hasard ni critique :
  fourchette de `ecart_min` à 1, chance de toucher ; depuis le 2026-10-25 « l'abat », « peut l'abattre », « le brise ») ; l'efficacité d'un type n'est comptée qu'une fois découverte
  (`weakKnown`), sinon neutre : rien ne se devine sans attaquer (Brisure). Ennemi vaincu : il s'efface en 0,6 s (`koT_`). Décor de l'horizon en sprites
  (arbres, sapins, rochers). Écran titre : les personnages de part et d'autre du menu, dans un paysage de nuit (lune, montagnes enneigées,
  village aux fenêtres allumées, brume, prairie aux herbes qui ondulent ; `drawTitle`, 2026-10-20). Habitants : regards et
  sautillements (dessin seul). Ordre des tours : portraits (`drawFighterIcon`). Un objet haut juste devant le joueur,
  un coffre, un panneau ou un habitant devient transparent (`behind` dans `drawMap`). Murs de grotte, de glace et
  falaises en `tex_pierre` teintée, dalles au sol des grottes (`texTile` avec teinte). Fontaine `W` dessinée une fois
  depuis le coin bas-droit de son bloc (ronde, jet d'eau) ; départ de la partie en (12, 10). Compagnon qui suit
  (`trail_`, `followDir_` : la première créature de l'équipe marche une case derrière le joueur, dessin seul, vidé
  au changement de carte ; pas en expédition à plusieurs). Promeneurs : champ `promenade` des habitants (distance
  autour de leur place ; jamais un dresseur), `walk_` (case actuelle et regard des habitants de la carte affichée,
  hasard à part `walkSeed_`, rien d'enregistré), `npcCell` pour toute position d'habitant (collision, parole,
  dessin) ; chaque habitant se tourne vers le joueur qui lui parle (sauf un dresseur). Éditeur de cartes : objets hauts dessinés (`drawProp`,
  `drawPropIcon` dans la palette). Carte du monde (Échap > Carte, `worldMapOn_`, `drawWorldMap` dans menus.cpp) : les
  lieux ayant `carte_monde` atteignables d'ici par des passages ouverts (`checkCond`, `worldOpen`) ; pas en
  Expédition. Voyage rapide (2026-10-20) : flèches pour choisir un village déjà visité qui a une maison de soin
  (`travelTargets`, `Game::visited`, lignes `lieu` de la sauvegarde), Entrée pour y aller (devant la maison de soin). Eau `~` dessinée en code (la texture `tex_eau` de la planche n'est pas utilisable) : bords peu
  profonds plus clairs, ombre de la berge, écume en tirets, vaguelettes, reflets. Neige (`ground`) : rides de vent,
  congères, brins secs, cristaux ; chemins `=` de neige tassée (empreintes, bords de neige fraîche), et les pas du
  joueur y restent 30 s (`footprints_`, dessin seul, vidé au changement de carte) ; case `c` d'une
  carte de neige = dalles de glace (Temple). Intérieurs : fenêtres aux murs (`interiorWindow`), lumière au sol sous
  elles et ombre des murs (`drawTile` ajoute tout cela par-dessus `drawTileBase`). Quêtes : bulle « ! » (à prendre)
  ou « ? » dorée (à rendre) au-dessus d'un habitant, `Game::questMark` (events.cpp) lit l'action `quete` de la page
  qui jouerait maintenant (aussi dans les réponses d'une question). Changement de carte : fondu depuis le noir (`mapFadeT_`, 0,3 s). La nuit, réverbères `L` et feux `X`
  allumés (halo dans `drawMap`).
- Graphismes des cartes (2.0, sprites.cpp) : maisons (`drawBuilding` : ombre, soubassement, pierres, tuiles en rangées,
  faîtage, cheminée, fenêtres à volets et jardinières), arbres `T` en trois variantes selon `hsh` (rond, petit, fruitier
  ou large en forêt), chemins `=` aux bords adoucis contre la nature (liseré, touffes, coins), herbe de la vallée variée.
  2026-10-25 : le large de l'eau `~` plus sombre loin des berges (carrés de 4 px) avec une houle lente ; ombres portées
  selon le soleil (`drawBuildingShadow`, `drawPropShadow`, appelées dans `drawMap` avant les personnages : longues vers
  l'ouest le matin, courtes à midi, vers l'est le soir, pâles au crépuscule et sous la pluie) ; pavés `q` bordés d'un
  rebord de pierres là où ils touchent l'herbe ou la terre, avec des brins qui débordent.
- Graphismes (1.8) : décor des combats avec l'horizon à y 56 (`HZ` dans `Battle::draw`, reliefs à moitié de hauteur) pour que les trois rangées du damier soient sur le sol ; dalles en losange ; sur les cartes, écume le long des berges et ombre au pied des arbres (sprites.cpp).
- Meutes (1.7) : `Game::wildDepth`, distance à pied du plus proche bâtiment ou
  passage ; plus on s'enfonce (jusqu'à `meute_distance` cases), plus les
  rencontres sont fréquentes (+`nature_dense`) et les groupes grands (jusqu'à
  `meute_max`, 6). Règles, rencontres ; une zone peut avoir son propre `meute_max` (`Zone::packCap` : 3 dans
  les étages de l'Abîme et du Vieux Phare).
- Échiquier (3.4, demandé le 2026-10-18 : « que ça marche comme un échiquier, que les combattants puissent passer du
  côté adverse ») ; code dans `src/echiquier.cpp` (méthodes de `Battle` sur le plateau, `challengeSetup`) : en combat, un seul plateau de `BOARD_W` × `GRID` (8 × 4) cases pour les deux camps (data.hpp :
  `boardX`, `boardY`, `boardAt`, `inHalf`, `boardDist` (diagonales comprises), `boardSteps`, `inBoardZone`,
  `boardName`). Alliés à gauche (colonnes 0 à 3, avant en 3), ennemis à droite (4 à 7, avant en 4). `Battle::cells_` =
  case du plateau ; la formation (`Fighter::cell`, ligne `case4` de la sauvegarde, 0 à 15 dans sa moitié, `GRID` 4,
  `cellCol` 0 avant … `BACK_COL` arrière) est placée par `toBoard` au début. Règles (`damier` de regles.json) :
  « Se déplacer » une fois par tour **sans finir le tour** (`moved_`, `walkable` : `mouvement` cases sans diagonale,
  +1 pour les types `rapides` ; on traverse ses alliés, pas les adversaires, arrêt au contact ; les types `vol`
  passent par-dessus), puis `resumeTurn` rouvre le menu (ou l'ordinateur agit) ; « Attendre » (`waitTurn`, jauge
  rapide). Corps à corps (`reachable`) : adversaire à `reach` (1, boss `allonge_boss`), sinon rejoignable ce tour
  (`approachCell`, le pas est fait au début de `useMove`). `posMult` (dans `applyHit`, `attackScore`, `damageText`) :
  dans le dos (`behind` : du côté opposé au regard de la cible, × `dos` ; regard `facing_` (2026-10-23) : vers où il
  marche (`step`), vers sa cible (`useMove`), vers l'adversaire le plus proche s'il attend (`waitTurn`) ; par défaut vers le
  camp adverse ; flèche au sol ; « Se déplacer » : dague dorée sur les cases d'où l'on frappe un adversaire dans le dos
  (`backReach`, aide « Dans le dos de … ») ; 10e nombre de chaque combattant dans `etat`, en miroir chez l'invité du duel), encerclé (`encercle` par adversaire au contact
  au-delà du premier, pas un boss), distance ou magie au contact d'un adversaire (× `contact`, pas un boss). Percée :
  atteindre le bord adverse donne +`percee` en Attaque, Magie et Vitesse une fois (`breakthrough_`). Poussée dans
  l'axe du lanceur (`shoveDest(a, t, m)`), choc contre tout combattant. Terrain : `terrain_[case du plateau]`.
  Ordinateur : `planMove`/`posValue` (meilleure attaque possible de la case, danger, distance, protecteur, terrain,
  zone annoncée, Percée) avant `think` dans `enemyTurn` et `autoCommand` ; `think` renvoie `Plan::wait` si rien
  n'est à portée ; tactiques : sans attaque à portée, il s'approche puis rejoue (`tacticTurn`). Menus : une
  technique sans cible atteignable est « hors d'atteinte ». Dessin : `cellPos(case)` (colonnes de 32 px), cases
  alternées, moitiés teintées, ligne de front, glissement (`slide_`), regard vers l'adversaire le plus proche
  (`facesRight`). Réseau : cases du plateau dans `etat`, vues **en miroir** chez l'invité du duel (`netCell`) ;
  `tour` porte `bouge` (déjà déplacé), ordres `deplacer` et `attendre`, `obstacles` dans `etat`. Obstacles
  (`obstacles_`, `blockedCell` ; règles `obstacles_min/max`, sprites par décor `decors_obstacles`) : tirés dans les 4
  colonnes du milieu, ni arrêt ni traversée (sauf vol), choc si on y pousse ; aucun en duel. En mode auto, les
  tactiques commencent par `planMove` (pas chez l'invité). « Se déplacer » marque les cases menacées
  (`meleeThreat` : losanges rouges, aide « Danger : n ennemis »). Techniques de mouvement (`Move::motion`, champ
  `mouvement`) : `bond` (le lanceur saute derrière la cible, `leapDest`) et `echange` (cases échangées, pas un
  boss), dans `motionAfterHit` ; effets Godot `poussiere` et `echange`. Sons `pas`, `bond`, `echange`. Menu de commande :
  portée en bleu (`walkable`), « Revenir » (`moveFrom_`, pas en ligne ni après une Percée) ; choix de case : chemin
  (`pathTo`). Tactiques `approcher`, `contourner`, `proteger` (`Plan::cell`, le tour continue) ; une règle de
  déplacement qui s'applique passe avant `planMove`. Formation : liste dans l'ordre du dessin (colonnes), Gauche/Droite
  par `adjust`, souris sur le dessin (`formationPick_`, `formationCellAt`). Équipement : champ `mouvement`
  (`ItemDef::moveBonus`, ajouté dans `moveRange`) : Bottes de vent, Bottes ailées. Défis : `data/defis.json` (alliés,
  ennemis et obstacles avec `case` [ligne, colonne] du plateau, `decor`), `Arena::menuChallenges`/`startChallenge`,
  `BattleSetup::fixedBoard` (`allyCells`, `foeCells`, `obstacleCells` ; ni météo ni terrain), `challengeSetup(id)` (echiquier.cpp,
  partagé par l'Arène, les événements et le joueur automatique). Action `combat` d'un événement : `defi` (ce défi, ses
  combattants prêtés : pas de « Changer ») et `entrainement` (ni gains ni évolutions, une défaite ne coûte rien :
  `BattleSetup::training`) ; maître d'armes de la Vallée (événement `maitre_armes`, drapeau `maitre_bottes`, Bottes de
  vent ; ensuite, trois questions en chaîne : « Lire les intentions », « La garde », « Dans le dos »). Sept défis : `dos`,
  `percee`, `ailes`, `mur`, `bousculade`, et (2026-10-25, pour apprendre la 3.7) `intentions` (un Ferrolin N.20 lent, blindé
  et faible à la magie annonce Coup d'acier sur Isra : l'écarter, Lior peut barrer la route ; simulé : IA 96 %, Isra qui ne
  bouge jamais 20 %) et `garde` (Brann acculé dans un coin par quatre Mulotins rapides : IA 37 %, toujours en garde 79 %).
  Mesuré (BRUMEVAL_SIMULATIONS=2) : sans
  réglage, boss bien plus faciles (encerclés par 6 alliés : +60 %) ; après (boss ni encerclés ni gênés au contact,
  encerclement 10 %, Ignarok N.25, Ronce-Mère N.22, Mère des Brumes N.41), difficulté proche de la 3.3.
- Primes (demandé le 2026-10-19 : « au moins 20 heures de jeu ») : `data/primes.json`, `primes.hpp` (chargées par
  `loadMaps`, qui ajoute le monstre de chaque prime à sa carte : `BossSpot::bounty`, visible si `BossSpot::cond`, jamais
  écrit par `mapToJson`), événement généré `prime:<id>` (`bountyEvent`, trouvé par `findEvent`, jamais dans
  evenements.json), drapeaux `prime:<id>`, `:vaincue`, `:fin` (`bountyFlag`). Tableau d'annonces = tuile `G`
  (`Game::interact` → `bountyMenu`, chasse.cpp ; pas en Expédition) dans la Vallée, à Forgeroc, Sylve-Noire,
  Givreval et au Port ; `claimBounty` (or, objets, rang de chasseur `bountyRanks`, `hunterTitle`). Bulle rouge « ! »
  au-dessus du monstre ; Journal : primes en cours. 17 primes (12 pendant l'histoire, 3 après la fin : Sylvarque,
  Ignarok, Mère des Brumes, super-boss ; 2 sur l'île des Échos, `outils/villes/echos_annexes.py`). Joueur automatique : `Pilot::bountyWork`/`bountyStep` (prend tout, touche
  tout ; chasse comme un boss, quand il est prêt). `checkEvents` vérifie les primes (sauf en Expédition).
- Attaques annoncées (1.8) : `Battle::bossPlan` ; un boss (pas en duel) annonce
  parfois (`boss_annonce`, 0,35) sa plus forte attaque sur la colonne, croix ou
  ligne qui couvre le plus d'alliés (`warnings_`, zone rouge, 8e champ `w` de
  `etat` pour l'invité) ; l'annonce ne lui coûte pas son tour. À son tour
  suivant, elle frappe ceux restés dans la zone × `boss_annonce_puissance`
  (1,6) ; si tous se sont écartés, il attaque normalement. Les alliés joués par
  l'ordinateur (`autoCommand`) se mettent à l'abri (`threatened`, `safeCell` : une
  case où il peut aller ce tour). Tactiques : conditions `soi_menace`, `ennemis_groupes`
  (taille de la zone), `allie_avant_pv_moins` ; action `esquiver`
  (`Plan::cell`).
- Rythme (`Move::pace`, champ `rythme`) : après une technique rapide, la jauge ATB
  du lanceur repart à `jauge_rapide` % ; après une lourde, à -`retard_lourde` %
  (`paceGauge`, passé à `Battle::afterAction`). `Battle::attackScore` note une
  attaque pour `think` et les tactiques : dégâts attendus divisés par
  `paceTime` ; pour les alliés seulement, critiques et effets en plus comptent
  aussi (les ennemis gardent l'ancienne notation, sur laquelle la difficulté
  est réglée). Principe des techniques : une faible est rapide et souvent avec
  un effet, une forte est lourde, pour qu'aucune ne devienne inutile.
- Brisure (2.5, Octopath) : `Fighter::shield/shieldMax/broken`, posés dans le
  constructeur de `Battle` (ennemi d'un événement `bouclier`, sinon espèce,
  sinon `brisure.bouclier(_boss)` ; + talent Cuirasse ; jamais en duel). Dans
  `applyHit`, un coup sur une faiblesse retire 1 (2 si ×4, + Briseur) et
  l'allié l'apprend (`Game::weak`, lignes `faible` de la sauvegarde) ;
  `breakFoe` : tour perdu (`skipTurn` le restaure), annonces annulées, dégâts
  × `brisure.degats`. Réseau : 8e et 9e nombres de chaque combattant de `etat`.
  `attackScore` (alliés) favorise la brisure. Frise `turnOrder`/`drawTurnOrder`
  (FFX, prédiction sans hasard). Talents (Pokémon) : `Rules::talents`,
  `talentOf`/`talentValue(s, effet)`, appliqués au début du combat (vif,
  intimidation), dans `applyHit` (résistances, rage, critique, furtif, briseur,
  piquant), dans `afterAction` (régénération, méditation, soin des voisins en
  croix) et `immuneTo` (sans_etats). Effets visuels : `drawTypeFx` (par type,
  `Anim::type`, champ `ty` du réseau), `shakeT_`, entrée en combat et bandeau du
  boss à la fin de `Battle::draw`. Mesuré (2.5) : boss relevés pour que la
  victoire demande faiblesses + équipement (Golem 50 % IA / 75 % forgé, Mère des
  Brumes 11 % / 63 %).
- Stratégie 2.6 (Into the Breach, Chrono Trigger) : `Move::shove` (champ `deplace`), `Battle::shove`/`shoveDest`
  après le coup sur la cible principale (choc `damier.choc` si case prise ou bord, KO ajoutés à la liste du coup ;
  boss : -2, inébranlable). Météo `weather_` et terrain `terrain_[camp][case]` tirés dans le constructeur selon
  `themeName(bg)` (`Rules::weathers`, `terrains` : types en identifiants, résolus en combat) ; aucun en duel
  (`goOnline`) ; envoyés au gardien à plusieurs (`meteo`, `terrain` de `etat`). Effets : `weatherMult` et précision
  dans `applyHit`/`attackScore`, `atbRate` (verglas, aussi `turnOrder`), `terrainTurn` (fin du tour dans
  `afterAction`, et à l'arrivée d'une poussée), herbes et cristaux dans `applyHit`. `threatened` inclut une case
  qui blesse (`terrainHurts`) : l'IA et la tactique « Se mettre à l'abri » la quittent ; l'ordinateur l'évite (`posValue`).
  Duos : `Rules::duos`, `duoMember` (espèce, `humain`, `type:x`), `Battle::duoOptions` (partenaire vivant, jauge ≥
  `duo.jauge`, PM des deux ; jamais en ligne), `duoMove` (le partenaire paie et sa jauge tombe à 0, puis `useMove`
  avec `duoPartner_` pour le message). IA : `think` les compte × (1 - 0,45 × jauge du partenaire) ; tactiques : action
  « attaque » (`Plan::duo`). Expédition : seuls les duos `humain`/`type:` restent (`Expedition::install`). Jour et nuit
  sur les cartes : `Game::nightAmount` (12 min par jour, dehors seulement), `drawBuildingLights`,
  `drawChimneySmoke` ; le combat s'assombrit aussi. Pluie (2026-10-20) : `Game::rainAmount` (cartes Vallée et Forêt
  sans ambiance active, averses de 210 s choisies d'après leur numéro, jamais au début), dessinée dans `drawMap`,
  entendue (`audio::ambience`, bruit filtré du `Mixer`, appelé par `updateMusic` ; en combat : `Battle::raining`), et
  `BattleSetup::rain` impose la météo d'effet « pluie » si le décor la permet. Ombres de nuages (`drawMap`) ; brins
  d'herbe et poussière de course (`Game::puffs_`, dessin seul, vidé au changement de carte). Coffres : un éclat passe de temps en
  temps sur un coffre fermé (`drawChest(…, t)`), et à l'ouverture des rayons de lumière et l'icône de l'objet qui monte
  (`Game::chestFx_`). Bulles d'humeur : de temps en temps, un habitant sans bulle de quête (ni dresseur) montre une
  note de musique, « … » ou un cœur (selon sa case ; dessin seul, `drawMap`).
  Combat : les alliés qui gagnent un niveau sont entourés d'étincelles dorées pendant la fenêtre des gains
  (`Battle::levelUps_`).
- Stratégie 2.7 (FF7, boss en phases) : gemmes `ItemDef::gem` (`gemMove`, `gemBonus` en %, `gemSupport`/`gemValue`),
  `Fighter::gems[emplacement]` (ligne `gemmes <emplacement> id…` après `equipement`, aussi dans les échanges),
  `gemSockets` (règles `gemmes`), `trimGems` (rendues au sac quand l'équipement change), `gemSupport(f, effet)` ;
  `recalc` ajoute les bonus, `techs`/`spells` les techniques (et `bonusMoves`) ; effets dans `applyHit` (vol de vie,
  riposte avec `countering_`, briseur) et le constructeur (éveil). Menu `Game::gemMenu` (Équipement > Gemmes) ;
  `Pilot::socketGems` (à chaque `plan`). Rôles `Species::combatRole` (champ `role_combat`) : `guardMult` (protecteur,
  dans `applyHit` et `attackScore`), `atbRate` (chef), `roleHeal` (soigneur, dans `enemyTurn`) ; icône dans `draw`.
  Phases : `makeFoe` (data.cpp, partagé par events, pilote et tests) garde `phases` ; `Battle::checkPhase` après
  chaque technique (bouclier, bonus, techniques, météo, renforts ajoutés à `foes`) ; jamais en ligne. Portraits :
  `speakerLook` (game.cpp) ; titre animé dans `drawTitle`.
- Confort 2.8 : astuces `Rules::tips` / `Game::tip(id, script)` (help.cpp : une fois, `tipsSeen` = lignes `astuce`,
  `lastHelp_` = page de l'astuce pour F1 ; coupées par `options().tips`, `tipsOff_` du mode test et le joueur
  automatique). En combat `Battle::tip` / `startTips` (une seule au début, dans `update`) ; déclencheurs dans
  `applyHit` (faiblesse), `breakFoe`, `checkPhase`, `shove`, `command` (duo) ; la gemme dans `Game::update` (carte).
  Conseils `Rules::advice`, `Game::adviceText` (action `conseil`, tête du Journal). Sac par catégories
  (`itemMenuAt`), comparaison dans `gearPick`, coût des duos. Pas de niveaux de difficulté (refusé par l'utilisateur).
- Expédition 2.9 (procgen.cpp) : talent par type (`typeTalent`) ou par classe, `role_combat` selon le profil, talent
  des gardiens, phases des gardiens, gemme en récompense, gemmes en boutique (`shopStock`). Les nouveaux tirages viennent
  d'un générateur à part semé par l'identifiant (`idHash`, `stream(…, 29)`) : le reste d'un monde ne change pas, mais
  l'empreinte de la graine 2026 a changé (28da4b9e14a4ce4d ; 3.4 : bottes dans les boutiques, 171c24418b3b440e ; 2026-10-23 : conseil « Échap > Préparation > Tactiques », eb8f2be6200f01bc). Les conseils de l'histoire sont vidés en Expédition.
- Acte 2 (3.0) : cartes `port`, `recif`, `ruines`, `tour` (générées par un script, puis réglées à la main), le bateau
  du pêcheur (page `boss4` de l'événement `pecheur`) et le `passeur`. Énigmes : tuiles `O`/`P`/`D`, `MapDef::puzzle*`
  (champ `enigme`), `solvePuzzle` (world.cpp, recherche en largeur, aussi dans `checkMaps`), `Game::boulders_`
  (`bouldersMap_` : la carte qu'ils décrivent ; remis en place en changeant de carte, sauf énigme résolue),
  `pushBoulder` dans `tryMove`, « Recommencer l'énigme » (menu Échap), dessin dans `drawMap`. Joueur automatique :
  objectif `enigme:<carte>` puis `puzzleMoves_`. Condition `objet_min`. Final : `boss5` (Seigneur des Orages).
- Acte 3 (2026-10-20, « 20 heures ») : l'île d'Aube, après le Seigneur des Orages (`boss5`). Jetée de l'est du port
  (passage sous condition `boss5`, navigatrice Ysolde) ; cartes `aube` (Havre-d'Aube : soins, comptoir, Ilya),
  `lueurs` (Bois des Lueurs, boss de carte `gardien_lueurs`), `marais` (Marais Pâle, `reine_marais` : Larme d'Aube),
  `gouffre` (énigme des rochers, porte de lumière : passage sous condition `objet` `larme_aube`), `citadelle` (Ilya
  soigne, `brume_noire`, drapeau `aube_fin`, écran de fin). 8 créatures d'Aube et 3 boss recolorés (variantes.json :
  `lueurine`, `loup_aube`…). Écrit par `outils/villes/acte3.py` (cartes, vérifie l'accès à pied) puis
  `outils/villes/acte3_histoire.py` (espèces, objets, conseils, événements) : pour refaire, remettre data/ avec git.
- Acte 4 (2026-10-20, `outils/villes/acte4.py`, puis `outils/sprites/decoupe.py`) : l'île des Échos, après `aube_fin`.
  Jetée ouest de Havre-d'Aube (passages sous condition `aube_fin`, Ysolde en parle) ; cartes `echos` (Port-Écho :
  soins, comptoir, Oriane, quête `echos`), `pierres` (Forêt Pétrifiée, boss de carte `colosse_echo`), `cratere`
  (Cratère des Échos, `phenix_echo`), `sanctuaire_echos` (donjon à énigme de cartegen.py ; la porte du nord de la forêt
  demande les deux drapeaux `colosse_echo_vaincu` et `phenix_echo_vaincu`) : Ombre du Premier Gardien (N.95, 3 phases),
  drapeau `echos_fin`, Cœur d'écho, écran de fin. 6 créatures et 3 boss recolorés (variantes.json). La forêt a des arbres de
  pierre : champ `arbres` d'une carte (`MapDef::trees`, `propSprite` cherche `<arbre>_<arbres>`) ; aussi `lueur` (Bois
  des Lueurs, arbres turquoise), `pale` (Marais Pâle) et `brume` (Val des Murmures, arbres violets) ;
  le fond des combats sur ces cartes reprend ces arbres.
- Techniques tardives (2026-10-20, `outils/villes/techniques_tardives.py`) : deux techniques de plus par héros, aux
  niveaux 35 et 60 (avant, ils n'apprenaient plus rien après le niveau 27 environ).
  Les créatures aussi (`outils/villes/apprentissages_tardifs.py`) : deux techniques de leur type aux niveaux 45 et 65
  (physiques ou magiques selon leur Attaque et leur Magie ; repli sur une technique normale ; jamais d'attaque sur
  tous les ennemis : une meute qui en a plusieurs écrase l'équipe).
- Équipement tardif (2026-10-20, `outils/villes/equipement_tardif.py`) : rangs lueur (boutique de Havre-d'Aube) et
  écho (Port-Écho), une arme et une armure pour les combattants et pour les mages, forgeables comme les autres.
- Créatures de la nuit (2026-10-20, `outils/villes/nuit.py`) : champ `nuit` d'une zone (`Zone::night`, écrit par
  `mapToJson` seulement s'il n'est pas vide) ; quand `nightAmount() > 0,5`, une part `rencontres.part_nuit` (0,5) des rencontres en viennent.
  Onze cartes du dehors en ont.
- Créatures chromatiques (2026-10-20) : `Fighter::chroma` (ligne `chromatique` après `membre` dans la sauvegarde),
  tirée dans `Game::encounter` (règle `rencontres.chromatique`, 0,006 ; jamais en Expédition) ; `decoupe.py` fabrique
  `<sprite>_chroma` (teinte tournée de 140°, gris dorés) pour chaque créature ni humaine ni boss ; `drawCreature(…,
  chroma)` le dessine avec des éclats dorés ; message d'entrée en combat ; condition `chromatiques_min`, quête `chroma`
  (l'assistant de la professeure, `outils/villes/chromatiques.py`). Alphas (même jour) : une meute de 2 créatures ou
  plus est parfois menée par un alpha (règle `rencontres.alpha`, 0,06 ; pas en Expédition) : +`alpha_niveaux` (3) et `Fighter::boss`
  (dessiné plus grand, bouclier de boss, attaques annoncées, gains de boss ; capturé, il redevient ordinaire) ; message « Une meute menée par un alpha ».
- Nouvelle partie + (2026-10-20) : après `aube_fin`, Ilya propose de tout recommencer (action `nouvelle_partie_plus`,
  `Game::startNewGamePlus`) : équipe, équipement, or, Bestiaire et drapeaux `peche:` gardés ; objets importants (prix 0,
  ni équipement ni gemme) rendus ; drapeaux effacés sauf `ngp<k>`, et `ngp<k+1>` posé. `foeLevelBonus` (data.cpp) =
  `NGP_LEVELS` (12) × tours, ajouté par `makeFoe` et aux créatures sauvages ; posé par `applyNewGamePlus` (nouvelle
  partie : 0, chargement, écran titre : 0). Le joueur automatique répond Non à cette question.
- Discussions (2026-10-20, `discussions.cpp`, `data/discussions.json`) : conversations courtes entre héros (comme les
  saynètes des Tales of) : `id`, `titre`, `si`, `membres` (espèces dans l'équipe), `dialogue` ; Échap > Journal > Discussions
  (nouvelles d'abord), drapeau `discussion:<id>` ; une nouvelle disponible est annoncée sur la carte
  (`checkDiscussions`, `lastNewTalks_`, remis au chargement). 50 discussions.
- Exploits (2026-10-20, `exploits.cpp`, `data/exploits.json`) : des succès (`id`, `nom`, `description`, `si`) ;
  `checkExploits` (toutes les secondes sur la carte, avec `checkDiscussions` ; pas en Expédition ni pendant la pêche)
  pose `exploit:<id>` (acquis pour de bon) et l'annonce ; Journal > Exploits (`exploitsMenu`, à faire d'abord). 32
  exploits ; condition `quetes_min` (quêtes terminées) ajoutée pour eux.
- Quêtes des héros (2026-10-20, `outils/villes/liens.py`) : après `aube_fin`, une par héros (`lien_lior`…, le héros doit
  être dans l'équipe : condition `membre`), un boss (4 recolorés : Brumeroi, Sentinelle d'aurore, Golem de magma, Ombre
  des galeries ; Varn et le Chevalier de glace sont des habitants qui lancent le combat) et une arme ou armure à lui seul :
  champ `pour` des objets (`ItemDef::owner`, `canEquip` ; « (pour Lior seulement) » dans `itemDetails` ; retirées des
  données en Expédition). L'événement `chapelle` garde son soin sur chaque page.
- Cartes au trésor (2026-10-20, `outils/villes/tresors.py`) : `Chest::cond` (champ `si` d'un coffre, `Game::chestShown` :
  caché tant que la condition est fausse, sauf déjà ouvert ; collision, dessin, Entrée, joueur automatique). Corbin, au
  port (quête `tresors`), donne la carte n°1 ; la description de chaque carte dit où est la cache (case la plus éloignée
  à pied des passages), et chaque trésor contient la carte suivante ; huit, puis la Boussole de Corbin.
- Cartes des gardiens (2026-10-20, `cartes.cpp`, `data/cartes_jeu.json`, `outils/villes/cartes.py`) : jeu de cartes à la
  Triple Triad. Carte = espèce (`cardValues` : Attaque, Vitesse, Défense, Magie de base ramenées de 1 à 10) ; collection
  (`cardCollection`) : cartes de départ, espèces obtenues (pas les humains ni les boss), drapeaux `carte:<espèce>` ; main
  (`cardHand_`, ligne `main_cartes`, complétée par les meilleures : `cardDeck`). `cards_` (comme la pêche : le jeu attend
  dans `update`, dessin `drawCards` par-dessus) ; règle `cardFlips` (+1 si le type est efficace), ordinateur `cardChoice`
  (niveaux 0 à 2 ; le joueur automatique joue avec le niveau 2) ; règle Identique (`regles`, `CardGame::same`) pour les trois
  derniers joueurs. Action `cartes` (menu `cardsMenu` : Jouer, Choisir ma
  main, Règles), première victoire : drapeau `cartes:<id>`, or et une carte de boss. Neuf joueurs (auberges, maisons),
  le Maître des cartes au port après les huit autres ; quête `cartes`, condition `cartes_min`.
- Échos des légendes (2026-10-20, `outils/villes/legendes.py`) : après `echos_fin`, la gardienne des échos de Port-Écho
  (événement `legendes`) : cinq cercles de trois boss de l'aventure d'affilée (N.98 à 120, comme la Ligue), drapeaux
  `legende1`…`5`, Médaillon des légendes, Couronne des échos ; quête `legendes`, 31 exploits.
- Pension (2026-10-20, `pension.cpp`, `outils/villes/pension.py`) : Mamie Rose à Moulinbourg (action `pension`). Deux
  créatures (`pension_`, `pensionLvl_` : niveau à l'arrivée) gagnent de l'expérience à chaque pas (`pensionStep` dans
  `arrive`, jamais au-dessus du plus haut niveau de l'équipe) ; deux d'un même type laissent parfois un œuf
  (`pensionEgg_`), emporté dans `eggs_` (espèce, pas restants) puis éclos (forme de base `baseForm`, niveau 1, plus
  souvent chromatique, drapeau `oeuf_eclos`). Règles `pension` de regles.json. Sauvegarde : `pension_niveaux`,
  `pension_pas`, `oeuf`, puis la ligne `pension` et ses `membre` (en dernier ; `Online::myTeam` s'y arrête). 32 exploits.
- Fête des lanternes (2026-10-20, `outils/villes/fete.py`) : condition `nuit` (`Game::isNight`, l'heure du jeu même dans
  un intérieur), Lison sur la place de la Vallée la nuit après `boss2`, cinq coffres `si` (quête et nuit) ; champ `fete`
  d'une carte (`MapDef::festival`) : des lanternes montent dans le ciel la nuit (`drawMap`).
- Graphismes (2026-10-21, « sans limite ») : relief des murs des grottes et des lieux de glace (`wallDepth`, sprites.cpp :
  dessus en texture sombre ou enneigé, face éclairée là où le mur touche le sol par en dessous, ombre au pied) ; roche
  propre à chaque donjon (`teinte_murs`, `MapDef::wallTint`, sol plus clair de la même teinte : `caveFloor`) ; lanterne
  des lieux obscurs en fondu de douze anneaux qui vacille, cristaux `x` qui brillent dans le noir. Captures
  `donjon_<carte>` du mode test. Combats : sol en perspective, brume d'horizon, vignette. Carte du monde illustrée (`drawWorldMap` :
  parchemin, rose des vents, paysage de chaque lieu découvert, noms des villages placés au moins encombré ;
  capture `carte_monde_fin`) Nom des lieux en ruban (bande qui s'efface, filets dorés, `textBig`, qui saute désormais les
  espaces) ; titre de l'écran d'accueil en `Gfx::textBigFancy` (contour, ombre, dégradé). Combats au bord de la mer
  (`Game::coastal` : plus de 15 % d'eau sur la carte) : la mer, une voile et le sable au fond ; mouettes sur ces cartes.
  Orage : éclair dessiné ; fontaine, fenêtres (ombre, coins arrondis), nombres des combats cernés de noir. Intérieurs :
  champ `style` (`MapDef::style`, `roomStyle` dans sprites.cpp : sols et murs propres à la chapelle, l'auberge, la
  boutique, la maison de soin). Champ `lumiere` (`MapDef::light`, voile de
  couleur le jour : Aube, Lueurs, Citadelle, Port-Écho, Forêt Pétrifiée, Cratère). Champ `eau` d'une carte (`MapDef::water`, `marais` : eau trouble et nénuphars) ; sol `z`
  du marais (boue, flaques, quenouilles éparses, bords fondus). Ponts, énigmes (rochers, plaques, herse), escaliers, cristaux, coffres ouverts redessinés. Second dossier de compilation `build-gfx` quand `build-dev` est occupé (démo ouverte). Menus (2026-10-23) :
  menu de pause en 12 entrées et quatre groupes (`MenuItem::sep`, filet) ; `prepMenu` (Échap > Préparation : Formation,
  Compagnons, Tactiques, Réserve) ; Discussions et Bestiaire dans le Journal (titre « Quêtes » avant la liste) ;
  icônes (`MenuItem::icon`, voir `MenuStack`) au menu de pause, aux commandes et techniques du combat, à l'écran titre,
  au Journal, à la boutique et au Bestiaire ; fines jauges sous les PV, PM, Exp. de la fiche d'équipe et sous les
  statistiques de la fiche du Bestiaire.
- Épreuves des gardiens (2026-10-20) : après `aube_fin`, la porte du sanctuaire d'Aube (événement `epreuves`) ouvre huit
  combats contre des esprits gardiens (drapeaux `epreuve1`…`epreuve8`, niveaux 70 à 84), puis Ilya (`epreuve9`,
  Insigne des gardiens) ; quête `epreuves`. Script `outils/villes/epreuves.py` ; une phase à mi-vie pour chaque gardien,
  le troll des Mines et les Rongecrins (`outils/villes/phases.py`, avec `carte.rewrite_events` qui ne réécrit que
  les entrées touchées d'evenements.json).
- Col des Hurlevents (2026-10-20, `outils/villes/col.py`) : carte `col` de l'histoire entre les Monts Cendrelune et
  les Pics Givrés (le passage du nord de Cendrelune y mène sous `boss2`, celui des Pics en revient), camp de bergers
  (soins, boutique), Harpie de givre (boss de carte `harpie_givre`, drapeau `harpie`, conseil) qui ouvre le défilé.
- Val des Murmures (2026-10-20, `outils/villes/val.py`) : carte `val` entre le sanctuaire du Givrecorne et la Source des
  Brumes (les passages des deux cartes y mènent) : hameau de Brumaille, gardienne des lanternes (quête `esprits`), trois
  esprits (`esprit_val_1..3`, un combat chacun) qui ouvrent le chemin de la Source.
- Plaine des Moulins (2026-10-20, `outils/villes/plaine.py`) : carte `plaine` entre la Vallée et les Monts Cendrelune
  (les passages des deux cartes y mènent) : village de Moulinbourg (soins, épicerie, meunier, charpentier), pont gardé
  par des passages sous condition `pont_repare` ; quête `pont` : chasser les Rongecrins (boss de carte `rongecrins`, si
  `quete:pont`, drapeau `rongecrins_chasses`), puis le charpentier répare le pont.
- Dresseurs des routes (2026-10-20, `outils/villes/dresseurs.py`) : 28 habitants avec `vue` 3 (événements et drapeaux
  `route_<carte><n>`) de la Vallée à la Citadelle : case choisie par le script (près d'un chemin, rien de bouché, loin
  des passages, bâtiments et énigmes ; accessibilité vérifiée), équipe tirée des créatures de leur zone (évoluées selon
  le niveau), or et parfois un objet. Revanches (`outils/villes/revanches.py`) : 37 dresseurs battus (ceux des routes et
  les anciens, sauf la mage du temple) proposent une revanche (question, drapeau `<drapeau>_revanche`) après `boss5`
  (actes 1-2, N.50-56) ou `aube_fin` (Aube, N.74-77) ; `questMark` rend 3 (bulle d'épées croisées) pour un combat
  proposé dans une question dont la victoire pose un drapeau pas encore posé.
- Ligue d'Écume (2026-10-20, `outils/villes/ligue.py`) : la maîtresse de la Ligue au nord du port (événement `ligue`,
  quête `ligue`) : rangs Bronze (`boss4`, N.47), Argent (`boss5`, N.57), Or (`aube_fin`, N.80), Champion (`epreuve9`,
  N.92), chacun trois combats enchaînés dans `victoire` (sans soin), drapeaux `ligue1`…`ligue4`, accessoires uniques.
- Île de Corail (2026-10-20, `outils/villes/corail.py`) : acte 2, passe à l'est du Récif vers la carte `corail`
  (village : soins, comptoir, doyenne, quête `trident`) et la Grotte des Marées (`marees`, donjon de cartegen.py,
  Gardien des marées, drapeau `marees_boss`, objet `trident`) ; le passage du Récif vers les Ruines demande le trident.
  Les outils de donjon (`dungeon`, `finish`) sont dans `outils/villes/cartegen.py`.
- Donjons optionnels (2026-10-20, `outils/villes/donjons.py`, salles et couloirs tirés d'une graine fixe, outil
  commun `outils/villes/cartegen.py`) : Mines de Forgeroc (`mines`, entrée à l'ouest de Forgeroc sous `boss1`, énigme),
  Glacier de Givreval (`glacier`, est des Pics, `boss2`, énigme), Épave du Léviathan (`epave`, sud du Récif, `boss4`),
  l'Abîme (`abime1` à `abime3`, ouest du Bois des Lueurs, `aube_fin`, escaliers `k`, super-boss Orage ancien N.100).
  Boss recolorés (troll_mines, colosse_givre, leviathan, orage_ancien), drapeau `<carte>_boss`, quêtes `mines`,
  `glacier`, `epave`, `abime`. Le Vieux Phare (`outils/villes/phare.py`, après l'acte 4) : `phare1` à `phare5` sous les
  ruines de Port-Écho (passage sous `echos_fin`), créatures N.90-102, Veilleur du phare N.110 (`phare5_boss`), quête `phare`.
- Pêche (2026-10-20, `peche.cpp`) : face à une case `~` avec l'objet `canne` (donné par le maître pêcheur du port,
  événement `maitre_pecheur`), `Game::interact` lance `startFishing` ; `fishing_` (0 attente, 1 ça mord : Entrée vite,
  2 ferrage : Entrée quand le curseur est dans la zone verte, `need` coups, 3 ratés = ligne cassée) ; le jeu attend
  pendant ce temps (`Game::update`), dessin `drawFishing` par-dessus la carte (`camX_`, `camY_`). Poissons :
  `data/peche.json` (`objet`, `eaux` : identifiants de carte d'abord, sinon thèmes ; `poids`, `difficulte`), objets de
  objets.json ; drapeau `peche:<id>`, `Game::fishCaught`, condition `poissons_min` ; quête `peche` (récompenses à 5, 10,
  15, 18 espèces). Script `outils/villes/peche.py`. Pas en Expédition ; le joueur automatique ne pêche pas.
- Langue (3.1) : `i18n.hpp` : `T("…")` (const char*) et `tr(std::string)` traduisent une phrase française avec
  `data/langues/<langue>.json` (absente : le français reste). `setLanguage` au démarrage (`options().english`, ligne
  `anglais` d'`options.txt` ; premier lancement : langue du système) et dans Options > Langue, puis `rebuildData()` et
  `loadMaps()` : noms et descriptions des données sont traduits à la construction des structures (`tr` dans data.cpp,
  world.cpp), les textes des événements et des quêtes à l'affichage (events.cpp, journal). Les documents de data/ restent
  en français ; les éditeurs (Réglages, cartes, histoire) sont désactivés en anglais. Le code affiche ses phrases avec
  `T("…")` (fragments pour les phrases composées) ; jamais pour un mot-clé de fichier ou de réseau (sauvegarde,
  `options.txt`, `en_ligne.txt`, messages) ni une étiquette comparée ailleurs sans `T` (le joueur automatique compare
  `T("Fuir")`…). procgen.cpp reste en français (le monde doit être identique chez tous). `python outils/textes.py`
  rassemble les phrases (T du code, tableaux de noms, textes des données) ; `BRUMEVAL_LANGUE=en brumeval --test …`
  liste à la fin les phrases affichées sans traduction et vérifie la mise en page en anglais ; depuis la 3.3 il doit
  aussi finir par « TOUS LES TESTS PASSENT » : les tests comparent les libellés à leur traduction (`T("…")` avec le
  morceau exact du code, `tr()` pour les données, `species(id).name`), et attendent les éditeurs fermés en anglais. Dans tactics.cpp et
  help.cpp, écrire `::T(` (un membre s'appelle `T`). Jamais `T(` dans un tableau global (rempli au chargement du
  programme, avant le choix de la langue : plantage sur Linux et Mac) : `N_("…")`, puis `tr()` à l'affichage.
- Données en mémoire : `dataDoc(DF_…)` garde chaque fichier de data/ sous forme
  de JSON ; `rebuildData()` reconstruit les structures du jeu à partir de ces
  documents, `saveDataDoc` les écrit. Les Réglages ne touchent qu'aux documents,
  jamais directement aux structures. Le mode test vérifie que réécrire un
  document sans changement redonne exactement le fichier.
- Souris : `Input` reçoit position, boutons (`mclick`, `mdown`) et molette
  (coordonnées déjà ramenées à 320x240 par SDL). `MenuStack::update` gère le
  survol et les clics. Raccourcis des éditeurs dans `Input` : `tab`, `undo`,
  `redo`, `saveKey` (Ctrl+Z/Y/S selon la disposition du clavier), `prev`/`next`
  (Page préc./suiv.), `del` (Suppr).
- Éditeurs : chacun a son mode (`Mode::Arena`, `Settings`, `Editor`, `Story`),
  ses menus dans `MenuStack` et ne modifie que la mémoire jusqu'à
  « Enregistrer ». Les tests en jeu (`editorTest_`, `storyTest_`) ajoutent
  « Fin du test » au menu de pause. Les identifiants créés passent par
  `makeSlug` (minuscules, sans accents, tirets bas).
- Saisie de texte : `Game::editText(titre, texte, max, rappel)` (SDL_TEXTINPUT
  transmis par main.cpp à `Game::onText`). Pendant la saisie, `onKey` ne sert
  qu'à écrire (Retour arrière, Entrée, Échap).
- Arène : mode `Mode::Arena` ; un combat lancé depuis l'Arène (`arenaBattle_`)
  y revient sans défaite « réelle ». La simulation crée des `Battle` en mode
  automatique et avance de quelques millisecondes par image. `BattleSetup::theme`
  impose un décor.
- Adversaires : `BattleSetup` (ennemis, `reserve` de renforts, `foeName`).
  Habitants avec `vue` : `Game::checkSight` les déclenche quand le joueur passe
  devant eux.
- Piège C++ : ne jamais écrire `for (auto& x : j.value(...).items())` (objet
  temporaire détruit avant la boucle) ; ranger d'abord le JSON dans une variable.
- Sous MSVC, l'option `/utf-8` (déjà dans CMakeLists.txt) est indispensable pour
  les accents. `/wd4244` coupe les centaines d'avertissements « int en float »
  des appels de dessin ; garder 0 avertissement de compilation.

### Modifier les données

Modifier les fichiers de `data/` puis relancer le jeu (pas besoin de recompiler).
Le mode test vérifie les références et que chaque porte, habitant, coffre,
panneau, passage et boss est accessible à pied (`checkMaps`). Les fichiers sont
réécrits par `writeJson` (petites listes sur une ligne) : garder ce format.

## Équilibrage

Mesures des boss, méthode et historique des réglages : compétence `equilibrage`. Un boss trop facile
vient souvent de sa lenteur ou d'acolytes trop faibles, pas de ses PV. Un boss qui frappe toute l'équipe à chaque tour
(Ouragan, Éclipse, Rayon sacré) écrase une équipe de six : lui donner plutôt des techniques à une cible ou en croix
(mesuré le 2026-10-20 sur le Phénix d'écho et l'Ombre du Premier Gardien, simulations du mode test ; elles se
font dans le décor de la carte du boss, avec sa météo).

## À faire / pistes

La suite du projet, étape par étape : `docs/PLANNING.md`.
