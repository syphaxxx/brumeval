// Aide (Échap > Aide, écran titre > Aide, ou F1 partout) : des pages qui expliquent les
// statistiques, les dégâts, le combat, le damier et les zones, le rythme, les états, le tableau
// des types et les touches. Les nombres viennent des règles (data/regles.json) : l'aide reste
// juste quand on règle le jeu. Méthodes de Game (game.hpp).
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "i18n.hpp"
#include "data.hpp"
#include "game.hpp"
#include "options.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), GREEN = rgb(0x7dffa8), RED = rgb(0xff8a7a),
                   DIM = rgb(0x5a6290);

// 0.75 → « 0,75 » ; 0.25 → « 25 % »
static std::string num(double x) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", x);
  std::string s = b;
  while (s.back() == '0') s.pop_back();
  if (s.back() == '.') s.pop_back();
  std::replace(s.begin(), s.end(), '.', ',');
  return s;
}
static std::string pct(double x) { return std::to_string(int(std::lround(x * 100))) + " %"; }

namespace {
struct HelpPage {
  std::string title, summary;
  std::vector<std::string> lines;  // vide : page spéciale (tableau des types)
};
}  // namespace

static std::vector<HelpPage> helpPages() {
  const Rules& R = rules();
  std::vector<HelpPage> p;
  p.push_back({::T("Nouveautés 3.7"), ::T("Les intentions des ennemis, la garde qui riposte, des combats plus durs mais lisibles."),
               {::T("Intentions : à la moitié de sa jauge, chaque ennemi annonce ce qu'il prépare (icône à côté de sa barre de PV, chevron orange et dégâts prévus au-dessus de l'allié visé, zone en orange au sol)."),
                ::T("Il fait ce qu'il a annoncé si c'est encore possible, sinon « Déjoué ! » : écartez la cible, mettez-la en garde, ou frappez-le d'abord. Un sablier : il va se mettre en garde, frappez-le de loin."),
                ::T("Attendre = se mettre en garde : un quart de dégâts en moins et une riposte à chaque coup au corps à corps jusqu'à votre prochain tour ; bouclier à côté de la tête et dans le tableau du bas."),
                ::T("Les ennemis visent les soigneurs, les lanceurs de sorts et les blessés, et s'acharnent sur un allié déjà visé : couvrez votre ligne arrière."),
                ::T("Tactiques : « Soi : visé » (le Protecteur se met en garde) et « Allié : visé » (il va le couvrir) ; chaque profil réanime et soigne, « (conseillé) » marque le profil qui convient."),
                ::T("Rencontres plus denses (0,6 créature par allié, jamais plus de 3 niveaux sous l'équipe), meutes de 2 créatures au début puis une de plus tous les 15 niveaux ; les créatures sauvages n'ont plus d'attaque sur toute l'équipe : une zone, que la formation évite."),
                ::T("Braisenard courait quatre fois trop vite depuis le 6 octobre (une coquille) : corrigé, et les boss réglés en conséquence."),
                ::T("Démo : le joueur automatique change de compagnons selon les créatures du lieu (Préparation > Réserve, Compagnons) et répète un boss dans le décor du combat."),
                ::T("En réseau, l'invité du duel et les autres joueurs du gardien voient aussi la garde et les intentions.")}});
  p.push_back({::T("Nouveautés 3.6"), ::T("Le regard, les embuscades, des créatures qui se battent autrement, les profils."),
               {::T("Le dos dépend du regard : chacun regarde vers sa dernière cible, vers où il a marché, ou vers l'adversaire le plus proche s'il attend. Une flèche au sol le montre."),
                ::T("En choisissant sa case, une dague dorée marque celles d'où l'on frappe un ennemi dans le dos."),
                ::T("Embuscades : parfois, des créatures surgissent au fond de votre camp, derrière l'équipe. Retournez-vous, ou protégez vos lanceurs de sorts."),
                ::T("Les rencontres sont à la mesure de l'équipe : plus de créatures contre une équipe nombreuse, jamais trop loin sous votre niveau."),
                ::T("Carapaces (Roche, Métal) et esprits encaissent les coups physiques ; certaines créatures encaissent la magie ; quelques-unes absorbent leur élément (le coup les soigne). Variez vos combattants."),
                ::T("Un adversaire bien plus faible que vous rapporte moins d'expérience : les boss s'affrontent au bon niveau."),
                ::T("Tactiques : nouvelles conditions (Ennemi : brisé, dos exposé, soigneur ou chef ; Soi : dos menacé) et actions (Viser une faiblesse, Se mettre en garde)."),
                ::T("Tactiques > Profils… : Combattant, Soigneur, Protecteur ou Briseur, des règles toutes faites pour un membre."),
                ::T("Menus : Échap > Préparation (formation, compagnons, tactiques, réserve) ; Discussions et Bestiaire dans le Journal ; la boutique compare l'équipement avec ce que porte chacun."),
                ::T("Démo (Outils) : le joueur automatique passe par les menus comme un joueur : équipement, gemmes, tactiques, formation.")}});
  p.push_back({::T("Nouveautés 3.5"), ::T("Primes, quêtes, éclats de brume, l'île d'Aube, musiques."),
               {::T("Primes : les tableaux d'annonces des villages affichent des monstres recherchés. Acceptez, chassez, touchez la prime, et montez en rang de chasseur."),
                ::T("Quêtes : plus de trente quêtes (Échap > Journal), dont un rival qui revient vous défier et une quête qui donne un compagnon."),
                ::T("Éclats de brume : trente éclats cachés dans de vieux coffres, aux quatre coins du monde. Une collectionneuse de la Vallée les récompense."),
                ::T("Après le Seigneur des Orages : une navigatrice du port vous emmène sur l'île d'Aube, pour un troisième acte."),
                ::T("L'histoire passe par de nouveaux lieux : la Plaine des Moulins, le Col des Hurlevents, le Val des Murmures, l'île de Corail."),
                ::T("Des dresseurs vous défient le long des routes : vingt-huit combats de plus, de l'or et des objets à la clé."),
                ::T("Après le Seigneur des Orages, les dresseurs battus proposent une revanche (épées croisées au-dessus d'eux)."),
                ::T("La Ligue d'Écume, au port : quatre rangs, trois combats d'affilée chacun, sans soin entre eux."),
                ::T("Après la Brume Noire : un quatrième acte sur l'île des Échos (la jetée ouest de Havre-d'Aube)."),
                ::T("Créatures chromatiques : très rarement, une créature sauvage a des couleurs rares. Capturez-la !"),
                ::T("La nuit, d'autres créatures sortent dans les herbes : de quoi compléter le Bestiaire."),
                ::T("Parfois, une meute sauvage est menée par un alpha : plus fort, il annonce ses attaques comme un boss."),
                ::T("Journal > Progression : tout ce qui reste à faire, en chiffres (quêtes, coffres, dresseurs, Bestiaire…)."),
                ::T("Sauvegarde automatique à chaque passage et porte (Options > Sauvegarde auto pour la couper)."),
                ::T("Voyage rapide : Échap > Carte, flèches pour choisir un village déjà visité, Entrée pour y aller."),
                ::T("Cinq donjons optionnels (Mines, Glacier, Épave, Abîme, Vieux Phare) et le grand bestiaire de la professeure de la Vallée."),
                ::T("Discussions : les héros se parlent au fil de l'aventure (Échap > Journal > Discussions)."),
                ::T("Exploits : trente-deux défis à relever, de la fin des actes au carnet de pêche complet (Échap > Journal > Exploits)."),
                ::T("Après l'acte 3, chaque héros a sa quête : quelqu'un de son passé, un adversaire, et une arme que lui seul peut porter."),
                ::T("Les cartes au trésor de Corbin, au port : chaque trésor caché contient la carte du suivant."),
                ::T("Cartes des gardiens : un jeu de cartes contre les joueurs des auberges. Chaque créature obtenue donne sa carte."),
                ::T("Après l'acte 4 : les Échos des légendes, à Port-Écho. Cinq cercles de trois grands boss d'affilée."),
                ::T("La pension de Moulinbourg : deux créatures y grandissent pendant que vous marchez, et laissent parfois un œuf."),
                ::T("Nouveaux graphismes : donjons en relief, carte du monde illustrée, intérieurs, météo, combats au bord de la mer."),
                ::T("Nouvelle partie + : après l'acte 3, Ilya propose de tout recommencer avec votre équipe, contre des créatures plus fortes."),
                ::T("Après l'acte 3 : les épreuves des gardiens, au sanctuaire d'Aube (huit combats puis Ilya)."),
                ::T("La pêche : le maître pêcheur du port donne une canne. Face à l'eau, Entrée ; dix-huit poissons à trouver."),
                ::T("Musiques réécrites, et de nouveaux morceaux : le port, les abysses, la tour, le dernier combat."),
                ::T("Il pleut parfois sur la vallée et les forêts ; un combat sous l'averse a la météo Pluie."),
                ::T("Un allié K.O. se voit enfin : couché, grisé, avec des étoiles. Les habitants ont des poses de dos et de profil.")}});
  p.push_back({::T("Nouveautés 3.4"), ::T("L'échiquier, les défis, les bottes : ce qui change."),
               {::T("Les deux camps partagent un seul plateau : on peut passer chez l'ennemi (page « Échiquier »)."),
                ::T("Se déplacer ne finit plus le tour : on avance, puis on agit. Revenir annule le déplacement."),
                ::T("Dans le dos, les coups font plus mal ; un combattant encerclé encaisse plus ; un sort lancé au contact d'un ennemi perd de sa force."),
                ::T("Percée : atteindre le bord adverse donne un bonus pour tout le combat."),
                ::T("Obstacles, bonds derrière la cible, échanges de cases, bottes qui font aller plus loin."),
                ::T("Tactiques : S'approcher, Contourner, Protéger. Formation : choisie sur le dessin du plateau."),
                ::T("Défis de l'échiquier : dans l'Arène, et auprès du maître d'armes de la Vallée et des instructeurs de Forgeroc et de Givreval.")}});
  p.push_back({::T("Statistiques"), ::T("PV, PM, Attaque, Défense, Magie, Résistance, Vitesse…"),
               {::T("PV (points de vie) : à 0, le combattant est K.O. Un soigneur ou une auberge rend tout."),
                ::T("PM (points de magie) : les sorts en coûtent. Les techniques sont gratuites."),
                ::T("Attaque : force des coups physiques. Défense : protège des coups physiques."),
                ::T("Magie : force des sorts et des soins. Résistance : protège des sorts."),
                ::T("Vitesse : remplit plus vite la jauge ATB, donc on agit plus souvent."),
                ::T("Précision : chance de toucher. Esquive : chance d'éviter un coup physique (jamais un sort)."),
                ::T("Critique : chance d'un coup critique (dégâts ×") + num(R.critMult) + ").",
                ::T("Niveau : toutes les statistiques montent avec lui. L'expérience va surtout aux combattants ; les autres membres de l'équipe en reçoivent une part."),
                ::T("Équipement (Échap > Équipement) : l'arme et l'armure (héros seulement) et l'accessoire ajoutent leurs bonus.")}});
  p.push_back({::T("Dégâts"), ::T("Comment se calculent les dégâts."),
               {::T("Coup physique : Attaque du lanceur contre Défense de la cible. Sort : Magie contre Résistance. La puissance de la technique et le niveau du lanceur comptent aussi."),
                ::T("Technique du même type que le lanceur : ×") + num(R.stab) + ".",
                ::T("Type efficace : ×2 (×4 si les deux types de la cible sont faibles). Peu efficace : ×0,5. Sans effet : 0. Voir la page Types."),
                ::T("Critique : ×") + num(R.critMult) + ::T(". Un peu de hasard : de ") + pct(R.spreadMin) + ::T(" à 100 % des dégâts."),
                ::T("Bonus et malus : de −3 à +3 par statistique, ") + pct(R.stageStep) + ::T(" de plus par niveau (un malus divise d'autant). Tout s'efface à la fin du combat."),
                ::T("Certaines créatures résistent en plus aux coups physiques, à la magie ou à un type : leur fiche du Bestiaire donne leurs faiblesses.")}});
  p.push_back({::T("Combat"), ::T("Jauge ATB, Limite, mode auto, objets, fuite."),
               {::T("Jauge ATB (trait blanc) : pleine, le combattant agit. Le temps s'arrête pendant que vous choisissez."),
                ::T("Compagnons : chaque héros combat avec sa créature. Leur jauge avance à ") + pct(R.companionAtb) + ::T(" de la vitesse normale."),
                ::T("Limite (trait rose) : se remplit quand on reçoit des coups. Pleine, la technique ultime du combattant est prête."),
                ::T("Mode auto (Tab) : chacun joue sa première tactique possible (Échap > Préparation > Tactiques)."),
                ::T("Objets : soins, remèdes. Une lanterne capture une créature sauvage : plus elle est affaiblie, mieux c'est."),
                ::T("Fuir : ") + pct(R.flee) + ::T(" de chance de réussir, pas contre tous les adversaires."),
                ::T("Changer : un autre membre de l'équipe prend la place d'un combattant.")}});
  p.push_back({::T("Brisure"), ::T("Le bouclier des ennemis, leurs faiblesses, l'ennemi brisé."),
               {::T("Chaque ennemi a un bouclier (le nombre à gauche de sa barre de vie) : ") + std::to_string(R.breakShield) + ::T(" points en général, ") +
                    std::to_string(R.breakShieldBoss) + ::T(" pour un boss."),
                ::T("Le frapper sur une faiblesse (un type efficace contre lui) retire un point, deux s'il est doublement faible. La faiblesse est alors découverte : une pastille de sa couleur s'affiche sous sa barre, et le Bestiaire la retient."),
                ::T("Bouclier à zéro : l'ennemi est BRISÉ. Sa jauge retombe à zéro, son attaque annoncée est annulée, il subit ×") + num(R.breakDamage) +
                    ::T(" de dégâts et perd son prochain tour. Puis il se ressaisit, bouclier rechargé."),
                ::T("Le bon moment pour les plus gros coups et les Limites : brisez d'abord, frappez ensuite."),
                ::T("Dans le menu de la cible, « faiblesse ! » signale un type déjà découvert ; « ? », un type pas encore essayé contre lui.")}});
  p.push_back({::T("Gemmes"), ::T("Sertir des gemmes dans l'équipement."),
               {::T("Chaque équipement porté a des chatons : ") + std::to_string(R.socketWeapon) + ::T(" pour une arme, ") + std::to_string(R.socketArmor) +
                    ::T(" pour une armure, ") + std::to_string(R.socketAccessory) + ::T(" pour un accessoire, un de plus forgé à +") + std::to_string(R.socketForge) + ".",
                ::T("Échap > Équipement > un membre > Gemmes : choisir un chaton, puis la gemme du sac (ou la retirer)."),
                ::T("Gemmes de technique : donnent un sort ou une technique (Feu, Soin, Rempart…), même à une créature."),
                ::T("Gemmes de bonus : un pourcentage de PV, d'Attaque ou de Magie en plus."),
                ::T("Gemmes de soutien : éveil (jauge avancée au début du combat), vampire (rend une part des dégâts infligés), riposte (rend les coups au corps à corps), brise-garde (use plus les boucliers)."),
                ::T("On les achète dans les boutiques ; chaque grand boss en laisse une. Changer d'équipement rend au sac les gemmes qui n'ont plus de place.")}});
  p.push_back({::T("Énigmes"), ::T("Les rochers à pousser."),
               {::T("Certaines salles ont des rochers ronds et des dalles gravées. Marchez dans un rocher pour le pousser d'une case (s'il y a de la place derrière)."),
                ::T("Quand chaque dalle porte un rocher, une grille s'ouvre quelque part : souvent le chemin vers un boss."),
                ::T("Un rocher coincé contre un mur ou dans un coin ne peut plus bouger : Échap > Recommencer l'énigme remet tout en place."),
                ::T("En sortant de la carte sans avoir fini, les rochers reviennent aussi à leur place.")}});
  p.push_back({::T("Rôles"), ::T("Les rôles des ennemis, les phases des boss."),
               {::T("Certaines créatures ont un rôle, montré par une icône à droite de leur barre de vie (et dans le menu de la cible) :"),
                ::T("Protecteur (écu gris) : ses voisins de damier (en croix) reçoivent ") + pct(R.roleGuard) + ::T(" de dégâts en moins. Abattez-le ou poussez sa cible loin de lui."),
                ::T("Soigneur (croix verte) : soigne ses alliés blessés au lieu d'attaquer. À éliminer en premier."),
                ::T("Chef (couronne) : tant qu'il est debout, la jauge ATB de ses alliés va ") + pct(R.roleChief) + ::T(" plus vite."),
                ::T("Vos créatures protectrices et chefs aident aussi votre équipe."),
                ::T("Les boss ont des phases : sous un seuil de PV, ils changent (bouclier rechargé, bonus, nouvelles techniques, autre météo, renforts). Gardez de quoi encaisser leur seconde forme.")}});
  p.push_back({::T("Pousser"), ::T("Pousser et tirer les combattants sur le damier."),
               {::T("Certaines techniques poussent la cible d'une case vers l'arrière (Charge, Ruée, Rafale, Vague, Fracas…) ou la tirent vers l'avant (Flèche-grappin, Tornade, Étreinte…). Leur description le précise."),
                ::T("Si la case d'arrivée est déjà prise, les deux combattants se heurtent : chacun subit un choc de ") + pct(R.shoveClash) +
                    ::T(" des dégâts du coup. Poussé contre le bord arrière : choc aussi."),
                ::T("Tirer un mage vers l'avant le met à portée du corps à corps ; pousser un ennemi sur des braises le brûle aussitôt."),
                ::T("Les boss sont inébranlables. Les ennemis aussi poussent et tirent : gardez vos soigneurs loin des tireurs de grappin.")}});
  {
    std::vector<std::string> dl = {::T("Deux alliés qui vont ensemble lancent une technique à deux, plus forte (menu « Duo »). Le partenaire doit avoir sa jauge ATB à ") +
                                       std::to_string(R.duoGauge) + ::T(" % au moins : il la dépense, et chacun paie les PM."),
                                   ::T("En mode auto, la tactique « Meilleure attaque » choisit un duo quand il vaut le coup.")};
    for (auto& d : R.duos) {
      if (!hasMove(d.move) || d.members.size() != 2) continue;
      auto who = [](const std::string& m) {
        if (m == "humain") return std::string(::T("un héros"));
        if (m.rfind("type:", 0) == 0) {
          for (auto& t : types())
            if (t.id == m.substr(5)) return ::T("une créature ") + t.name;
          return m;
        }
        return hasSpecies(m) ? species(m).name : m;
      };
      dl.push_back(moveInfo(d.move).name + " : " + who(d.members[0]) + " + " + who(d.members[1]) + ".");
    }
    p.push_back({::T("Duos"), ::T("Les techniques à deux."), dl});
  }
  {
    std::vector<std::string> ml = {::T("Chaque combat a un temps, selon le lieu (affiché sous l'ordre des tours) :")};
    for (auto& w : R.weathers) ml.push_back(w.name + " : " + w.desc);
    ml.push_back(::T("Des cases du damier ont un terrain, de leur couleur. Dans « Se déplacer », chaque case dit le sien :"));
    for (auto& t : R.terrains) ml.push_back(t.name + " : " + t.desc);
    ml.push_back(::T("En mode auto, la tactique « Se mettre à l'abri » quitte aussi une case qui blesse ou ralentit."));
    p.push_back({::T("Météo"), ::T("Le temps et les cases de terrain."), ml});
  }
  {
    std::vector<std::string> tl = {::T("Chaque espèce a un talent, un effet permanent (fiche d'un membre : Échap > Équipe ; Bestiaire). Une évolution le garde.")};
    for (auto& t : R.talents) tl.push_back(t.name + " : " + t.desc);
    p.push_back({::T("Talents"), ::T("Le pouvoir passif de chaque espèce."), tl});
  }
  p.push_back({::T("Échiquier"), ::T("Un seul plateau pour les deux camps : se placer, avancer, passer derrière."),
               {::T("Les deux camps partagent un plateau de ") + std::to_string(BOARD_W) + ::T(" colonnes sur ") + std::to_string(GRID) +
                    ::T(" lignes : votre équipe part de la gauche, les ennemis de la droite. Chacun peut passer de l'autre côté."),
                ::T("Se déplacer : une fois par tour, avant d'agir, jusqu'à ") + std::to_string(R.moveRange) +
                    ::T(" cases (une de plus pour les types Vent et Foudre). On traverse ses alliés, pas les ennemis, et on s'arrête en arrivant à leur contact. Les types Vent et Esprit volent par-dessus tout."),
                ::T("Corps à corps : un ennemi au contact (les 8 cases autour ; un boss frappe plus loin). Si la cible est plus loin mais qu'on peut la rejoindre, on s'approche d'abord."),
                ::T("À distance (sorts, flèches, jets…) : n'importe quelle case, mais seulement ") + pct(R.contact) + ::T(" des dégâts quand un ennemi est au contact du lanceur."),
                ::T("Dans le dos : chacun regarde vers sa dernière cible, vers où il a marché, ou vers l'adversaire le plus proche s'il attend ; le frapper au corps à corps depuis l'autre côté fait ×") + num(R.backStab) + ::T(" de dégâts. Encerclé : chaque adversaire au contact au-delà du premier ajoute ") + pct(R.surround) + ::T(" aux dégâts reçus (pas un boss : trop grand pour être encerclé)."),
                ::T("Percée : le premier qui atteint le bord du camp adverse gagne +") + std::to_string(R.breakthrough) + ::T(" en Attaque, Magie et Vitesse pour le combat (les ennemis aussi : bloquez-les !)."),
                ::T("Une ligne de combattants protège ceux de derrière : les ennemis doivent la contourner. Attendre : se mettre en garde jusqu'à son prochain tour (un quart de dégâts en moins, une riposte à chaque coup au corps à corps, face à l'ennemi le plus proche), et rejouer plus tôt."),
                ::T("Intentions : à la moitié de sa jauge, un ennemi annonce ce qu'il prépare (icône à côté de sa barre de PV, chevron orange sur sa cible, zone en orange au sol, pointillé vers sa cible quand on le survole). Écartez la cible, mettez-la en garde, ou frappez-le d'abord. Un sablier : il va se mettre en garde, frappez-le de loin."),
                ::T("Embuscade : parfois, une partie des créatures surgit au fond de votre camp, dans votre dos. Retournez-vous ou protégez vos lanceurs de sorts."),
                ::T("Obstacles : rochers, buissons ou tonneaux au milieu du plateau. Personne ne s'y arrête ni ne les traverse (sauf en volant) ; une cible poussée contre l'un d'eux subit un choc."),
                ::T("En choisissant sa case : le chemin s'affiche, des losanges rouges comptent les ennemis qui pourront y frapper, une dague dorée marque les cases d'où l'on frappe un dos. La flèche au sol montre où chacun regarde. « Revenir » annule le déplacement."),
                ::T("Certaines techniques bondissent derrière la cible ou échangent les cases. Les bottes (accessoires) font avancer d'une case de plus. Tactiques : S'approcher, Contourner, Protéger."),
                ::T("Pour s'entraîner : Arène > Défis de l'échiquier (positions imposées, une leçon chacune)."),
                ::T("Formation (Échap > Préparation > Formation) : la case de chacun au début du combat, dans votre moitié. Les costauds devant, les lanceurs de sorts derrière.")}});
  p.push_back({::T("Zones"), ::T("Ligne, colonne, croix, tout le camp ; attaques annoncées."),
               {::T("Une cible : seulement la case visée."),
                ::T("Ligne : la case visée et toute sa ligne, d'un bord du plateau à l'autre."),
                ::T("Colonne : la case visée et toute sa colonne, de haut en bas."),
                ::T("Croix : la case visée et ses voisines (dessus, dessous, devant, derrière)."),
                ::T("Tout le camp : tous les ennemis (ou tous les alliés), sans viser."),
                ::T("Autour de la case visée, une zone fait ") + pct(R.zoneSide) + ::T(" des dégâts. En visant, les cases touchées s'allument en or : des ennemis serrés sont une belle cible, vos combattants aussi !"),
                ::T("Attaques annoncées : un boss marque parfois une zone en rouge. À son tour suivant, il frappe ×") + num(R.bossBurst) + ::T(" ceux qui y sont encore : écartez-vous.")}});
  p.push_back({::T("Rythme"), ::T("Techniques rapides, normales et lourdes."),
               {::T("Normale : après l'action, la jauge ATB repart de zéro."),
                ::T("Rapide : la jauge repart à ") + std::to_string(R.quickGauge) + ::T(" % : on rejoue plus tôt. Souvent plus faible, parfois avec un effet en plus."),
                ::T("Lourde : la jauge repart à −") + std::to_string(R.heavyDelay) + ::T(" % : on rejoue plus tard. Ce sont les plus fortes."),
                ::T("Se déplacer est toujours rapide."),
                ::T("Chaque technique dit son rythme dans sa description (en bas de l'écran quand on la choisit).")}});
  // États : effets chiffrés, puis les types insensibles
  std::vector<std::string> st = {
      ::T("Poison : perd ") + pct(R.poisonDmg) + ::T(" de ses PV max à la fin de chacun de ses tours."),
      ::T("Brûlure : perd ") + pct(R.burnDmg) + ::T(" de ses PV max par tour, et son Attaque ×") + num(R.burnAtk) + ".",
      ::T("Paralysie : Vitesse ×") + num(R.paraSpeed) + ::T(" et ") + pct(R.paraSkip) + ::T(" de chance de perdre son tour."),
      ::T("Sommeil : ne fait rien pendant ") + std::to_string(R.sleepMin) + ::T(" à ") + std::to_string(R.sleepMax) + ::T(" tours."),
      ::T("Un seul état à la fois. Un Remède ou un sort de guérison l'enlève, et tout s'efface à la fin du combat.")};
  std::string imm;
  for (auto& t : types())
    if (!t.immune.empty()) {
      std::string list;
      for (auto& s : t.immune) list += (list.empty() ? "" : ", ") + std::string(statusName(statusOf(s)));
      imm += (imm.empty() ? "" : " ; ") + t.name + " : " + list;
    }
  if (!imm.empty()) st.push_back(::T("Insensibles : ") + imm + ".");
  p.push_back({::T("États"), ::T("Poison, brûlure, paralysie, sommeil."), st});
  p.push_back({::T("Types"), ::T("Le tableau des forces et faiblesses entre les types."), {}});
  {
    int n = 0, lo = 99, hi = 0;
    for (auto& s : allSpecies())
      if (!s.evolveTo.empty()) n++, lo = std::min(lo, s.evolveLvl), hi = std::max(hi, s.evolveLvl);
    if (n)
      p.push_back({::T("Évolution"), ::T("Les créatures qui deviennent plus fortes."),
                   {std::to_string(n) + ::T(" créatures peuvent évoluer, du niveau ") + std::to_string(lo) + ::T(" au niveau ") + std::to_string(hi) +
                        ::T(" selon l'espèce (fiche du Bestiaire : « Évolue en … »)."),
                    ::T("Après le combat où elle atteint ce niveau, le jeu demande « Évoluer ? ». Elle devient une espèce plus forte, un peu plus grande, et apprend de nouvelles techniques."),
                    ::T("Elle garde son niveau, son expérience, son équipement, ses tactiques, sa place sur le damier et le héros qu'elle accompagne."),
                    ::T("Vous pouvez répondre Non (une petite créature est plus rapide à faire progresser) : Échap > Évolution la fait évoluer plus tard."),
                    ::T("Les formes évoluées vivent aussi à l'état sauvage dans les régions les plus lointaines.")}});
  }
  {
    std::vector<std::string> fl = {::T("À Forgeroc, une fois Brann dans l'équipe, la forge renforce une arme ou une armure (portée ou dans le sac).")};
    for (size_t L = 0; L < R.forge.size(); L++) {
      std::string need;
      for (auto& [id, n] : R.forge[L].items) need += ", " + std::to_string(n) + " " + (id == "trophee" ? std::string(::T("trophée de boss")) : item(id).name);
      fl.push_back("+" + std::to_string(L + 1) + ::T(" : bonus de l'objet +") + pct(R.forge[L].bonus) + ::T(" (arrondi au-dessus). Coût : ") +
                   std::to_string(R.forge[L].gold) + ::T(" or") + need + ".");
    }
    std::string drops;
    for (auto& td : R.typeDrops) {
      std::string tn = td.type;
      for (auto& t : types())
        if (t.id == td.type) tn = t.name;
      drops += (drops.empty() ? "" : ", ") + item(td.item).name + ::T(" (créatures ") + tn + ")";
    }
    if (!drops.empty()) fl.push_back(::T("Matériaux : butin des combats, ") + drops + ::T(", et quelques coffres."));
    fl.push_back(::T("Chaque boss vaincu laisse un trophée. Un objet forgé se revend plus cher."));
    p.push_back({::T("Forge"), ::T("Renforcer armes et armures à Forgeroc."), fl});
  }
  p.push_back({::T("Exploration"), ::T("Courir, carte du monde, quêtes, Journal."),
               {::T("Courir : Maj maintenue (manette : X). Le compagnon suit une case derrière."),
                ::T("Carte du monde (Échap > Carte du monde) : les lieux où vous pouvez aller, et où vous êtes."),
                ::T("Une bulle « ! » au-dessus d'un habitant : il propose une quête. Une bulle « ? » dorée : vous pouvez la lui rendre."),
                ::T("Journal (Échap > Journal) : les quêtes en cours et terminées, et un conseil pour la suite."),
                ::T("Hautes herbes, forêts et grottes cachent des créatures sauvages : plus on s'éloigne des villes, plus elles sont nombreuses."),
                ::T("Coffres, panneaux et habitants : Entrée devant eux.")}});
  const auto& K = options().keys;
  p.push_back({::T("Touches"), ::T("Clavier, raccourcis F1, F2, F11."),
               {::T("Se déplacer, choisir : flèches ou ") + keyName(K[K_UP]) + keyName(K[K_LEFT]) + keyName(K[K_DOWN]) + keyName(K[K_RIGHT]) + ".",
                ::T("Valider, parler : Entrée ou ") + keyName(K[K_OK]) + ::T(". Retour, menu : Échap ou ") + keyName(K[K_BACK]) + ".",
                ::T("Mode auto en combat : ") + keyName(K[K_AUTO]) + ".",
                ::T("Courir : Maj maintenue (manette : X)."),
                ::T("En combat, la cible et la case se choisissent sur le damier : flèches ou souris."),
                ::T("F1 : cette aide. F2 : vitesse du jeu ×1, ×2 ou ×3. F11 : plein écran."),
                ::T("Les touches se changent dans Options > Touches. Une manette branchée marche toute seule.")}});
  return p;
}

