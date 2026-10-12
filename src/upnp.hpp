// Ouverture automatique du port du multijoueur sur la box (UPnP, bibliothèque miniupnpc) et code
// de partie. Quand on héberge, le jeu demande à la box de faire suivre le port vers cet ordinateur
// et lui demande son adresse Internet ; cette adresse devient un code court (« 7K2Q-XA3P ») à
// donner aux amis. Pas de serveur : si la box refuse ou n'a pas d'adresse publique, le jeu le dit
// et les solutions d'avant restent (Tailscale, adresse, port ouvert à la main).
#pragma once
#include <string>

namespace upnp {

struct Result {
  bool done = false;   // la recherche est finie (réussie ou non)
  bool ok = false;     // port ouvert et adresse publique connue
  std::string external;  // adresse Internet de la box
  std::string local;     // adresse de cet ordinateur sur le réseau de la maison
  std::string error;     // pourquoi ça n'a pas marché (en français)
};

void open(int port);  // lance la recherche dans un fil à part (ne bloque pas)
Result state();       // où on en est
void close();         // referme le port sur la box (s'il avait été ouvert)

}  // namespace upnp

namespace net {
// Code de partie : adresse IPv4 (4 octets) et un octet de contrôle, en 8 caractères de l'alphabet
// de Crockford (sans I, L, O, U, faciles à confondre), coupés en deux : « 7K2Q-XA3P »
std::string gameCode(const std::string& ipv4);
// Code (ou adresse) tapé par le joueur → adresse IPv4 ; faux si ce n'est pas un code valide
bool decodeGameCode(const std::string& text, std::string& ipv4);
bool isPublicIPv4(const std::string& ipv4);  // ni privée (10., 192.168.…), ni partagée (100.64/10)
}  // namespace net
