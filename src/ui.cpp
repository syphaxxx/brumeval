#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

#include "audio.hpp"
#include "data.hpp"
#include "options.hpp"

std::string utf8Prefix(const std::string& s, int n) {
  size_t i = 0;
  int c = 0;
  while (i < s.size()) {
    if ((s[i] & 0xC0) != 0x80) {
      if (c == n) break;
      c++;
    }
    i++;
  }
  return s.substr(0, i);
}

// ---------------------------------------------------------------------------
// Script
// ---------------------------------------------------------------------------
void Script::push(Step s) {
  if (collecting_) pending_.push_back(std::move(s));
  else q_.push_back(std::move(s));
}
void Script::say(const std::string& text, float autoSec) { push({Msg, text, nullptr, autoSec}); }
void Script::call(std::function<void()> fn) { push({Call, "", std::move(fn), 0}); }
void Script::wait(float sec) { push({Wait, "", nullptr, sec}); }
void Script::runNow(const std::function<void()>& fn) {
  if (collecting_) {  // déjà dans un appel : les étapes seront insérées juste après lui
    if (fn) fn();
    return;
  }
  collecting_ = true;
  pending_.clear();
  if (fn) fn();
  collecting_ = false;
  for (auto it = pending_.rbegin(); it != pending_.rend(); ++it) q_.push_front(std::move(*it));
  pending_.clear();
}

static int countChars(const std::string& s) {
  int n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) n++;
  return n;
}
bool Script::messageDone() const { return typed_ >= countChars(text_); }