void Game::helpMenu(int sel) {
  auto pages = helpPages();
  int panel = panelMode_;
  panelMode_ = 0;  // le menu prend la place du résumé de l'équipe
  Menu m;
  m.title = ::T("Aide");
  m.x = 96, m.y = 30, m.w = 128, m.rows = std::min(9, (int)pages.size() + 1), m.sel = sel;  // au-delà, la liste défile
  for (size_t i = 0; i < pages.size(); i++)
    m.items.push_back({pages[i].title, "", pages[i].summary + ::T(" (aussi avec F1)"), true, [this, i] { openHelp((int)i); }});
  m.items.push_back({::T("Retour"), "", "", true, [this, panel] {
                       panelMode_ = panel;
                       menus.pop();
                     }});
  m.onCancel = m.items.back().act;
  menus.push(m);
}

bool Game::tip(const std::string& id, Script& s) {
  if (tipsOff_ || !options().tips || pilot_ || tipsSeen.count(id)) return false;
  for (auto& t : rules().tips) {
    if (t.id != id) continue;
    tipsSeen.insert(id);
    auto pages = helpPages();
    for (size_t i = 0; i < pages.size(); i++)
      if (pages[i].title == t.help) lastHelp_ = (int)i;
    s.say(::T("Astuce : ") + t.text + ::T(" (F1 : en savoir plus)"), 0);
    return true;
  }
  return false;
}

