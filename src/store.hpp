// Fichiers de données (dossier data/) : lecture et écriture au format JSON.
// Tout le contenu du jeu (types, techniques, espèces, objets, cartes, histoire,
// règles) est rangé dans data/ et peut être modifié à la main ou par les éditeurs.
#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using Json = nlohmann::ordered_json;  // garde l'ordre des clés à l'écriture

// Dossier data/ : cherché à côté du programme, dans ses dossiers parents,
// puis dans le dossier du projet au moment de la compilation.
const std::string& dataDir();
// Lit data/<rel>. Lève une erreur lisible si le fichier manque ou est mal formé.
Json readJson(const std::string& rel);
// Écrit data/<rel> joliment (les petites listes restent sur une ligne).
void writeJson(const std::string& rel, const Json& j);
std::string prettyJson(const Json& j);
// Fichiers .json d'un sous-dossier de data/ (noms sans extension, triés)
std::vector<std::string> listJson(const std::string& relDir);

// Lecture tolérante : valeur par défaut si la clé manque
template <class T>
T jget(const Json& j, const char* key, T def) {
  auto it = j.find(key);
  return it != j.end() && !it->is_null() ? it->template get<T>() : def;
}
// Couleurs écrites "#rrggbb"
uint32_t parseColor(const Json& j);
std::string colorStr(uint32_t c);