void Script::update(float dt, Input& in) {
  while (!q_.empty() && !halted_) {
    Step& s = q_.front();
    if (s.kind == Call) {
      auto fn = std::move(s.fn);
      q_.pop_front();
      collecting_ = true;
      pending_.clear();
      if (fn) fn();
      collecting_ = false;
      for (auto it = pending_.rbegin(); it != pending_.rend(); ++it) q_.push_front(std::move(*it));
      pending_.clear();
      continue;
    }
    if (s.kind == Wait) {
      t_ += dt;
      if (t_ >= s.dur) {
        t_ = 0;
        q_.pop_front();
        continue;
      }
      return;
    }
    // Message
    if (!started_) {
      started_ = true;
      text_ = s.text;
      typed_ = 0;
      t_ = 0;
    }
    int len = countChars(text_);
    typed_ = std::min<float>(len, typed_ + dt * 60);
    t_ += dt;
    bool next = false;
    if (in.confirm) {
      in.confirm = false;
      if (typed_ < len) typed_ = (float)len;
      else next = true;
    }
    if (s.dur > 0 && t_ >= s.dur + (len > 40 ? .4f : 0)) next = true;
    if (next) {
      q_.pop_front();
      started_ = false;
      t_ = 0;
      continue;
    }
    return;
  }
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------
// Saute les titres de section dans le sens d (+1 ou -1)
static void skipHeaders(Menu& m, int d) {
  int n = (int)m.items.size();
  for (int k = 0; k < n && m.items[m.sel].header; k++) m.sel = (m.sel + d + n) % n;
}

void MenuStack::push(Menu m) {
  m.sel = std::clamp(m.sel, 0, std::max(0, (int)m.items.size() - 1));
  if (!m.items.empty()) skipHeaders(m, 1);
  if (m.sel >= m.top + m.rows) m.top = m.sel - m.rows + 1;
  st_.push_back(std::move(m));
  hover();
}
void MenuStack::pop() {
  if (!st_.empty()) st_.pop_back();
  hover();
}
void MenuStack::hover() {
  if (st_.empty()) return;
  Menu& m = st_.back();
  if (m.sel < (int)m.items.size() && m.items[m.sel].hover) m.items[m.sel].hover();
}
std::string MenuStack::help() const {
  if (st_.empty()) return "";
  const Menu& m = st_.back();
  if (m.sel < (int)m.items.size()) return m.items[m.sel].help;
  return "";
}

// Élément du menu sous la souris (-1 si aucun)
// Icône d'un objet d'après son nom affiché (sac, boutiques, équipement, forge, combat) ; nullptr : pas un objet,
// ou un objet sans icône
static const SpriteRect* itemIcon(const std::string& label) {
  static std::map<std::string, std::string> names;
  static size_t count = 0;
  static std::string first;
  const auto& all = allItems();
  if (all.size() != count || (count && all[0].name != first)) {  // données rechargées (Réglages, langue)
    names.clear();
    for (auto& d : all)
      if (!d.icon.empty()) names[d.name] = d.icon;
    count = all.size();
    first = count ? all[0].name : "";
  }
  auto it = names.find(label);
  return it == names.end() ? nullptr : findSprite(it->second);
}

bool typeMark(Gfx& g, float cx, float cy, int type, uint8_t alpha) {
  if (!options().typeMarks || type < 0 || type >= (int)types().size()) return false;
  static const struct {
    const char* id;
    const char* rows[5];
  } MARKS[] = {
      {"normal", {".....", ".###.", ".#.#.", ".###.", "....."}},   // anneau
      {"feu", {"#.#.#", "#.#.#", "#####", "#####", ".###."}},      // flammes
      {"eau", {"..#..", "..#..", ".###.", "#####", ".###."}},      // goutte
      {"plante", {"#...#", "##.##", ".###.", "..#..", "..#.."}},   // pousse
      {"foudre", {"..##.", ".##..", "#####", "..##.", ".##.."}},   // éclair
      {"ombre", {".###.", "##...", "##...", "##...", ".###."}},    // lune
      {"lumiere", {"..#..", "#####", ".###.", ".#.#.", "#...#"}},  // étoile
      {"glace", {"..#..", ".#.#.", "#...#", ".#.#.", "..#.."}},    // losange (cristal)
      {"roche", {"##.##", "##.##", ".....", "##.##", "##.##"}},    // pierres
      {"vent", {"####.", ".....", ".####", ".....", "###.."}},     // rafales
      {"poison", {".###.", "#.#.#", "#####", ".###.", ".#.#."}},   // crâne
      {"metal", {"#####", "..#..", "..#..", "..#..", "#####"}},    // poutre
      {"esprit", {"#####", "....#", "###.#", "#...#", "#####"}},   // spirale
  };
  const TypeDef& td = types()[(size_t)type];
  float x0 = std::round(cx - 2.5f), y0 = std::round(cy - 2.5f);  // sur les pixels : net
  g.rect(x0, y0, 5, 5, rgb(td.color, alpha));  // fond de sa couleur : invisible sur une pastille, il porte le motif ailleurs
  Color ink = rgb(0x0a0f33, alpha);
  for (auto& mk : MARKS)
    if (td.id == mk.id) {
      for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++)
          if (mk.rows[r][c] == '#') g.rect(x0 + c, y0 + r, 1, 1, ink);
      return true;
    }
  g.text(cx, y0 - 3, utf8Prefix(td.name, 1), ink, 1, false);  // un type ajouté sans motif : son initiale
  return true;
}

