// Multijoueur (écran titre > Multijoueur) : un joueur héberge une partie, jusqu'à
// trois amis la rejoignent avec son adresse (réseau local, VPN comme Tailscale ou
// Radmin VPN, ou port ouvert sur la box). Tous doivent avoir la même version du jeu
// et les mêmes données. Les invités ne sont reliés qu'à l'hôte, qui fait suivre
// leurs messages aux autres (champ « a » : destinataire, « de » : expéditeur).
//
// Dans le salon, l'hôte lance :
// - un duel (à deux joueurs), chacun avec l'équipe de sa partie principale :
//   l'hôte calcule le combat (Battle, Net::Host), l'invité l'affiche et envoie ses
//   ordres (Net::Guest) ;
// - une expédition à plusieurs (coop.cpp) : le monde de la même graine pour tous ;
//   chacun explore, combat et capture de son côté et voit les autres sur la carte ;
//   le gardien de chaque région ne se combat qu'ensemble.
// Chacun peut aussi échanger des créatures avec un autre joueur (trade.cpp), au salon ou
// pendant l'expédition.
#pragma once
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "data.hpp"
#include "net.hpp"

class Game;
class Gfx;
class Expedition;
enum class BattleResult;

class Online {
 public:
  static constexpr int MAX_PLAYERS = 4;  // l'hôte et trois invités
  explicit Online(Game& g);
  bool onScreen() const { return screen_; }  // les écrans du multijoueur remplacent l'écran titre
  void menu(int sel = 0);
  void update(float dt);  // réseau, à chaque image (aussi pendant le combat)
  void draw(Gfx& g);
  void onBattleEnd(BattleResult r);  // fin d'un duel : retour au salon
  void leave();                      // ferme les connexions (retour à l'écran titre)

  // Expédition à plusieurs (coop.cpp)
  bool inGroup() const { return state_ == State::Expedition; }
  void guardian(const std::string& event);  // le joueur touche le gardien de sa région
  void runEnded();                          // son expédition s'arrête (défaite, abandon) : retour au salon
  void groupMenu();                         // menu de pause > Groupe : où en sont les autres
  struct Avatar {                           // un autre joueur dessiné sur la carte
    float x, y;                             // en cases (glisse d'une case à l'autre)
    int dir, step, look;
    std::string name;
    bool busy, ready;  // en combat ; prêt devant le gardien
  };
  std::vector<Avatar> avatars() const;  // les autres joueurs dans la même région

  // Échange de créatures (trade.cpp) : dans le salon (partie principale) ou pendant l'expédition
  void tradeMenu();  // choisir avec qui échanger, puis sa créature

  // Réglages gardés dans multijoueur.txt (le mode test utilise un autre fichier)
  std::string pseudo = "Joueur", address;
  std::string settingsName = "multijoueur.txt";
  int port = net::PORT;
  bool loopback = false;  // mode test : n'accepter que cet ordinateur (pas d'alerte du pare-feu)
  void loadSettings();
  void saveSettings() const;
  static std::string dataHash();  // empreinte des données du jeu (mêmes données des deux côtés)
  struct Member {  // un membre de l'équipe d'un autre joueur (duel)
    std::string sp;
    int lvl;
    Json gear;  // son équipement
  };

 private:
  friend class Game;  // le mode test pilote plusieurs joueurs
  enum class State { Menu, Joining, Salon, Duel, Expedition };
  // Un autre joueur, tel que ce joueur le connaît
  struct Player {
    int id = 0;
    std::string name;
    std::vector<Member> team;  // 3 premiers membres de sa partie principale (duel)
    // Expédition à plusieurs (message « ou ») : où il en est
    bool run = false, fighting = false, beaten = false;
    int region = 0, x = 0, y = 0, dir = 0;
    int ready = 0;     // région du gardien devant lequel il attend (0 : n'attend pas)
    int launched = 0;  // hôte : gardien déjà lancé avec lui (on attend qu'il ne soit plus « prêt »)
    std::string hero;  // vide : il choisit encore son héros
    Json fighters = Json::array();  // ses combattants pour le gardien
    float sx = 0, sy = 0;           // position affichée
  };
  // Une connexion : hôte, un par invité ; invité, une seule (l'hôte, numéro 0)
  struct Peer {
    std::unique_ptr<net::Conn> conn;
    int id = -1;
    bool greeted = false;  // version et données vérifiées
  };
  // Expédition lancée par l'hôte (graine, empreinte du monde, région où en est le groupe)
  struct Session {
    bool on = false;
    uint64_t seed = 0;
    std::string text, world;
    int region = 1;
  };
  Game& G;
  bool screen_ = false, host_ = false, sameLevel_ = false;
  State state_ = State::Menu;
  net::Server server_;
  std::vector<Peer> peers_;
  std::vector<Player> players_;  // les autres joueurs
  int myId_ = -1;                // 0 pour l'hôte ; attribué par l'hôte aux invités
  std::string me_;               // mon nom, tel que les autres le voient
  std::string status_, hash_;
  std::vector<std::string> addresses_;
  float stateT_ = 0;
  std::vector<std::pair<std::string, int>> myView_;  // mon équipe, pour l'affichage (nom, niveau)

