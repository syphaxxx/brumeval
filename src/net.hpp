// Réseau du multijoueur : connexions TCP entre deux ordinateurs (Windows, Mac,
// Linux). Chaque message est un objet JSON précédé de sa longueur. Rien ne
// bloque : poll() et accept() sont appelés à chaque image.
#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "store.hpp"

namespace net {

constexpr int PORT = 47474;  // port par défaut (à ouvrir sur la box si on n'utilise pas de VPN)

// Une connexion avec un autre joueur
class Conn {
 public:
  ~Conn();
  // Lance la connexion vers address:port (nom ou adresse IP) ; nullptr et err si impossible tout de suite
  static std::unique_ptr<Conn> connect(const std::string& address, int port, std::string& err);
  bool connecting() const { return state_ == Connecting; }
  bool connected() const { return state_ == Connected; }
  bool closed() const { return state_ == Closed; }
  void send(const Json& msg);
  std::vector<Json> poll();  // messages reçus depuis le dernier appel ; met à jour l'état
  void close();
  std::string error;  // pourquoi la connexion s'est arrêtée (en français)

 private:
  friend class Server;
  enum State { Connecting, Connected, Closed };
  explicit Conn(std::uintptr_t s, State st);
  void flush();
  void fail(const std::string& why);
  std::uintptr_t sock_;
  State state_;
  std::string in_, out_;
  std::chrono::steady_clock::time_point started_;
};

// Le joueur qui héberge : écoute sur un port et accepte les autres
class Server {
 public:
  ~Server();
  // loopback : seulement depuis cet ordinateur (mode test, pas d'alerte du pare-feu)
  bool listen(int port, std::string& err, bool loopback = false);
  void close();
  bool open() const;
  std::unique_ptr<Conn> accept();  // nouvelle connexion, ou nullptr

 private:
  std::uintptr_t sock_ = ~std::uintptr_t(0);
};

// Adresses IPv4 de cet ordinateur (à donner aux amis), sans 127.0.0.1
std::vector<std::string> localAddresses();

}  // namespace net