// (2026-10-23) Petits dessins pour les entrées de menu qui n'ont pas de sprite (case de 11x11 en x, y)
void menuGlyph(Gfx& g, const std::string& n, float x, float y, uint8_t alpha) {
  auto rgb = [alpha](uint32_t c) { return ::rgb(c, alpha); };  // une entrée grisée : le dessin à moitié transparent
  float cx = x + 5.5f, cy = y + 5.5f;
  if (n.size() >= 7 && n[0] == '#') {  // « #rrggbb » : une pastille de cette couleur ; suivi d'un numéro (typeDot) : ce type
    uint32_t c = (uint32_t)std::stoul(n.substr(1, 6), nullptr, 16);
    auto dark = [](uint32_t v) { return ((v >> 1) & 0x7f7f7f); };
    g.ellipse(cx, cy, 4.5f, 4.5f, rgb(dark(c))), g.ellipse(cx, cy, 3.5f, 3.5f, rgb(c));
    if (!typeMark(g, cx, cy, n.size() > 7 ? std::atoi(n.c_str() + 7) : -1, alpha))
      g.ellipse(cx - 1.2f, cy - 1.4f, 1.3f, 1, rgb(0xffffff));  // reflet
  } else if (n == "vide") {  // un emplacement libre : un carré en pointillés
    for (int k = 0; k < 4; k++) {
      g.rect(x + 1.5f + k * 2.2f, y + 1.5f, 1.2f, 1, rgb(0x8a92b8)), g.rect(x + 1.5f + k * 2.2f, y + 9.5f, 1.2f, 1, rgb(0x8a92b8));
      g.rect(x + 1.5f, y + 1.5f + k * 2.2f, 1, 1.2f, rgb(0x8a92b8)), g.rect(x + 9.5f, y + 1.5f + k * 2.2f, 1, 1.2f, rgb(0x8a92b8));
    }
  } else if (n == "inconnu") {  // pas encore rencontrée : un rond vide
    g.ellipse(cx, cy, 4.5f, 4.5f, rgb(0x5a6498)), g.ellipse(cx, cy, 3.5f, 3.5f, rgb(0x1c2660));
  } else if (n == "patte") {  // les compagnons
    g.ellipse(cx, cy + 2, 3, 2.3f, rgb(0xe8c89a));
    for (int i = -1; i <= 1; i++) g.ellipse(cx + i * 3.2f, cy - 1.8f - (i == 0) * 1.2f, 1.3f, 1.4f, rgb(0xe8c89a));
  } else if (n == "damier") {  // la formation : le damier de combat
    for (int i = 0; i < 9; i++) g.rect(x + 1 + i % 3 * 3.2f, y + 1 + i / 3 * 3.2f, 2.6f, 2.6f, rgb((i % 3 + i / 3) % 2 ? 0x4a5a98 : 0xb8c8f0));
  } else if (n == "viser") {  // les tactiques
    g.ellipse(cx, cy, 5, 5, rgb(0xd84a3a)), g.ellipse(cx, cy, 3.5f, 3.5f, rgb(0xf4ecd8));
    g.ellipse(cx, cy, 2, 2, rgb(0xd84a3a)), g.rect(cx - .5f, cy - .5f, 1, 1, rgb(0xf4ecd8));
  } else if (n == "bulle") {  // les discussions
    g.ellipse(cx, cy - 1, 5, 3.6f, rgb(0xf4ecd8));
    g.tri(cx - 3, cy + 1, cx - 1, cy + 2, cx - 4, cy + 5, rgb(0xf4ecd8));
    for (int i = -1; i <= 1; i++) g.rect(cx + i * 2.4f - .5f, cy - 1.5f, 1.2f, 1.2f, rgb(0x5a5a7a));
  } else if (n == "carte") {  // la carte du monde : un parchemin plié, un chemin et une croix
    g.rect(x + 1, y + 2, 9, 7, rgb(0xe8d4a0));
    g.rect(x + 4, y + 2, 1, 7, rgb(0xc0a870)), g.rect(x + 7, y + 2, 1, 7, rgb(0xc0a870));
    g.line(x + 2, y + 7, x + 5, y + 4, 1, rgb(0x6a8a4a)), g.line(x + 5, y + 4, x + 8, y + 5, 1, rgb(0x6a8a4a));
    g.line(x + 7, y + 3, x + 9, y + 5, 1, rgb(0xc83a2a)), g.line(x + 9, y + 3, x + 7, y + 5, 1, rgb(0xc83a2a));
  } else if (n == "engrenage") {  // les options
    for (int i = 0; i < 8; i++) {
      float a = i * 3.14159f / 4;
      g.line(cx + std::cos(a) * 2, cy + std::sin(a) * 2, cx + std::cos(a) * 5, cy + std::sin(a) * 5, 2, rgb(0xa8b0c0));
    }
    g.ellipse(cx, cy, 3.6f, 3.6f, rgb(0xa8b0c0)), g.ellipse(cx, cy, 1.4f, 1.4f, rgb(0x2a2e48));
  } else if (n == "parchemin") {  // le journal des quêtes
    g.rect(x + 2, y + 1, 7, 9, rgb(0xe8d4a0));
    g.rect(x + 1, y + 1, 9, 2, rgb(0xc0a870)), g.rect(x + 1, y + 9, 9, 2, rgb(0xc0a870));
    for (int i = 0; i < 3; i++) g.rect(x + 3, y + 4 + i * 1.6f, 5 - i % 2 * 2, 1, rgb(0x6a5a3a));
  } else if (n == "aide") {
    g.ellipse(cx, cy, 5, 5, rgb(0x4a7ad8));
    g.text(cx + .5f, y + 1, "?", rgb(0xffffff), 1);
  } else if (n == "porte") {  // l'écran titre
    g.ellipse(cx, y + 4, 3.5f, 3, rgb(0x8a5a2a)), g.rect(cx - 3.5f, y + 4, 7, 6, rgb(0x8a5a2a));
    g.rect(cx - .5f, y + 2, 1, 8, rgb(0x5a3a1a)), g.rect(cx + 1.5f, y + 6, 1, 1, rgb(0xffd34d));
  } else if (n == "reprendre") {
    g.tri(x + 3, y + 1, x + 3, y + 10, x + 9, y + 5.5f, rgb(0x6ad87a));
  } else if (n == "etoile") {  // l'évolution
    std::vector<Pt> p;
    for (int i = 0; i < 10; i++) {
      float a = -1.5708f + i * 3.14159f / 5, r = i % 2 ? 2.2f : 5.2f;
      p.push_back({cx + std::cos(a) * r, cy + 0.5f + std::sin(a) * r});
    }
    g.poly(p, rgb(0xffd34d));
  } else if (n == "duo") {  // deux silhouettes côte à côte
    g.ellipse(x + 3.5f, y + 3, 1.8f, 1.8f, rgb(0xf4ecd8)), g.ellipse(x + 3.5f, y + 8, 2.8f, 2.5f, rgb(0xf4ecd8));
    g.ellipse(x + 7.5f, y + 3.5f, 1.8f, 1.8f, rgb(0xffb04a)), g.ellipse(x + 7.5f, y + 8.5f, 2.8f, 2.5f, rgb(0xffb04a));
  } else if (n == "pas") {  // se déplacer : deux empreintes
    for (int k = 0; k < 2; k++) {  // semelle, puis talon
      float fx = x + 3.5f + k * 4, fy = y + 5 - k * 2.5f;
      g.ellipse(fx, fy, 1.8f, 2.4f, rgb(0xe8c89a)), g.ellipse(fx, fy + 3.6f, 1.3f, 1.2f, rgb(0xe8c89a));
    }
  } else if (n == "retour") {  // revenir sur ses pas : une flèche vers la gauche
    g.tri(x + 1, cy, x + 5, cy - 4, x + 5, cy + 4, rgb(0x8ab8f0)), g.rect(x + 5, cy - 1.5f, 5, 3, rgb(0x8ab8f0));
  } else if (n == "sablier") {  // attendre
    g.rect(x + 2, y + 1, 7, 1, rgb(0x8a5a2a)), g.rect(x + 2, y + 9, 7, 1, rgb(0x8a5a2a));
    g.tri(x + 3, y + 2, x + 8, y + 2, cx, cy, rgb(0xd8e4f4)), g.tri(x + 3, y + 9, x + 8, y + 9, cx, cy, rgb(0xd8e4f4));
    g.tri(x + 4, y + 9, x + 7, y + 9, cx, y + 6.5f, rgb(0xffd34d));
  } else if (n == "echange") {  // changer de combattant : deux flèches
    g.rect(x + 1, y + 3, 6, 2, rgb(0x6ad87a)), g.tri(x + 7, y + 1, x + 7, y + 7, x + 10, y + 4, rgb(0x6ad87a));
    g.rect(x + 4, y + 7, 6, 2, rgb(0xf08a6a)), g.tri(x + 4, y + 5, x + 4, y + 11, x + 1, y + 8, rgb(0xf08a6a));
  } else if (n == "piece") {  // vendre : une pièce d'or
    g.ellipse(cx, cy, 4.5f, 4.5f, rgb(0xa8741a)), g.ellipse(cx, cy, 3.6f, 3.6f, rgb(0xffd34d));
    g.rect(cx - .5f, cy - 2, 1, 4, rgb(0xa8741a)), g.ellipse(cx - 1.5f, cy - 1.8f, 1, .8f, rgb(0xfff4c0));
  } else if (n == "coupe") {  // les exploits
    g.ellipse(cx, y + 3.5f, 3.6f, 3, rgb(0xffd34d)), g.rect(cx - 3.6f, y + 1, 7.2f, 2.5f, rgb(0xffd34d));
    g.rect(cx - 5, y + 2, 1.4f, 3, rgb(0xe8a83a)), g.rect(cx + 3.6f, y + 2, 1.4f, 3, rgb(0xe8a83a));  // les anses
    g.rect(cx - .7f, y + 6, 1.4f, 2.5f, rgb(0xe8a83a)), g.rect(cx - 2.5f, y + 8.5f, 5, 1.5f, rgb(0xa8741a));
  } else if (n == "coche") {  // une quête terminée
    g.line(x + 1.5f, cy, x + 4.5f, y + 9, 2, rgb(0x6ad87a)), g.line(x + 4.5f, y + 9, x + 9.5f, y + 2, 2, rgb(0x6ad87a));
  } else if (n == "drapeau") {  // abandonner l'expédition
    g.rect(x + 2, y + 1, 1, 10, rgb(0xc0c0c8)), g.tri(x + 3, y + 1, x + 3, y + 6, x + 9, y + 3.5f, rgb(0xf4ecd8));
  }
}

