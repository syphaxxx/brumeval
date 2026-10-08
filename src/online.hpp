// Multijoueur (écran titre > Multijoueur) : un joueur héberge, l'autre le rejoint
// avec son adresse (réseau local, VPN comme Tailscale ou Radmin VPN, ou port
// ouvert sur la box). Ils doivent avoir la même version du jeu et les mêmes
// données. Puis duel en ligne, chacun avec l'équipe de sa partie principale :
// l'hôte calcule le combat (Battle, mode Net::Host), l'invité l'affiche et envoie
// ses ordres (Net::Guest).
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "data.hpp"
#include "net.hpp"

class Game;
class Gfx;
enum class BattleResult;

class Online {
 public:
  explicit Online(Game& g);
  bool onScreen() const { return screen_; }  // les écrans du multijoueur remplacent l'écran titre
  void menu(int sel = 0);
  void update(float dt);  // réseau, à chaque image (aussi pendant le combat)
  void draw(Gfx& g);
  void onBattleEnd(BattleResult r);  // fin d'un duel : retour au salon
  void leave();                      // ferme les connexions (retour à l'écran titre)

  // Réglages gardés dans multijoueur.txt (le mode test utilise un autre fichier)
  std::string pseudo = "Joueur", address;
  std::string settingsName = "multijoueur.txt";
  int port = net::PORT;
  bool loopback = false;  // mode test : n'accepter que cet ordinateur (pas d'alerte du pare-feu)
  void loadSettings();
  void saveSettings() const;
  static std::string dataHash();  // empreinte des données du jeu (mêmes données des deux côtés)

 private:
  friend class Game;  // le mode test pilote deux joueurs
  enum class State { Menu, Hosting, Joining, Salon, Duel };
  struct Member {
    std::string sp;
    int lvl;
  };
  Game& G;
  bool screen_ = false, host_ = false, sameLevel_ = false;
  State state_ = State::Menu;
  net::Server server_;
  std::unique_ptr<net::Conn> conn_;
  std::string peer_, status_;
  std::vector<Member> peerTeam_;
  std::vector<std::string> addresses_;
  float stateT_ = 0;

  std::vector<std::pair<std::string, int>> myView_;  // mon équipe, pour l'affichage (nom, niveau)
  void refreshTeam();
  std::string file(const std::string& name) const;
  void host();
  void join(const std::string& addr);
  void send(const Json& m);
  void onMessage(const Json& m);
  void salon(int sel = 0);
  void startDuel(int theme);
  void disconnect(const std::string& why);
  std::vector<FighterP> myTeam() const;  // équipe de la partie principale (3 premiers), ou équipe de départ
  void waitMenu(const std::string& title, const std::string& help);
};