std::string Game::adviceText() const {
  for (auto& a : rules().advice) {
    if (has(a.flag) || !hasSpecies(a.boss)) continue;
    const Species& s = species(a.boss);
    std::string t = ::T("Prochain gardien : ") + s.name + (a.place.empty() ? "" : ::T(", dans ") + a.place) + ". ";
    std::string known;
    for (int ty = 0; ty < (int)types().size(); ty++)
      if (weakKnown(a.boss, ty)) known += (known.empty() ? "" : ", ") + types()[ty].name;
    t += known.empty() ? ::T("Ses faiblesses sont à découvrir : essayez chaque type, le Bestiaire s'en souviendra. ")
                       : ::T("Faiblesses connues : ") + known + ". ";
    t += a.text;
    if (!a.gem.empty() && hasItem(a.gem)) t += ::T(" Gemme conseillée : ") + item(a.gem).name + ".";
    return t;
  }
  return "";
}

void Game::openHelp(int page) {
  helpPage_ = std::clamp(page, 0, (int)helpPages().size() - 1);
  helpScroll_ = 0;
  in.endFrame();
}

// Pendant l'aide, le reste du jeu attend. Gauche / droite : page précédente / suivante ;
// haut / bas : faire défiler une page longue ; Entrée, Échap ou un clic : fermer.
void Game::updateHelp() {
  int n = (int)helpPages().size();
  if (in.press[LEFT]) helpPage_ = (helpPage_ + n - 1) % n, helpScroll_ = 0;
  if (in.press[RIGHT]) helpPage_ = (helpPage_ + 1) % n, helpScroll_ = 0;
  if (in.press[UP] || in.wheel > 0) helpScroll_ = std::max(0, helpScroll_ - 1);
  if (in.press[DOWN] || in.wheel < 0) helpScroll_++;  // borné au dessin
  if (in.confirm || in.cancel || in.mclick[0]) {
    if (menus.active() && menus.top().title == ::T("Aide")) menus.top().sel = helpPage_;
    helpPage_ = -1;
  }
}

