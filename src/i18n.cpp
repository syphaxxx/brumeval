#include "i18n.hpp"

#include <regex>
#include <set>
#include <unordered_map>

#include "store.hpp"

static std::string LANG = "fr";
static std::unordered_map<std::string, std::string> DICT;
static std::set<std::string> MISSING;
// Motifs (data/langues/<langue>_motifs.json) : les phrases composées par le jeu (monde de l'Expédition)
// ne sont pas dans le dictionnaire. [motif, modèle] : {1} = le morceau 1 traduit, {=1} = tel quel
struct Motif {
  std::regex re;
  std::string out;
};
static std::vector<Motif> MOTIFS;

void setLanguage(const std::string& id) {
  LANG = id.empty() ? "fr" : id;
  DICT.clear();
  MISSING.clear();
  MOTIFS.clear();
  if (LANG == "fr") return;
  try {
    Json j = readJson("langues/" + LANG + ".json");
    for (auto& [fr, other] : j.items())
      if (other.is_string() && !other.get<std::string>().empty()) DICT[fr] = other.get<std::string>();
  } catch (...) {
    LANG = "fr";  // fichier absent ou abîmé : on reste en français
    return;
  }
  try {
    Json m = readJson("langues/" + LANG + "_motifs.json");
    for (auto& e : m)
      if (e.is_array() && e.size() == 2) MOTIFS.push_back({std::regex(e[0].get<std::string>()), e[1].get<std::string>()});
  } catch (...) {
  }
}
const std::string& language() { return LANG; }
bool translating() { return LANG != "fr"; }

static bool byMotif(const std::string& fr, std::string& out) {
  std::smatch m;
  for (auto& mo : MOTIFS) {
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
  if (LANG == "fr" || !fr || !*fr) return fr;
  auto it = DICT.find(fr);
  if (it != DICT.end()) return it->second.c_str();
  std::string out;
  if (byMotif(fr, out)) return (DICT[fr] = out).c_str();  // gardée : le pointeur reste valable
  MISSING.insert(fr);
  return fr;
}
std::string tr(const std::string& fr) {
  if (LANG == "fr" || fr.empty()) return fr;
  auto it = DICT.find(fr);
  if (it != DICT.end()) return it->second;
  std::string out;
  if (byMotif(fr, out)) return out;
  MISSING.insert(fr);
  return fr;
}
std::vector<std::string> missingTranslations() { return {MISSING.begin(), MISSING.end()}; }
