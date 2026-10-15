#include "i18n.hpp"

#include <regex>
#include <set>
#include <unordered_map>

#include "store.hpp"

// Motifs (data/langues/<langue>_motifs.json) : les phrases composées par le jeu (monde de l'Expédition)
// ne sont pas dans le dictionnaire. [motif, modèle] : {1} = le morceau 1 traduit, {=1} = tel quel
struct Motif {
  std::regex re;
  std::string out;
};
struct Lang {
  std::string id = "fr";
  std::unordered_map<std::string, std::string> dict;
  std::set<std::string> missing;
  std::vector<Motif> motifs;
};
// Créée au premier appel, pas au chargement du programme : T() reste sûr même appelé par un tableau global d'un
// autre fichier, rempli avant (sinon plantage au démarrage selon l'ordre des fichiers, vu sur Linux et Mac en 3.1)
static Lang& L() {
  static Lang l;
  return l;
}

void setLanguage(const std::string& id) {
  L().id = id.empty() ? "fr" : id;
  L().dict.clear();
  L().missing.clear();
  L().motifs.clear();
  if (L().id == "fr") return;
  try {
    Json j = readJson("langues/" + L().id + ".json");
    for (auto& [fr, other] : j.items())
      if (other.is_string() && !other.get<std::string>().empty()) L().dict[fr] = other.get<std::string>();
  } catch (...) {
    L().id = "fr";  // fichier absent ou abîmé : on reste en français
    return;
  }
  try {
    Json m = readJson("langues/" + L().id + "_motifs.json");
    for (auto& e : m)
      if (e.is_array() && e.size() == 2) L().motifs.push_back({std::regex(e[0].get<std::string>()), e[1].get<std::string>()});
  } catch (...) {
  }
}
const std::string& language() { return L().id; }
bool translating() { return L().id != "fr"; }

static bool byMotif(const std::string& fr, std::string& out) {
  std::smatch m;
  for (auto& mo : L().motifs) {
    if (!std::regex_match(fr, m, mo.re)) continue;
    out.clear();
    const std::string& t = mo.out;
    for (size_t i = 0; i < t.size(); i++) {
      if (t[i] == '{') {
        size_t end = t.find('}', i);
        if (end != std::string::npos) {
          std::string tag = t.substr(i + 1, end - i - 1);
          bool raw = !tag.empty() && tag[0] == '=';
          int n = std::atoi(tag.c_str() + (raw ? 1 : 0));
          if (n > 0 && n < (int)m.size()) {
            out += raw ? m[(size_t)n].str() : tr(m[(size_t)n].str());
            i = end;
            continue;
          }
        }
      }
      out += t[i];
    }
    return true;
  }
  return false;
}

const char* T(const char* fr) {
  if (L().id == "fr" || !fr || !*fr) return fr;
  auto it = L().dict.find(fr);
  if (it != L().dict.end()) return it->second.c_str();
  std::string out;
  if (byMotif(fr, out)) return (L().dict[fr] = out).c_str();  // gardée : le pointeur reste valable
  L().missing.insert(fr);
  return fr;
}
std::string tr(const std::string& fr) {
  if (L().id == "fr" || fr.empty()) return fr;
  auto it = L().dict.find(fr);
  if (it != L().dict.end()) return it->second;
  std::string out;
  if (byMotif(fr, out)) return out;
  L().missing.insert(fr);
  return fr;
}
std::vector<std::string> missingTranslations() { return {L().missing.begin(), L().missing.end()}; }
