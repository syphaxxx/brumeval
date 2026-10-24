---
name: multijoueur
description: "Brumeval : protocole du multijoueur (net, online, coop, trade, upnp) : messages, duel, expédition à plusieurs, échanges, code de partie. À lire avant de toucher au réseau."
---

# Multijoueur de Brumeval

- Code de partie (2.3, plan validé le 2026-10-12, sans serveur) : `Online::host` lance `upnp::open(port)` (sauf en
  boucle locale du mode test) : la box fait suivre le port et donne son adresse Internet ; le panneau de droite affiche
  le code (adresse IPv4 + octet de contrôle, 8 caractères Crockford « XXXX-XXXX »), « Copier le code » (salon) le met
  dans le presse-papiers. Échec (pas d'UPnP, refus, adresse privée ou partagée 100.64/10) : message en bas avec les
  solutions (UPnP dans la box, Tailscale). « Rejoindre » accepte un code ou une adresse. `disconnect` referme le port.
  Bibliothèque miniupnpc 2.3.3 (FetchContent, `libminiupnpc-static`) ; `net::startup` (WSAStartup) avant la recherche.
  Vérifié le 2026-10-12 sur la box de l'utilisateur (`--upnp` : port ouvert puis refermé). Piste suivante si des amis
  ont une box sans adresse à elle : un serveur relais (payant, à décider).
- Multijoueur (online.hpp, net.hpp) : TCP sur le port 47474, l'hôte (numéro 0)
  et jusqu'à trois invités (1 à 3), reliés seulement à l'hôte : `sendTo(à qui,
  message)` ajoute `de` (expéditeur) et, chez un invité, `a` (destinataire, -1 :
  tous) ; l'hôte fait suivre. Chacun envoie `bonjour` (version, empreinte des
  données, pseudo, équipe : 3 premiers membres de la partie principale) ;
  l'hôte répond `bienvenue` (numéro, liste des joueurs, expédition en cours) et
  envoie `joueurs` aux autres. Le duel n'est possible qu'à deux. En duel, l'**hôte**
  calcule tout le combat (`Battle::goOnline`, `Net::Host` : les « ennemis » sont
  l'équipe de l'invité ; quand la jauge de l'un d'eux est pleine, il envoie
  `tour` et attend `ordre`, qu'il vérifie) et envoie son état dix fois par
  seconde (`netState` : PV, PM, jauges, états, message en cours), plus les
  nombres flottants, éclairs et clignotements. L'**invité** (`Net::Guest`) ne
  fait pas avancer le temps : il affiche cet état et répond à `tour` par son
  menu ou ses tactiques ; `useMove` envoie alors l'ordre au lieu de le jouer.
  Un combattant dans un message : `[0, rang]` pour l'équipe de l'expéditeur,
  `[1, rang]` pour l'autre (`Battle::mine`, `Battle::other`). Pas d'objets, ni
  remplaçants, ni expérience en duel. Le mode test lance deux `Game` reliés
  par 127.0.0.1 (port 47475), joue un duel complet puis une expédition à deux.
  Augmenter `BRUMEVAL_VERSION` à chaque version publiée (et si les messages
  changent).
- Expédition à plusieurs (coop.cpp) : l'hôte envoie `expedition` (graine,
  empreinte du monde `Expedition::worldHash`, région du groupe) ; chacun joue sa
  propre `Expedition` (`group()`, sauvegarde `expedition_groupe_<pseudo>.txt`,
  `groupTag` : deux fenêtres sur le même ordinateur ne se gênent pas) : il
  reprend sa sauvegarde de cette graine, sinon part avec un nouveau héros de la
  région du groupe. Chacun envoie son état quand il change (`ou` : région, case,
  héros, gardien vaincu, en combat, `p` : prêt devant le gardien avec ses
  combattants). `Game::bossTouched` : le gardien attend tous ceux qui ne l'ont
  pas encore battu (`Online::missing`) ; l'hôte vérifie (`checkGuardians`) puis
  envoie `gardien` (groupe, combattants, chef = plus petit numéro). Chez chacun,
  le combat a les mêmes alliés dans le même ordre (`BattleSetup::allies` : ses
  vrais combattants, des copies pour ceux des autres ; 3 seul, 2 chacun à deux,
  1 chacun à trois ou quatre ; PV des ennemis × `guardianScale` : alliés/3
  au-delà de trois, puis +`coopGuardian` (règle `multijoueur.gardien_par_joueur`,
  0.15) par joueur en plus ; l'équipement de chacun passe dans `m` → `eq`). Le
  chef (`Net::Lead`) calcule le combat comme l'hôte d'un duel, envoie `tour`
  au joueur de l'allié (`Battle::owner`), vérifie son `ordre` (technique, ou
  objet déjà pris dans son sac), envoie `renfort` et `issue` ; ceux qui suivent
  (`Net::Follow`) jouent alors la fin chez eux (`victory` : expérience de leurs
  combattants, or). Joueur parti : l'ordinateur joue ses combattants
  (`dropPlayer`) ; chef parti : le combat s'arrête sans conséquence. Une
  défaite en solo ramène au village (`Expedition::knockedOut`, moitié de l'or) ;
  `groupBattle_` : défaite contre le gardien, l'expédition s'arrête
  (`onDefeat`, puis `Online::runEnded` : retour au salon). Dans le mode test,
  les deux `Game` partagent les données en mémoire : l'invité met de côté les
  vraies données (celles de l'hôte), voir `shareBackup`.
  Le monde généré doit être identique partout : `-ffp-contract=off` (GCC,
  Clang), aucun `sin`/`pow`/`std::shuffle`/`unordered_map` dans procgen.cpp,
  et jamais deux tirages au hasard dans les arguments d'un même appel (Clang
  les fait de gauche à droite, MSVC et GCC de droite à gauche) ;
  le mode test compare l'empreinte de la graine 2026 à une valeur fixe (à
  mettre à jour si on change le générateur).
- Échange de créatures (trade.cpp, `Online::trade_`) : message `echange`, champ
  `e` : `offre` (créature, `cadeau`, `run` : pendant l'expédition), puis
  `contre` (créature en retour) ou `accepte` (cadeau), `fait`, `refus`
  (`raison`). Quand tout est d'accord, celui qui a proposé envoie `go` : l'autre
  fait l'échange chez lui (`commitTrade`) et répond `fait`, puis celui qui a
  proposé le fait à son tour (une coupure donne au pire un double, jamais une
  perte). Une créature dans
  un message : `creatureJson` / `creatureFrom` (vérifiée : espèce connue ici,
  pas un humain). Au salon, l'équipe est lue et réécrite dans la sauvegarde de
  la partie principale (`loadMine` = `Game::loadGame`, puis `saveGame`) ;
  pendant l'expédition, c'est `G.team`, sauvegardée aussitôt. Le menu de
  l'échange (titre « Échange : … ») s'ouvre dans `updateTrade` dès que le joueur
  est libre (ni combat, ni dialogue) ; l'échange s'arrête si l'autre part.

**2026-10-24 (branche jeu-tactique, à publier avec un `BRUMEVAL_VERSION` plus grand)** : `etat` porte pour chaque
combattant un 11e nombre (1 : en garde) et une liste `i` des intentions des ennemis (`f` indice de l'ennemi, `m`
technique, `c` case annoncée, `t` indices des alliés visés, `w` garde annoncée) ; l'invité et les autres joueurs du
gardien les dessinent comme le chef (icône, chevrons, dégâts prévus, ATH). Test : la garde de l'hôte se voit chez
l'invité du duel.