  std::string file(const std::string& name) const;
  void refreshTeam();
  std::vector<FighterP> myTeam() const;  // équipe de la partie principale (3 premiers), ou équipe de départ
  void host();
  void join(const std::string& addr);
  Json hello() const;
  Peer* peer(int id);
  Player* player(int id);
  const Player* player(int id) const;
  std::string nameOf(int id) const;
  Player* opponent();  // duel : l'autre joueur, s'il n'y en a qu'un
  void sendTo(int to, Json m);  // to : numéro du joueur, -1 : tout le monde
  void send(const Json& m) { sendTo(-1, m); }
  void onMessage(int from, Json m);
  void onHello(int from, const Json& m);
  void applyPlayers(const Json& list);
  Json playersJson() const;
  void lost(int id, const std::string& why);  // connexion perdue avec ce joueur
  void hostLost(const std::string& why);      // invité : l'hôte est parti
  void salon(int sel = 0);
  void startDuel(int theme);
  void disconnect(const std::string& why);
  void waitMenu(const std::string& title, const std::string& help);

  // Expédition à plusieurs (coop.cpp)
  Session session_;
  std::set<uint64_t> ended_;    // graines dont l'expédition s'est arrêtée pour ce joueur (pas de retour)
  int ready_ = 0;               // région du gardien devant lequel ce joueur attend
  Json sent_;                   // dernier état envoyé (message « ou »)
  std::vector<int> battleIds_;  // joueurs du combat du gardien en cours (le chef d'abord)
  Expedition& expedition();
  void expeditionMenu(int sel = 0);
  void confirmLaunch(uint64_t seed, const std::string& text);       // une expédition sauvegardée serait remplacée
  void launch(uint64_t seed, const std::string& text, bool fresh);  // hôte : lance l'expédition pour tous
  void startRun(uint64_t seed, const std::string& text, int region, const std::string& world);
  void leaveRun();  // sauvegarde et remet les données du jeu (déconnexion)
  bool anyoneInRun() const;
  Json myStatus() const;
  Json myFighters() const;
  void syncGroup(float dt);  // envoie mon état s'il a changé ; fait glisser les autres joueurs
  void onStatus(int from, const Json& m);
  std::vector<int> missing(int k) const;  // autres joueurs qui doivent encore battre le gardien k
  std::string waitText(int id) const;     // « prêt », « région 2 », « en combat »…
  void waitGuardian(int k);
  void guardianWait();  // menu d'attente devant le gardien
  void checkGuardians();  // hôte : tout le groupe est prêt devant un gardien ?
  void launchGuardian(int k, const std::vector<int>& group);
  void onGuardian(const Json& m);  // début du combat du gardien, chez chaque joueur du groupe

  // Échange de créatures (trade.cpp). Celui qui propose donne une créature (en échange ou en
  // cadeau) ; l'autre répond (sa créature, « accepter » le cadeau ou refuser). Quand tout est
  // d'accord, celui qui a proposé envoie « go » : l'autre fait l'échange chez lui, puis envoie
  // « fait », et celui qui a proposé le fait à son tour (au pire un double, jamais une perte).
  struct Trade {
    enum class Step { None, Offered, Answering, Countered, Waiting } step = Step::None, shown = Step::None;  // shown : menu affiché
    int with = -1;      // l'autre joueur
    bool run = false;   // pendant l'expédition (sinon : partie principale, depuis le salon)
    bool asked = false; // c'est moi qui ai proposé
    bool gift = false;  // cadeau : rien en retour
    int slot = -1;      // ma créature (rang dans l'équipe ; -1 : aucune, je reçois un cadeau)
    std::string sp;     // son espèce (vérifiée juste avant l'échange)
    Json mine, theirs;  // les deux créatures (theirs : celle que je recevrai)
  };
  Trade trade_;
  bool canTrade(std::string* why = nullptr) const;  // je peux échanger maintenant ?
  bool partnerOk(const Player& p) const;            // ce joueur peut échanger maintenant ?
  bool loadMine(std::string& why);  // met mon équipe dans G.team (salon : lue dans la sauvegarde de la partie principale)
  bool commitTrade();               // fait l'échange chez moi et sauvegarde
  void pickCreature(int with);
  void offerTrade(int with, int slot, bool gift);
  void answerTrade(int slot);  // ma créature en retour (-1 : accepter le cadeau)
  // Fin sans échange : note pour moi ; tell (non vide) : prévient l'autre joueur
  void endTrade(const std::string& note, const std::string& tell = "");
  void onTrade(int from, const Json& m);
  void showTrade();    // menu de l'échange en cours
  void updateTrade();  // à chaque image : ouvre (ou rouvre) ce menu, arrête l'échange si l'autre est parti
  void tradeNote(const std::string& s);
};
