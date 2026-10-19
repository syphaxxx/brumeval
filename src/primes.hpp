// Primes (demandé le 2026-10-19 : « au moins 20 heures de jeu ») : monstres recherchés de data/primes.json.
//
// Un tableau d'annonces (tuile G) ouvre Game::bountyMenu. Une prime acceptée pose le drapeau « prime:<id> » : son
// monstre apparaît sur sa carte (un boss de carte ajouté par addBountySpots, visible sous condition). Vaincu, il pose
// « prime:<id>:vaincue » ; la prime se touche ensuite au tableau (« prime:<id>:fin ») : or, objets, et les
// récompenses du rang de chasseur (« rangs » : nombre de primes touchées). Le combat est un événement généré
// « prime:<id> » (findEvent le trouve comme les autres ; il n'est jamais écrit dans evenements.json).
#pragma once
#include <string>
#include <vector>

#include "store.hpp"

struct MapDef;

void loadBounties();                                   // lit data/primes.json (appelé par loadMaps)
const Json& bounties();                                // liste des primes
const Json& bountyRanks();                             // rangs : {"primes": n, "titre", "objets"}
const Json* findBounty(const std::string& id);
const Json* bountyEvent(const std::string& eventId);  // l'événement « prime:<id> », sinon nullptr
std::string bountyFlag(const std::string& id, int stage);  // 0 : acceptée, 1 : vaincue, 2 : touchée
void addBountySpots(MapDef& m);  // le monstre des primes de cette carte (appelé par mapFromJson)
std::vector<std::string> checkBounties();              // vérifications (mode test)
