#include "store.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

static bool hasData(const fs::path& p) { return fs::exists(p / "regles.json"); }

const std::string& dataDir() {
  static const std::string dir = [] {
    std::vector<fs::path> starts;
    if (char* b = SDL_GetBasePath()) {
      starts.push_back(fs::u8path(b));
      SDL_free(b);
    }
    std::error_code ec;
    starts.push_back(fs::current_path(ec));
    for (fs::path s : starts)
      for (int up = 0; up < 5 && !s.empty(); up++, s = s.parent_path())
        if (hasData(s / "data")) return (s / "data").u8string();
#ifdef BRUMEVAL_DATA_DIR
    if (hasData(fs::u8path(BRUMEVAL_DATA_DIR))) return std::string(BRUMEVAL_DATA_DIR);
#endif
    return std::string("data");
  }();
  return dir;
}

static fs::path full(const std::string& rel) { return fs::u8path(dataDir()) / fs::u8path(rel); }

Json readJson(const std::string& rel) {
  std::ifstream f(full(rel), std::ios::binary);
  if (!f) throw std::runtime_error("Fichier introuvable : data/" + rel);
  std::stringstream ss;
  ss << f.rdbuf();
  try {
    return Json::parse(ss.str());
  } catch (const Json::parse_error& e) {
    // Retrouve la ligne de l'erreur pour un message compréhensible
    std::string s = ss.str();
    size_t pos = std::min<size_t>(e.byte, s.size());
    int line = 1 + (int)std::count(s.begin(), s.begin() + pos, '\n');
    throw std::runtime_error("Erreur dans data/" + rel + ", ligne " + std::to_string(line) + " : le fichier JSON est mal formé.");
  }
}

// --- Écriture lisible -------------------------------------------------------
static std::string scalar(const Json& j) {
  if (j.is_number_float()) {
    double v = j.get<double>();
    if (std::fabs(v - std::round(v)) < 1e-9 && std::fabs(v) < 1e9) return std::to_string((long long)std::llround(v)) + ".0";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4f", v);  // 4 décimales suffisent, évite 0.10999999
    std::string s = buf;
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s += '0';
    return s;
  }
  return j.dump(-1, ' ', false, Json::error_handler_t::replace);
}
static std::string inlineJson(const Json& j) {
  if (j.is_object()) {
    if (j.empty()) return "{}";
    std::string s = "{";
    bool first = true;
    for (auto it = j.begin(); it != j.end(); ++it) {
      if (!first) s += ", ";
      first = false;
      s += Json(it.key()).dump(-1, ' ', false, Json::error_handler_t::replace) + ": " + inlineJson(it.value());
    }
    return s + "}";
  }
  if (j.is_array()) {
    if (j.empty()) return "[]";
    std::string s = "[";
    for (size_t i = 0; i < j.size(); i++) s += (i ? ", " : "") + inlineJson(j[i]);
    return s + "]";
  }
  return scalar(j);
}
static void pretty(std::string& out, const Json& j, size_t ind, size_t used) {
  std::string flat = inlineJson(j);
  if (!j.is_structured() || j.empty() || (ind > 0 && used + flat.size() <= 110)) {
    out += flat;
    return;
  }
  bool obj = j.is_object();
  out += obj ? "{\n" : "[\n";
  size_t i = 0;
  for (auto it = j.begin(); it != j.end(); ++it, ++i) {
    out.append(ind + 2, ' ');
    size_t u = ind + 2;
    if (obj) {
      std::string k = Json(it.key()).dump(-1, ' ', false, Json::error_handler_t::replace) + ": ";
      out += k;
      u += k.size();
    }
    pretty(out, *it, ind + 2, u);
    if (i + 1 < j.size()) out += ",";
    out += "\n";
  }
  out.append(ind, ' ');
  out += obj ? "}" : "]";
}
std::string prettyJson(const Json& j) {
  std::string s;
  pretty(s, j, 0, 0);
  return s + "\n";
}

void writeJson(const std::string& rel, const Json& j) {
  fs::path p = full(rel);
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  // Écrit dans un fichier temporaire puis remplace : jamais de fichier à moitié écrit
  fs::path tmp = p;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) throw std::runtime_error("Impossible d'écrire data/" + rel);
    f << prettyJson(j);
  }
  fs::rename(tmp, p, ec);
  if (ec) {
    fs::remove(p, ec);
    fs::rename(tmp, p, ec);
    if (ec) throw std::runtime_error("Impossible de remplacer data/" + rel);
  }
}

std::vector<std::string> listJson(const std::string& relDir) {
  std::vector<std::string> v;
  std::error_code ec;
  for (auto& e : fs::directory_iterator(full(relDir), ec))
    if (e.is_regular_file() && e.path().extension() == ".json") v.push_back(e.path().stem().u8string());
  std::sort(v.begin(), v.end());
  return v;
}

uint32_t parseColor(const Json& j) {
  if (j.is_number_integer()) return j.get<uint32_t>();
  std::string s = j.get<std::string>();
  if (!s.empty() && s[0] == '#') s = s.substr(1);
  return (uint32_t)std::stoul(s, nullptr, 16);
}
std::string colorStr(uint32_t c) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "#%06x", c & 0xffffff);
  return buf;
}
