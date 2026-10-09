// Événements de l'histoire (data/evenements.json).
//
// Un événement est une suite d'actions, éventuellement découpée en « pages » :
// la première page dont la condition « si » est vraie est jouée. Exemple :
//   "pecheur": {"pages": [
//     {"si": {"sans_drapeau": "pecheur"}, "actions": [
//        {"action": "drapeau", "nom": "pecheur"},
//        {"action": "dire", "texte": "Pêcheur : Tenez, des plumes !"},
//        {"action": "donner", "objet": "plume", "quantite": 2}]},
//     {"actions": [{"action": "dire", "texte": "Pêcheur : Le lac est calme."}]}]}
//
// Actions : dire, donner, retirer, or, drapeau, recruter, combat, question, si,
// soigner, boutique, reveil, teleporter, evenement, attendre, fin, quete.
// Conditions (toutes doivent être vraies) : drapeau, sans_drapeau, objet,
// sans_objet, or_min, membre.
// Le détail de chaque action est dans events.cpp (Game::execAction).
#pragma once
#include <string>
#include <vector>

#include "store.hpp"

void loadEvents();
Json& events();  // objet { identifiant : événement }
const Json* findEvent(const std::string& id);
void saveEvents();
// Vérifie toutes les actions et que les événements cités par les cartes existent.
std::vector<std::string> checkEvents();

// Quêtes annexes (data/quetes.json, lu avec les événements). Une quête commence avec
// l'action {"action": "quete", "id": …} et se termine avec "fin": true ; ses étapes
// (texte, condition « si ») s'affichent dans le journal (menu de pause > Journal).
const Json& quests();
const Json* findQuest(const std::string& id);
std::string questFlag(const std::string& id, bool done = false);  // « quete:<id> » ou « quete:<id>:fin »