void setIcons(Menu& m, std::initializer_list<std::pair<std::string, const char*>> icons) {
  for (auto& it : m.items)
    for (auto& [label, ic] : icons)
      if (it.label == label) it.icon = ic;
}

size_t MenuStack::shownDepth() const {
  size_t n = 0;
  for (auto& m : st_) n += !m.hidden;
  return n;
}

static int itemAt(const Menu& m, int mx, int my) {
  if (m.hidden) return -1;
  int th = m.title.empty() ? 0 : 13;
  int shown = std::min(m.rows, (int)m.items.size());
  if (mx < m.x || mx >= m.x + m.w) return -1;
  int i = (my - (m.y + 4 + th)) / 12;
  if (my < m.y + 4 + th || i < 0 || i >= shown) return -1;
  return m.top + i;
}

void MenuStack::update(Input& in) {
  if (st_.empty()) return;
  Menu& m = st_.back();
  int n = (int)m.items.size();
  // Souris : survol, clic gauche = Entrée, clic droit = Échap, molette = défiler ou régler
  if (in.mouseOn && n > 0) {
    int at = itemAt(m, in.mx, in.my);
    if (at >= 0 && !m.items[at].header && in.moved && at != m.sel) {
      m.sel = at;
      audio::play("curseur");
      hover();
    }
    if (in.wheel && at >= 0 && m.items[m.sel].adjust && m.items[m.sel].enabled) {
      auto f = m.items[m.sel].adjust;
      int d = in.wheel > 0 ? 1 : -1;
      in.wheel = 0;
      f(d);
      return;
    }
    if (in.wheel && n > m.rows) {
      m.top = std::clamp(m.top - in.wheel, 0, n - m.rows);
      in.wheel = 0;
    }
    if (in.mclick[0] && at >= 0 && !m.items[at].header) {
      in.mclick[0] = false;
      if (at != m.sel) m.sel = at, hover();  // (Codex, 2026-10-26) un clic sans survol : l'aperçu suit la ligne cliquée
      if (m.items[at].adjust && m.items[at].enabled && !m.items[at].act) {
        auto f = m.items[at].adjust;
        f(+1);
        return;
      }
      in.confirm = true;
    } else if (in.mclick[2]) {
      in.mclick[2] = false;
      if (at >= 0 && m.items[at].adjust && m.items[at].enabled) {
        auto f = m.items[at].adjust;
        f(-1);
        return;
      }
      in.cancel = true;
    }
  }
  if (n > 0) {
    int old = m.sel;
    if (in.press[UP]) {
      m.sel = (m.sel + n - 1) % n;
      skipHeaders(m, -1);
    }
    if (in.press[DOWN]) {
      m.sel = (m.sel + 1) % n;
      skipHeaders(m, 1);
    }
    if ((in.press[LEFT] || in.press[RIGHT]) && m.items[m.sel].adjust && m.items[m.sel].enabled) {
      auto f = m.items[m.sel].adjust;  // copie : le menu peut être reconstruit pendant le réglage
      int d = in.press[RIGHT] ? 1 : -1;
      in.press[LEFT] = in.press[RIGHT] = false;
      audio::play("curseur");
      f(d);
      return;
    }
    if (in.press[LEFT] && n > m.rows) m.sel = std::max(0, m.sel - m.rows);
    if (in.press[RIGHT] && n > m.rows) m.sel = std::min(n - 1, m.sel + m.rows);
    if (m.sel < m.top) m.top = m.sel;
    if (m.sel > 0 && m.items[m.sel - 1].header && m.sel - 1 < m.top) m.top = m.sel - 1;  // garde le titre visible
    if (m.sel >= m.top + m.rows) m.top = m.sel - m.rows + 1;
    if (old != m.sel) {
      audio::play("curseur");
      hover();
    }
  }
  if (in.confirm) {
    in.confirm = false;
    m.sel = std::clamp(m.sel, 0, std::max(0, n - 1));  // un curseur placé hors de la liste (par le code) ne fait rien de grave
    if (n > 0 && m.items[m.sel].enabled && m.items[m.sel].act) {
      auto act = m.items[m.sel].act;  // copie : le menu peut être détruit pendant l'action
      audio::play("valider");
      act();
    } else if (n > 0 && !m.items[m.sel].header)
      audio::play("refus");
    return;
  }
  if (in.cancel) {
    in.cancel = false;
    if (!m.cancelable) return;
    audio::play("retour");
    if (m.onCancel) {
      auto f = m.onCancel;
      f();
    } else pop();
  }
}