void Game::drawHelp() {
  auto pages = helpPages();
  if (helpPage_ < 0 || helpPage_ >= (int)pages.size()) return;
  const HelpPage& p = pages[(size_t)helpPage_];
  g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x000010, 150));
  g.newLayer();
  g.window(4, 4, 312, 232);
  g.text(160, 9, ::T("Aide · ") + p.title + "  (" + std::to_string(helpPage_ + 1) + "/" + std::to_string(pages.size()) + ")", GOLD, 1);
  g.text(160, 222, ::T("Gauche / droite : page · Échap : fermer"), MUTED, 1);
  if (!p.lines.empty()) {
    std::vector<std::pair<std::string, bool>> rows;  // ligne, début de paragraphe
    for (auto& l : p.lines) {
      auto w = Gfx::wrap(l, 288);
      for (size_t i = 0; i < w.size(); i++) rows.push_back({w[i], i == 0});
    }
    const int visible = 17;
    helpScroll_ = std::clamp(helpScroll_, 0, std::max(0, (int)rows.size() - visible));
    for (int i = 0; i < visible && helpScroll_ + i < (int)rows.size(); i++) {
      auto& [s, first] = rows[(size_t)(helpScroll_ + i)];
      float y = 24 + i * 11;
      if (first) g.text(12, y, "·", GOLD);
      g.text(20, y, s, WHITE);
    }
    if ((int)rows.size() > visible) g.text(306, 210, helpScroll_ + visible < (int)rows.size() ? "▼" : " ", GOLD, 2);
    return;
  }
  // Tableau des types : ligne = type de l'attaque, colonne = type de la cible
  const auto& T = types();
  int n = (int)T.size();
  const float cw = std::min(21.f, 272.f / std::max(1, n)), x0 = 38, y0 = 24;
  for (int d = 0; d < n; d++) g.text(x0 + d * cw + cw / 2, y0, utf8Prefix(T[d].name, 3), rgb(T[d].color), 1);
  for (int a = 0; a < n; a++) {
    float y = y0 + 12 + a * 12;
    if (a % 2 == 0) g.rect(10, y - 2, 300, 12, rgb(0xffffff, 14));
    g.text(12, y, utf8Prefix(T[a].name, 3), rgb(T[a].color));
    for (int d = 0; d < n; d++) {
      float e = typeEff(a, d), x = x0 + d * cw + cw / 2;
      if (e > 1) g.text(x, y, "2", GREEN, 1);
      else if (e == 0) g.text(x, y, "0", MUTED, 1);
      else if (e < 1) g.text(x, y, "-", RED, 1);
      else g.text(x, y, "·", DIM, 1);
    }
  }
  float ly = y0 + 16 + n * 12;
  g.text(160, ly, ::T("Ligne : l'attaque · colonne : la cible"), MUTED, 1);
  g.text(20, ly + 12, "2", GREEN);
  g.text(30, ly + 12, ::T("efficace ×2"), WHITE);
  g.text(112, ly + 12, "-", RED);
  g.text(122, ly + 12, ::T("peu efficace ×0,5"), WHITE);
  g.text(232, ly + 12, "0", MUTED);
  g.text(242, ly + 12, ::T("sans effet"), WHITE);
}
