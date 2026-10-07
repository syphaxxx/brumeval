// Tactiques (comme les « gambits » de Final Fantasy XII) : chaque membre de
// l'équipe a une liste de règles « condition → action ». En combat, quand sa
// jauge ATB est pleine et que le mode auto est actif (touche Tab), la première
// règle possible, de haut en bas, est jouée toute seule. Si aucune ne l'est, le
// menu de commande s'ouvre comme d'habitude.
//
// Les conditions et les actions automatiques sont décrites ici ; les règles de
// départ sont dans data/regles.json (tactiques > defaut) ou dans une espèce
// (« tactiques »). Le choix de la cible et de l'action en combat est fait par
// Battle::tacticPlan (battle.cpp).
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "data.hpp"

class Game;

// Camp visé par une condition ou une action
enum class TSide { Foe, Ally, Self, Any };

struct TacticCond {
  const char* id;
  const char* label;  // « Allié : PV < {n} % » : {n} est remplacé par le seuil
  TSide side;
  int def, min, max, step;  // seuil (max = 0 : pas de seuil)
  const char* help;
  bool hasValue() const { return max > 0; }
};
const std::vector<TacticCond>& tacticConds();
const TacticCond* findTacticCond(const std::string& id);

// Actions automatiques (Tactic::Act::Auto) : « attaque », « soin »…
struct TacticAuto {
  const char* id;
  const char* name;
  TSide side;
  const char* help;
};
const std::vector<TacticAuto>& tacticAutos();
const TacticAuto* findTacticAuto(const std::string& id);

std::string tacticCondName(const Tactic& t);  // « Allié : PV < 40 % »
std::string tacticActName(const Tactic& t);   // « Meilleur soin », « Feu », « Potion »
TSide tacticActSide(const Tactic& t);
// Pourquoi la règle ne pourra pas marcher (vide si tout va bien). Avec f, vérifie
// aussi que le combattant connaît l'action.
std::string tacticProblem(const Tactic& t, const Fighter* f);
int tacticSlots(int lvl);                          // lignes utilisables à ce niveau
std::vector<Tactic> defaultTactics(const Species& s);  // celles de l'espèce, sinon des règles

Tactic tacticFromJson(const Json& o);
Json tacticToJson(const Tactic& t);
std::vector<std::string> checkTactics(const std::vector<Tactic>& list, const std::string& who);

// Éditeur des tactiques (menu de pause, Arène, Réglages). Il modifie *list et
// appelle changed() après chaque modification, closed() en quittant.
struct TacticsTarget {
  std::vector<Tactic>* list = nullptr;
  bool* on = nullptr;      // interrupteur du membre (nul : pas d'interrupteur)
  FighterP fighter;        // pour proposer ses techniques ; nul : tactiques de départ
  std::string title;
  int slots = 10;          // lignes utilisables
  std::function<void()> changed, closed;
};
void openTacticsEditor(Game& g, TacticsTarget t, int sel = 0);