// Cadre d'un menu à l'écran
static void menuBox(const Menu& m, int& h, int& th) {
  th = m.title.empty() ? 0 : 13;
  h = std::min(m.rows, (int)m.items.size()) * 12 + 8 + th;
}
// Texte coupé avec « … » s'il est plus large que w pixels (le mode test le signale, sauf si c'est prévu)
static std::string fit(Gfx& g, const std::string& s, int w, bool planned = false) {
  if (Gfx::textW(s) <= w) return s;
  if (g.checkLayout && !planned) g.layoutIssue("texte coupé dans un menu : « " + s + " »");
  return utf8Prefix(s, std::max(0, w / 6 - 1)) + "…";
}

int MenuStack::maxRight() const {
  int r = 0;
  for (auto& m : st_) r = std::max(r, m.x + m.w);
  return r;
}

void MenuStack::draw(Gfx& g, float t) const {
  for (size_t k = 0; k < st_.size(); k++) {
    const Menu& m = st_[k];
    if (m.hidden) continue;
    bool top = k + 1 == st_.size();
    int h, th;
    menuBox(m, h, th);
    // Un menu ouvert par-dessus qui n'est pas bien à l'intérieur de celui-ci le cache entièrement,
    // plutôt que d'en laisser dépasser des morceaux
    bool hidden = false;
    for (size_t j = k + 1; j < st_.size() && !hidden; j++) {
      const Menu& o = st_[j];
      if (o.hidden) continue;
      int oh, oth;
      menuBox(o, oh, oth);
      bool meet = o.x < m.x + m.w && m.x < o.x + o.w && o.y < m.y + h && m.y < o.y + oh;
      bool inside = o.x >= m.x + 4 && o.y >= m.y + 4 && o.x + o.w <= m.x + m.w - 4 && o.y + oh <= m.y + h - 4;
      hidden = meet && !inside;
    }
    if (hidden) continue;
    int shown = std::min(m.rows, (int)m.items.size());
    g.window(m.x, m.y, m.w, h);
    if (!m.title.empty()) {
      g.text(m.x + 6, m.y + 3, fit(g, m.title, m.w - 12), rgb(0xffd34d));
      g.rect(m.x + 4, m.y + 14, m.w - 8, 1, rgb(0x8090c8, 140));
    }
    for (int i = 0; i < shown; i++) {
      int idx = m.top + i;
      const MenuItem& it = m.items[idx];
      float y = m.y + 4 + th + i * 12;
      if (it.header) {
        std::string label = fit(g, it.label, m.w - 12);
        g.text(m.x + 6, y, label, rgb(0xffd34d));
        g.rect(m.x + 8 + Gfx::textW(label), y + 5, m.w - 16 - Gfx::textW(label), 1, rgb(0x8090c8, 140));
        continue;
      }
      if (it.sep && i > 0) g.rect(m.x + 12, y - 1, m.w - 24, 1, rgb(0x8090c8, 90));  // début d'un groupe
      Color c = !it.enabled ? rgb(0x8a92b8) : it.color ? rgb(it.color) : rgb(0xffffff);
      std::string right = it.notches ? std::string() : it.rightFn ? it.rightFn() : it.right;
      int rw = it.notches ? 58 : right.empty() ? 0 : Gfx::textW(right) + 6;
      int ix = 0;  // un objet du jeu (ou une icône donnée) : son icône avant le nom
      if (const SpriteRect* ic = it.icon.empty() ? itemIcon(it.label) : findSprite(it.icon)) {
        float k = it.icon.empty() ? 1 : std::min(1.f, 11.f / std::max(ic->w, ic->h));  // une icône donnée tient dans sa case
        g.sprite(*ic, m.x + 12 + (11 - ic->w * k) / 2, y + 1 + (11 - ic->h * k) / 2, false, k, it.enabled ? rgb(0xffffff) : rgb(0x8a8a9a));
        ix = 11;
      } else if (!it.icon.empty()) {
        menuGlyph(g, it.icon, m.x + 12, y + 1, it.enabled ? 255 : 90);
        ix = 11;
      }
      g.text(m.x + 13 + ix, y, fit(g, it.label, m.w - 19 - rw - ix, it.shrink), c);
      if (!right.empty()) g.text(m.x + m.w - 6, y, right, it.enabled ? (it.adjust ? rgb(0xffe066) : rgb(0xd8def2)) : rgb(0x8a92b8), 2);
      if (it.bar >= 0) {  // (2026-10-23) l'avancement, sous la ligne
        float bx = m.x + 13 + ix, bw = m.x + m.w - 6 - bx;
        g.rect(bx, y + 10.5f, bw, 1, rgb(0x03051a, 170));
        g.rect(bx, y + 10.5f, bw * std::clamp(it.bar, 0.f, 1.f), 1, it.bar >= 1 ? rgb(0xffe066) : rgb(0x6ad87a));
      }
      if (it.notches) {  // (2026-10-23) dix crans, pleins jusqu'à la valeur ; flèches gauche/droite sur la ligne choisie
        int v = std::clamp(it.notches(), 0, 10);
        float bx = m.x + m.w - 56;
        for (int k = 0; k < 10; k++) {
          float hgt = 3 + k * .5f;  // de plus en plus hauts, comme un volume
          g.rect(bx + k * 5, y + 9 - hgt, 4, hgt, k < v ? rgb(0xffe066) : rgb(0x03051a, 160));
        }
        if (idx == m.sel && top) {
          g.tri(bx - 3, y + 2.5f, bx - 3, y + 7.5f, bx - 6, y + 5, rgb(0xffd34d));
          g.tri(bx + 50, y + 2.5f, bx + 50, y + 7.5f, bx + 53, y + 5, rgb(0xffd34d));
        }
      }
      if (idx == m.sel) {
        if (top) g.cursor(m.x + 4 + (int(t * 4) % 2), y + 1);
        else g.rect(m.x + 4, y + 3, 4, 4, rgb(0xffd34d, 150));
      }
    }
    if (m.top > 0) g.tri(m.x + m.w - 7, m.y + th + 6, m.x + m.w - 2, m.y + th + 6, m.x + m.w - 4.5f, m.y + th + 3, rgb(0xffd34d));  // sur le bord : jamais sur un libellé
    if (m.top + shown < (int)m.items.size())
      g.tri(m.x + m.w - 7, m.y + h - 6, m.x + m.w - 2, m.y + h - 6, m.x + m.w - 4.5f, m.y + h - 3, rgb(0xffd34d));
    if (!top) {
      Gfx& gg = g;
      gg.rect(m.x, m.y, m.w, h, rgb(0x000010, 70));
    }
  }
}
