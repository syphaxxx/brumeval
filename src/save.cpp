// Sauvegarde de la partie : fichier texte, une information par ligne (voir docs/GUIDE.md).
// Méthodes de Game (game.hpp) et readMemberLine, partagé avec le multijoueur.
#include <algorithm>
#include <fstream>
#include <sstream>

#include "i18n.hpp"
#include "expedition.hpp"
#include "game.hpp"
#include "tactics.hpp"

// ---------------------------------------------------------------------------
// Sauvegarde (fichier texte dans le dossier de l'utilisateur)
// ---------------------------------------------------------------------------
std::string Game::savePath() const { return userFile(inExpedition() ? expedition_->saveFile() : saveName_); }
bool Game::saveExists() const {
  std::ifstream f(savePath());
  return f.good();
}
bool Game::saveGame() {
  std::ostringstream f;
  f << "BRUMEVAL 1\n";
  if (inExpedition()) expedition_->writeSave(f);  // en premier : le chargement génère d'abord le monde
  f << "carte " << M().id << ' ' << px << ' ' << py << ' ' << dir << '\n';
  f << "reveil " << maps()[respawnMap].id << ' ' << respawnX << ' ' << respawnY << '\n';
  f << "or " << gold << '\n';
  for (auto& fl : flags) f << "drapeau " << fl << '\n';
  for (auto& [id, n] : items)
    if (n > 0) f << "objet " << id << ' ' << n << '\n';
  f << "auto " << tacticsAuto << '\n';
  noteBestiary();
  for (auto& sp : seen)
    if (hasSpecies(sp)) f << "vu " << sp << '\n';
  for (auto& sp : caught)
    if (hasSpecies(sp)) f << "pris " << sp << '\n';
  for (auto& t : tipsSeen) f << "astuce " << t << '\n';  // astuces déjà montrées (2.8)
  if (!inExpedition())
    for (auto& v : visited) f << "lieu " << v << '\n';  // lieux visités (voyage rapide)
  for (auto& w : weak) {  // « espèce:numéro du type » : écrit avec l'identifiant du type (stable si on en ajoute)
    auto c = w.find(':');
    int t = std::atoi(w.c_str() + c + 1);
    if (hasSpecies(w.substr(0, c)) && t >= 0 && t < (int)types().size()) f << "faible " << w.substr(0, c) << ' ' << types()[t].id << '\n';
  }
  static const char KIND[] = {'a', 't', 'o'};  // action automatique, technique, objet
  auto member = [&](const FighterP& m) {
    f << "membre " << m->sp << ' ' << m->lvl << ' ' << m->xp << ' ' << m->hp << ' ' << m->mp << ' ' << m->lim << '\n';
    if (m->gear != decltype(m->gear){}) {
      f << "equipement";
      for (auto& g : m->gear) f << ' ' << (g.empty() ? "-" : g);
      f << '\n';
    }
    for (int s = 0; s < N_GEAR; s++)  // gemmes serties (2.7)
      if (!m->gems[(size_t)s].empty()) {
        f << "gemmes " << s;
        for (auto& g : m->gems[(size_t)s]) f << ' ' << g;
        f << '\n';
      }
    if (m->cell >= 0) f << "case4 " << m->cell << '\n';  // formation sur le damier 4x4
    if (m->evolveLater) f << "evolution_plus_tard\n";       // évolution refusée (Échap > Évolution)
    if (m->chroma) f << "chromatique\n";                    // couleurs rares (2026-10-20)
    auto c = std::find(team.begin(), team.end(), m->companion.lock());
    if (c != team.end() && hasSpecies(m->sp) && m->S().human) f << "compagnon " << (c - team.begin()) << '\n';
    f << "tactiques " << m->tacticsOn << '\n';
    for (auto& t : m->tactics) f << "tactique " << t.on << ' ' << t.cond << ' ' << t.value << ' ' << KIND[(int)t.kind] << ' ' << t.act << '\n';
  };
  for (auto& m : team) member(m);
  if (!reserve.empty()) {  // la réserve après l'équipe : ses lignes « membre » suivent la ligne « reserve »
    f << "reserve\n";
    for (auto& m : reserve) member(m);
  }
  return writeUserFile(inExpedition() ? expedition_->saveFile() : saveName_, f.str());
}
// Lignes d'un membre de l'équipe : « membre » (espèce, niveau, expérience, PV, PM, Limite), puis
// « equipement », « tactiques » et ses lignes « tactique ». Une espèce inconnue ajoute nullptr :
// les lignes qui la suivent sont ignorées (à retirer ensuite).
bool readMemberLine(const std::string& k, std::istream& s, std::vector<FighterP>& team, std::vector<std::pair<size_t, int>>* links) {
  if (k == "membre") {
    std::string sp;
    int lvl = 1, xp = 0, hp = 1, mp = 0;
    float lim = 0;
    s >> sp >> lvl >> xp >> hp >> mp >> lim;
    if (!hasSpecies(sp)) {
      team.push_back(nullptr);
      return true;
    }
    auto m = makeFighter(sp, lvl);
    m->xp = xp, m->hp = hp, m->mp = mp, m->lim = lim;  // à borner une fois l'équipement lu
    team.push_back(m);
    return true;
  }
  if (k != "equipement" && k != "gemmes" && k != "tactiques" && k != "tactique" && k != "compagnon" && k != "case" && k != "case4" &&
      k != "evolution_plus_tard" && k != "chromatique")
    return false;
  if (team.empty() || !team.back()) return true;
  Fighter& m = *team.back();
  if (k == "evolution_plus_tard") {
    m.evolveLater = true;
    return true;
  }
  if (k == "chromatique") {
    m.chroma = true;
    return true;
  }
  if (k == "case4" || k == "case") {  // formation : sa case sur le damier (« case » : ancien damier 3x3, avant la 2.0)
    int c = -1;
    if (k == "case" && s >> c && c >= 0 && c < 9) {
      static const int COL[3] = {0, 1, BACK_COL};  // l'arrière reste l'arrière
      m.cell = cellAt(c / 3, COL[c % 3]);
    } else if (k == "case4" && s >> c && c >= 0 && c < N_CELLS)
      m.cell = c;
    return true;
  }
  if (k == "compagnon") {  // rang de sa créature compagnon dans l'équipe (relié une fois tout lu)
    int i = -1;
    if (s >> i && links) links->push_back({team.size() - 1, i});
    return true;
  }
  if (k == "gemmes") {
    int slot = -1;
    std::string id;
    if (s >> slot && slot >= 0 && slot < N_GEAR)
      while (s >> id)
        if (hasItem(id) && item(id).gem) m.gems[(size_t)slot].push_back(id);
    trimGems(m);
    m.recalc();
    return true;
  }
  if (k == "equipement") {
    for (auto& g : m.gear) {
      std::string id;
      s >> id;
      g = id != "-" && hasItem(id) && canEquip(m, item(id)) ? id : "";
    }
    m.recalc();
  } else if (k == "tactiques") {  // les anciennes sauvegardes gardent les tactiques de départ
    s >> m.tacticsOn;
    m.tactics.clear();
  } else {
    Tactic t;
    char kind = 'a';
    s >> t.on >> t.cond >> t.value >> kind >> t.act;
    t.kind = kind == 't' ? Tactic::Act::Move : kind == 'o' ? Tactic::Act::Item : Tactic::Act::Auto;
    if (s && findTacticCond(t.cond)) m.tactics.push_back(t);
  }
  return true;
}

