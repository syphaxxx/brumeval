// Son : musiques et bruitages fabriqués par le programme (synthèse, comme les
// vieilles consoles), sans aucun fichier audio. Tout est décrit dans
// data/sons.json : chaque musique est une partition (une ligne de notes par voix),
// chaque effet une suite de sons simples (onde, fréquence de départ et d'arrivée,
// durée). Voir data/LISEZMOI.md.
//
// La sortie son tourne dans un fil à part (rappel de SDL) : les fonctions ci-dessous
// peuvent être appelées n'importe quand. Sans carte son (mode test), tout marche
// sans rien jouer.
#pragma once
#include <string>
#include <vector>

namespace audio {

void loadSounds();                      // lit data/sons.json (erreur lisible s'il est mal formé)
std::vector<std::string> checkSounds(); // morceaux et effets inconnus ou mal écrits
void init();                            // ouvre la sortie son (sans effet si impossible)
void shutdown();

void play(const std::string& effect);  // bruitage (ignoré s'il n'existe pas)
bool has(const std::string& effect);   // ce bruitage existe dans data/sons.json
// Musique jouée en boucle ("" : silence). Le même morceau continue sans repartir du
// début ; un autre remplace l'ancien après un court fondu.
void music(const std::string& track);
std::string currentMusic();
std::string musicForPlace(const std::string& theme);  // morceau d'un thème de carte (« vallee »…)
void setVolumes(int music, int effects);  // 0 à 10

// Mode test : fabrique les échantillons d'un effet ou d'un morceau sans les jouer
std::vector<float> render(const std::string& name, bool isMusic, float seconds);
std::vector<std::string> effectNames();
std::vector<std::string> trackNames();
// Mode test (pas de sortie son) : fait tourner le vrai mélangeur pendant « seconds », comme le
// ferait le fil audio, et renvoie le plus fort échantillon de musique produit
float runMixer(float seconds);
extern int playedCount;  // effets demandés depuis le début (mode test)
extern std::string lastEffect;

}  // namespace audio
