// Génération procédurale du mode Expédition.
//
// À partir d'une graine (un nombre), crée tout le contenu d'une partie :
// techniques de chaque type, créatures, héros, dresseurs, apparences, puis les
// régions une par une (carte, village, dresseurs, coffres, boss) avec leurs
// événements. Tout est produit au format des fichiers de data/ : le moteur du jeu
// le charge ensuite comme les vraies données (dataDoc + rebuildData, maps(),
// events()). Les types et les objets restent ceux de data/.
//
// La même graine redonne exactement le même monde (hasard maison, identique sur
// tous les ordinateurs) : une sauvegarde ne garde que la graine et la région.
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "store.hpp"

namespace procgen {

// Hasard reproductible (xoshiro256**)
class Rng {
 public:
  explicit Rng(uint64_t seed);
  uint64_t next();
  int range(int lo, int hi);  // entier entre lo et hi inclus
  float real();               // entre 0 et 1
  bool chance(float p) { return real() < p; }
  template <class T>
  const T& pick(const std::vector<T>& v) {
    return v[range(0, (int)v.size() - 1)];
  }

 private:
  uint64_t s_[4];
};

// Graine écrite par le joueur : un nombre tel quel, sinon l'empreinte du texte
uint64_t seedFromText(const std::string& s);
// Graine au hasard (horloge)
uint64_t randomSeed();

// Techniques d'un type, de la plus faible à la plus forte
struct Pool {
  std::string p1, p2, p3, p4;  // physiques : rapide, normale, forte, lourde
  std::string area;            // physique sur tous les ennemis
  std::string m1, m3;          // magiques gratuites : faible, forte
  std::string s1, s2, s3, s4;  // sorts (PM) : faible, zone, fort, grande zone
};
// Techniques de soutien (soins, bonus, états)
struct Support {
  std::string heal, healWater, healAll, healAllPlant, revive, cure, guardAll, warCry, focus, shell, haste, sleep, hypno, toxin, stun,
      curse, breaker;
};

// Contenu d'une expédition, au format de data/
struct Content {
  Json moves = Json::array();    // techniques.json
  Json species = Json::array();  // especes.json
  Json looks = Json::array();    // apparences.json (ajoutées à celles du jeu)
  std::vector<std::string> heroes;    // héros proposés au départ (6 ; le Camp en montre 3 à 6)
  std::vector<std::string> starters;  // créatures proposées au départ (5)
  std::map<std::string, Pool> pools;  // techniques par type
  Support sup;
  std::set<std::string> names;  // noms déjà pris (techniques, espèces, personnes, lieux)
  int regions = 0;              // régions déjà générées (leurs espèces sont dans species)
};

// Une région : sa carte et ses événements
struct Region {
  int index = 1;  // 1, 2, 3…
  std::string name, village, bossName, nextName;
  Json map;                       // format data/cartes
  Json events = Json::object();   // { identifiant : événement }
  int level = 4;                  // niveau des créatures sauvages
  int startX = 0, startY = 0;     // arrivée du joueur
  int exitX = 0, exitY = 0;       // sortie vers la région suivante (derrière le boss)
  std::string bossFlag;           // posé quand le boss est vaincu
  std::vector<std::string> wild;  // créatures sauvages
};

int regionLevel(int k);  // niveau des créatures sauvages de la région k
std::string regionName(uint64_t seed, int k);
Content generateBase(uint64_t seed);
// Région k : ajoute ses créatures, dresseurs et apparences à c. Les régions
// précédentes doivent déjà avoir été générées (c.regions == k - 1).
Region generateRegion(uint64_t seed, int k, Content& c);

}  // namespace procgen