bool Game::loadGame() {
  std::ifstream f(savePath());
  std::string head;
  if (!f || !std::getline(f, head) || head.rfind("BRUMEVAL", 0) != 0) return false;
  team.clear();
  reserve.clear();
  seen.clear(), caught.clear(), weak.clear(), tipsSeen.clear();
  visited.clear();
  bool inReserve = false;                      // après la ligne « reserve »
  std::vector<std::pair<size_t, int>> links;  // lignes « compagnon » : reliées à la fin
  items.clear();
  flags.clear();
  // Anciennes sauvegardes : cartes désignées par leur numéro
  static const char* OLD_MAPS[] = {"vallee", "cendrelune", "grotte"};
  auto mapOf = [&](const std::string& v) {
    int m = mapIndex(v);
    if (m < 0 && v.size() == 1 && v[0] >= '0' && v[0] <= '2') m = mapIndex(OLD_MAPS[v[0] - '0']);
    return std::max(0, m);
  };
  std::vector<std::string> oldChests;
  std::string line;
  mapId = 0;
  walkMap_ = -1;  // les habitants repartent de leur place
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string k, v;
    s >> k;
    if (k == "expedition" || k == "graine") {
      std::string rest;
      std::getline(s, rest);
      if (!expedition_ || !expedition_->readSave(k, rest)) return false;
    } else if (k == "carte") {
      s >> v >> px >> py >> dir;
      mapId = mapOf(v);
    } else if (k == "reveil") {
      s >> v >> respawnX >> respawnY;
      respawnMap = mapOf(v);
    } else if (k == "or") s >> gold;
    else if (k == "drapeau") {
      s >> v;
      // Ancien format des coffres : coffre:<numéro de carte>:<numéro du coffre>
      if (v.rfind("coffre:", 0) == 0 && std::count(v.begin(), v.end(), ':') == 2) oldChests.push_back(v);
      else flags.insert(v);
    } else if (k == "objet") {
      std::string id;
      int n;
      s >> id >> n;
      items[id] = n;
    } else if (k == "auto") s >> tacticsAuto;
    else if (k == "vu" || k == "pris") {
      s >> v;
      if (hasSpecies(v)) (k == "vu" ? seen : caught).insert(v);
    } else if (k == "astuce") {
      s >> v;
      tipsSeen.insert(v);
    } else if (k == "lieu") {
      s >> v;
      if (mapIndex(v) >= 0) visited.insert(v);
    } else if (k == "faible") {
      std::string t;
      s >> v >> t;
      for (int i = 0; i < (int)types().size(); i++)
        if (types()[i].id == t && hasSpecies(v)) weak.insert(v + ":" + std::to_string(i));
    } else if (k == "reserve") inReserve = true;
    else if (inReserve) readMemberLine(k, s, reserve, nullptr);
    else readMemberLine(k, s, team, &links);
  }
  reserve.erase(std::remove(reserve.begin(), reserve.end(), nullptr), reserve.end());
  for (auto& m : reserve) m->hp = std::min(m->hp, m->mhp), m->mp = std::min(m->mp, m->mmp);
  noteBestiary();  // anciennes sauvegardes : l'équipe compte déjà
  for (auto& [h, c] : links)  // compagnons : avant de retirer les espèces disparues (les rangs changeraient)
    if (c >= 0 && c < (int)team.size() && team[h] && team[(size_t)c] && !team[(size_t)c]->S().human) team[h]->companion = team[(size_t)c];
  team.erase(std::remove(team.begin(), team.end(), nullptr), team.end());  // espèces qui n'existent plus
  for (auto& v : oldChests) {
    int m = mapOf(v.substr(7, v.find(':', 7) - 7));
    int i = std::atoi(v.substr(v.rfind(':') + 1).c_str());
    if (i >= 0 && i < (int)maps()[m].chests.size()) {
      auto& c = maps()[m].chests[i];
      flags.insert("coffre:" + maps()[m].id + ":" + std::to_string(c.x) + ":" + std::to_string(c.y));
    }
  }
  moving = false;
  steps = 0;
  sc.clear();
  for (auto& m : team) m->hp = std::min(m->hp, m->mhp), m->mp = std::min(m->mp, m->mmp);
  lastNewTalks_ = newDiscussions();  // pas d'annonce pour les discussions déjà disponibles
  applyNewGamePlus();  // Nouvelle partie + : ennemis plus forts selon les drapeaux « ngp<k> »
  if (!inExpedition() && mapId >= 0 && mapId < (int)maps().size()) {  // le lieu où l'on est (une ancienne sauvegarde n'a pas de « lieu »)
    if (M().worldX >= 0) visited.insert(M().id);
    for (auto& w : M().warps)  // dans un intérieur : son village
      if (int t = mapIndex(w.map); M().theme == Theme::Interieur && t >= 0 && maps()[t].worldX >= 0) visited.insert(maps()[t].id);
  }
  return !team.empty();
}
