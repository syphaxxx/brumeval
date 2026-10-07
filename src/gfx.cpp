#include "gfx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

Color rgb(uint32_t h, uint8_t a) { return Color{uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h), a}; }
Color shade(Color c, float k) {
  auto f = [&](uint8_t v) {
    float r = k >= 0 ? v * (1 - k) : v + (255 - v) * -k;
    return uint8_t(std::clamp(r, 0.f, 255.f));
  };
  return Color{f(c.r), f(c.g), f(c.b), c.a};
}
Color mix(Color a, Color b, float t) {
  auto m = [&](uint8_t x, uint8_t y) { return uint8_t(x + (y - x) * t); };
  return Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

void Gfx::set(Color c) {
  SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, uint8_t(c.a * std::clamp(alpha, 0.f, 1.f)));
}
void Gfx::span(int y, int x0, int x1) {
  if (x1 < x0 || y < 0 || y >= SCREEN_H) return;
  SDL_Rect rc{x0, y, x1 - x0 + 1, 1};
  SDL_RenderFillRect(r_, &rc);
}
void Gfx::clear(Color c) {
  SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, 255);
  SDL_RenderClear(r_);
  newLayer();
}

// ---------------------------------------------------------------------------
// Vérification de la mise en page (mode test)
// ---------------------------------------------------------------------------
void Gfx::newLayer() {
  wins_.clear();
  texts_.clear();
}
void Gfx::layoutIssue(const std::string& s) {
  std::string m = (layoutScene.empty() ? "" : "après " + layoutScene + " : ") + s;
  if (std::find(layoutIssues.begin(), layoutIssues.end(), m) == layoutIssues.end()) layoutIssues.push_back(m);
}
static std::string boxStr(int x, int y, int w, int h) {
  return "(" + std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(w) + "x" + std::to_string(h) + ")";
}
void Gfx::rect(float x, float y, float w, float h, Color c) {
  set(c);
  SDL_Rect rc{(int)std::lround(x), (int)std::lround(y), (int)std::lround(w), (int)std::lround(h)};
  SDL_RenderFillRect(r_, &rc);
}
void Gfx::frame(float x, float y, float w, float h, Color c) {
  rect(x, y, w, 1, c);
  rect(x, y + h - 1, w, 1, c);
  rect(x, y, 1, h, c);
  rect(x + w - 1, y, 1, h, c);
}

// Ellipse pleine (éventuellement tournée), remplie ligne par ligne
void Gfx::ellipse(float cx, float cy, float rx, float ry, Color c, float rot) {
  if (rx <= 0 || ry <= 0) return;
  set(c);
  float cs = std::cos(rot), sn = std::sin(rot);
  float A = cs * cs / (rx * rx) + sn * sn / (ry * ry);
  float Bk = 2 * sn * cs * (1 / (rx * rx) - 1 / (ry * ry));
  float Ck = sn * sn / (rx * rx) + cs * cs / (ry * ry);
  float R = std::max(rx, ry);
  int y0 = (int)std::floor(cy - R), y1 = (int)std::ceil(cy + R);
  for (int y = y0; y <= y1; y++) {
    float yy = y + .5f - cy;
    float B = Bk * yy, C = Ck * yy * yy - 1;
    float disc = B * B - 4 * A * C;
    if (disc < 0) continue;
    float sq = std::sqrt(disc);
    float xa = (-B - sq) / (2 * A) + cx, xb = (-B + sq) / (2 * A) + cx;
    span(y, (int)std::ceil(xa - .5f), (int)std::floor(xb - .5f));
  }
}

void Gfx::poly(const std::vector<Pt>& p, Color c) {
  if (p.size() < 3) return;
  set(c);
  float ymin = p[0].y, ymax = p[0].y;
  for (auto& q : p) ymin = std::min(ymin, q.y), ymax = std::max(ymax, q.y);
  std::vector<float> xs;
  for (int y = (int)std::floor(ymin); y <= (int)std::ceil(ymax); y++) {
    float yc = y + .5f;
    xs.clear();
    for (size_t i = 0; i < p.size(); i++) {
      Pt a = p[i], b = p[(i + 1) % p.size()];
      if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc)) xs.push_back(a.x + (yc - a.y) / (b.y - a.y) * (b.x - a.x));
    }
    std::sort(xs.begin(), xs.end());
    for (size_t i = 0; i + 1 < xs.size(); i += 2) span(y, (int)std::ceil(xs[i] - .5f), (int)std::floor(xs[i + 1] - .5f));
  }
}
void Gfx::tri(float x1, float y1, float x2, float y2, float x3, float y3, Color c) { poly({{x1, y1}, {x2, y2}, {x3, y3}}, c); }
void Gfx::line(float x1, float y1, float x2, float y2, float w, Color c) {
  float dx = x2 - x1, dy = y2 - y1, l = std::sqrt(dx * dx + dy * dy);
  if (l < .01f) return;
  float nx = -dy / l * w / 2, ny = dx / l * w / 2;
  poly({{x1 + nx, y1 + ny}, {x2 + nx, y2 + ny}, {x2 - nx, y2 - ny}, {x1 - nx, y1 - ny}}, c);
}
void Gfx::gradV(float x, float y, float w, float h, Color top, Color bot) {
  int H = (int)h;
  for (int i = 0; i < H; i++) rect(x, y + i, w, 1, mix(top, bot, H > 1 ? float(i) / (H - 1) : 0));
}

// ---------------------------------------------------------------------------
// Police pixel : 5 colonnes, 11 lignes (2 pour les accents, 7 pour le corps,
// 2 pour les jambages). Chaque glyphe est dessiné en texte : '#' = pixel.
// ---------------------------------------------------------------------------
struct Glyph {
  std::array<uint8_t, 11> rows{};
};
static void put(Glyph& g, int top, const char* art) {
  int row = top, col = 0;
  for (const char* p = art; *p; p++) {
    if (*p == '/') {
      row++;
      col = 0;
      continue;
    }
    if (*p == '#' && row < 11) g.rows[row] |= uint8_t(1 << (4 - col));
    col++;
  }
}
static std::unordered_map<uint32_t, Glyph> buildFont() {
  std::unordered_map<uint32_t, Glyph> f;
  struct Def {
    uint32_t cp;
    int top;
    const char* art;
  };
  static const Def defs[] = {
      {'A', 2, ".###./#...#/#...#/#####/#...#/#...#/#...#"}, {'B', 2, "####./#...#/#...#/####./#...#/#...#/####."},
      {'C', 2, ".###./#...#/#..../#..../#..../#...#/.###."}, {'D', 2, "####./#...#/#...#/#...#/#...#/#...#/####."},
      {'E', 2, "#####/#..../#..../####./#..../#..../#####"}, {'F', 2, "#####/#..../#..../####./#..../#..../#...."},
      {'G', 2, ".###./#...#/#..../#.###/#...#/#...#/.###."}, {'H', 2, "#...#/#...#/#...#/#####/#...#/#...#/#...#"},
      {'I', 2, ".###./..#../..#../..#../..#../..#../.###."}, {'J', 2, "..###/...#./...#./...#./#..#./#..#./.##.."},
      {'K', 2, "#...#/#..#./#.#../##.../#.#../#..#./#...#"}, {'L', 2, "#..../#..../#..../#..../#..../#..../#####"},
      {'M', 2, "#...#/##.##/#.#.#/#.#.#/#...#/#...#/#...#"}, {'N', 2, "#...#/##..#/#.#.#/#..##/#...#/#...#/#...#"},
      {'O', 2, ".###./#...#/#...#/#...#/#...#/#...#/.###."}, {'P', 2, "####./#...#/#...#/####./#..../#..../#...."},
      {'Q', 2, ".###./#...#/#...#/#...#/#.#.#/#..#./.##.#"}, {'R', 2, "####./#...#/#...#/####./#.#../#..#./#...#"},
      {'S', 2, ".####/#..../#..../.###./....#/....#/####."}, {'T', 2, "#####/..#../..#../..#../..#../..#../..#.."},
      {'U', 2, "#...#/#...#/#...#/#...#/#...#/#...#/.###."}, {'V', 2, "#...#/#...#/#...#/#...#/.#.#./.#.#./..#.."},
      {'W', 2, "#...#/#...#/#...#/#.#.#/#.#.#/##.##/#...#"}, {'X', 2, "#...#/#...#/.#.#./..#../.#.#./#...#/#...#"},
      {'Y', 2, "#...#/#...#/.#.#./..#../..#../..#../..#.."}, {'Z', 2, "#####/....#/...#./..#../.#.../#..../#####"},
      {'a', 4, ".###./....#/.####/#...#/.####"}, {'b', 2, "#..../#..../####./#...#/#...#/#...#/####."},
      {'c', 4, ".###./#..../#..../#..../.###."}, {'d', 2, "....#/....#/.####/#...#/#...#/#...#/.####"},
      {'e', 4, ".###./#...#/#####/#..../.###."}, {'f', 2, "..##./.#.../####./.#.../.#.../.#.../.#..."},
      {'g', 4, ".####/#...#/#...#/.####/....#/.###."}, {'h', 2, "#..../#..../####./#...#/#...#/#...#/#...#"},
      {'i', 2, "..#../..../.##../..#../..#../..#../.###."}, {'j', 2, "...#./..../..##./...#./...#./...#./#..#./.##.."},
      {'k', 2, "#..../#..../#..#./#.#../##.../#.#../#..#."}, {'l', 2, ".##../..#../..#../..#../..#../..#../.###."},
      {'m', 4, "##.#./#.#.#/#.#.#/#.#.#/#...#"}, {'n', 4, "####./#...#/#...#/#...#/#...#"},
      {'o', 4, ".###./#...#/#...#/#...#/.###."}, {'p', 4, "####./#...#/#...#/####./#..../#...."},
      {'q', 4, ".####/#...#/#...#/.####/....#/....#"}, {'r', 4, "#.##./##..#/#..../#..../#...."},
      {'s', 4, ".####/#..../.###./....#/####."}, {'t', 2, ".#.../.#.../####./.#.../.#.../.#..#/..##."},
      {'u', 4, "#...#/#...#/#...#/#..##/.##.#"}, {'v', 4, "#...#/#...#/#...#/.#.#./..#.."},
      {'w', 4, "#...#/#.#.#/#.#.#/#.#.#/.#.#."}, {'x', 4, "#...#/.#.#./..#../.#.#./#...#"},
      {'y', 4, "#...#/#...#/#...#/.####/....#/.###."}, {'z', 4, "#####/...#./..#../.#.../#####"},
      {'0', 2, ".###./#...#/#..##/#.#.#/##..#/#...#/.###."}, {'1', 2, "..#../.##../..#../..#../..#../..#../.###."},
      {'2', 2, ".###./#...#/....#/...#./..#../.#.../#####"}, {'3', 2, "####./....#/....#/.###./....#/....#/####."},
      {'4', 2, "...#./..##./.#.#./#..#./#####/...#./...#."}, {'5', 2, "#####/#..../####./....#/....#/#...#/.###."},
      {'6', 2, ".###./#..../#..../####./#...#/#...#/.###."}, {'7', 2, "#####/....#/...#./..#../.#.../.#.../.#..."},
      {'8', 2, ".###./#...#/#...#/.###./#...#/#...#/.###."}, {'9', 2, ".###./#...#/#...#/.####/....#/....#/.###."},
      {'.', 8, "..#.."}, {',', 8, "..#../..#../.#..."}, {'!', 2, "..#../..#../..#../..#../..#../...../..#.."},
      {'?', 2, ".###./#...#/....#/..##./..#../...../..#.."}, {':', 4, "..#../...../...../...../..#.."},
      {';', 4, "..#../...../...../...../..#../.#..."}, {'\'', 2, "..#../..#../.#..."}, {0x2019, 2, "..#../..#../.#..."},
      {'-', 5, ".###."}, {'_', 9, "#####"}, {0x2013, 5, "#####"}, {0x2014, 5, "#####"}, {'+', 4, "..#../..#../#####/..#../..#.."},
      {'/', 2, "....#/...#./...#./..#../.#.../.#.../#...."}, {'(', 2, "...#./..#../.#.../.#.../.#.../..#../...#."},
      {')', 2, ".#.../..#../...#./...#./...#./..#../.#..."}, {'%', 2, "##..#/##..#/...#./..#../.#.../#..##/#..##"},
      {0xD7, 4, "#...#/.#.#./..#../.#.#./#...#"}, {'"', 2, ".#.#./.#.#."}, {0xAB, 4, "..#.#/.#.#./#.#../.#.#./..#.#"},
      {0xBB, 4, "#.#../.#.#./..#.#/.#.#./#.#.."}, {0x25BC, 5, "#####/.###./..#.."}, {0x25B6, 2, "#..../##.../###../####./###../##.../#...."},
      {0x2605, 2, "..#../..#../#####/.###./.#.#./#...#"}, {0x2026, 8, "#.#.#"}, {'=', 4, "#####/...../#####"},
      {0xB7, 5, "..#.."}, {'#', 3, ".#.#./#####/.#.#./#####/.#.#."}, {'<', 3, "...#./..#../.#.../..#../...#."},
      {'>', 3, ".#.../..#../...#./..#../.#..."}, {'*', 3, "#.#.#/.###./#####/.###./#.#.#"}, {0x2665, 3, ".#.#./#####/#####/.###./..#.."},
  };
  for (auto& d : defs) {
    Glyph g;
    put(g, d.top, d.art);
    f[d.cp] = g;
  }
  f[' '] = Glyph{};
  f[0xA0] = Glyph{};
  // Lettres accentuées : lettre de base + accent
  const char* acute = "...#./..#..";
  const char* grave = ".#.../..#..";
  const char* circ = "..#../.#.#.";
  const char* trema = ".#.#./.....";
  struct Acc {
    uint32_t cp;
    char base;
    const char* acc;
  };
  static const Acc acc[] = {
      {0xE9, 'e', acute}, {0xE8, 'e', grave}, {0xEA, 'e', circ}, {0xEB, 'e', trema}, {0xE0, 'a', grave}, {0xE2, 'a', circ},
      {0xE4, 'a', trema}, {0xF9, 'u', grave}, {0xFB, 'u', circ},  {0xFC, 'u', trema}, {0xF4, 'o', circ},  {0xF6, 'o', trema},
      {0xC9, 'E', acute}, {0xC8, 'E', grave}, {0xCA, 'E', circ},  {0xC0, 'A', grave}, {0xC2, 'A', circ},  {0xD4, 'O', circ},
      {0xCE, 'I', circ},  {0xD9, 'U', grave}, {0xDB, 'U', circ},
  };
  for (auto& a : acc) {
    Glyph g = f[(uint32_t)a.base];
    bool upper = a.base >= 'A' && a.base <= 'Z';
    put(g, upper ? 0 : 2, a.acc);
    f[a.cp] = g;
  }
  // i sans point pour î et ï
  const std::pair<uint32_t, const char*> dotless[] = {{0xEE, circ}, {0xEF, trema}};
  for (auto& [cp, a] : dotless) {
    Glyph g;
    put(g, 4, ".##../..#../..#../..#../.###.");
    put(g, 2, a);
    f[cp] = g;
  }
  // Cédille
  Glyph c = f['c'];
  put(c, 9, "..#../.#...");
  f[0xE7] = c;
  Glyph C = f['C'];
  put(C, 9, "..#../.#...");
  f[0xC7] = C;
  // Ligatures (approximations)
  f[0x153] = f['e'];
  return f;
}
static const std::unordered_map<uint32_t, Glyph>& font() {
  static auto f = buildFont();
  return f;
}

static std::vector<uint32_t> decode(const std::string& s) {
  std::vector<uint32_t> out;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = s[i];
    uint32_t cp;
    int n;
    if (c < 0x80) cp = c, n = 1;
    else if ((c >> 5) == 6) cp = c & 0x1F, n = 2;
    else if ((c >> 4) == 14) cp = c & 0x0F, n = 3;
    else cp = c & 0x07, n = 4;
    for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3F);
    out.push_back(cp);
    i += n;
  }
  return out;
}
int Gfx::textW(const std::string& s) { return int(decode(s).size()) * 6; }

void Gfx::text(float x, float y, const std::string& s, Color c, int align, bool shadow) {
  auto cps = decode(s);
  int w = int(cps.size()) * 6;
  int X = (int)std::lround(x - (align == 1 ? w / 2 : align == 2 ? w : 0)), Y = (int)std::lround(y);
  // Mode test : le texte doit tenir dans la dernière fenêtre dessinée sous lui, sans toucher un autre texte
  while (checkLayout && w > 0 && !s.empty() && s.find_first_not_of(' ') != std::string::npos) {
    int px = align == 0 ? X + 1 : align == 2 ? X + w - 1 : X + w / 2, py = Y + 5;
    size_t k = wins_.size();
    while (k > 0 && !(px >= wins_[k - 1].x && px < wins_[k - 1].x + wins_[k - 1].w && py >= wins_[k - 1].y && py < wins_[k - 1].y + wins_[k - 1].h)) k--;
    if (k == 0) break;  // texte posé sur le décor
    const Box& b = wins_[k - 1];
    Box t{X, Y, w - 1, 10};
    if (t.x < b.x + 2 || t.x + t.w > b.x + b.w - 2 || t.y < b.y + 1 || t.y + t.h > b.y + b.h - 1)
      layoutIssue("texte qui dépasse de sa fenêtre " + boxStr(b.x, b.y, b.w, b.h) + " : « " + s + " »");
    for (auto& o : texts_)
      if (o.win == k && t.x < o.b.x + o.b.w && o.b.x < t.x + t.w && t.y < o.b.y + o.b.h && o.b.y < t.y + t.h)
        layoutIssue("textes qui se chevauchent dans " + boxStr(b.x, b.y, b.w, b.h) + " : « " + o.s + " » et « " + s + " »");
    texts_.push_back({k, t, s});
    break;
  }
  auto& F = font();
  for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
    set(pass == 0 ? Color{0, 0, 0, uint8_t(c.a * 0.85f)} : c);
    int cx = X + (pass == 0 ? 1 : 0), cy = Y + (pass == 0 ? 1 : 0);
    for (auto cp : cps) {
      auto it = F.find(cp);
      if (it == F.end()) it = F.find('?');
      for (int r = 0; r < 11; r++) {
        uint8_t bits = it->second.rows[r];
        int run = -1;
        for (int k = 0; k <= 5; k++) {
          bool on = k < 5 && (bits & (1 << (4 - k)));
          if (on && run < 0) run = k;
          if (!on && run >= 0) {
            span(cy + r, cx + run, cx + k - 1);
            run = -1;
          }
        }
      }
      cx += 6;
    }
  }
}

void Gfx::textBig(float x, float y, const std::string& s, Color c, int k, int align) {
  auto cps = decode(s);
  int w = int(cps.size()) * 6 * k;
  int X = (int)std::lround(x - (align == 1 ? w / 2 : align == 2 ? w : 0)), Y = (int)std::lround(y);
  auto& F = font();
  for (int pass = 0; pass < 2; pass++) {
    set(pass == 0 ? Color{0, 0, 0, uint8_t(c.a * .85f)} : c);
    int cx = X + (pass == 0 ? k : 0), cy = Y + (pass == 0 ? k : 0);
    for (auto cp : cps) {
      auto it = F.find(cp);
      if (it == F.end()) it = F.find('?');
      for (int r = 0; r < 11; r++)
        for (int q = 0; q < 5; q++)
          if (it->second.rows[r] & (1 << (4 - q))) {
            SDL_Rect rc{cx + q * k, cy + r * k, k, k};
            SDL_RenderFillRect(r_, &rc);
          }
      cx += 6 * k;
    }
  }
}

std::vector<std::string> Gfx::wrap(const std::string& s, int width) {
  std::vector<std::string> lines;
  size_t maxc = (size_t)std::max(1, width / 6);
  std::string para;
  auto flush = [&](const std::string& p) {
    std::string line, word;
    auto add = [&]() {
      if (word.empty()) return;
      std::string cand = line.empty() ? word : line + " " + word;
      if (decode(cand).size() > maxc && !line.empty()) {
        lines.push_back(line);
        line = word;
      } else line = cand;
      word.clear();
    };
    for (char ch : p) {
      if (ch == ' ') add();
      else word += ch;
    }
    add();
    lines.push_back(line);
  };
  size_t start = 0;
  while (true) {
    size_t nl = s.find('\n', start);
    flush(s.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return lines;
}

void Gfx::window(float x, float y, float w, float h) {
  if (checkLayout) {
    Box b{(int)x, (int)y, (int)w, (int)h};
    if (b.x < 0 || b.y < 0 || b.x + b.w > SCREEN_W || b.y + b.h > SCREEN_H) layoutIssue("fenêtre hors de l'écran " + boxStr(b.x, b.y, b.w, b.h));
    // Une fenêtre peut recouvrir entièrement une autre, ou s'ouvrir bien à l'intérieur (question,
    // sous-menu) ; sinon elle chevauche de travers et laisse dépasser des morceaux de l'autre
    for (auto& o : wins_) {
      if (o.hidden) continue;
      bool meet = b.x < o.x + o.w && o.x < b.x + b.w && b.y < o.y + o.h && o.y < b.y + b.h;
      bool covers = b.x <= o.x && b.y <= o.y && b.x + b.w >= o.x + o.w && b.y + b.h >= o.y + o.h;
      bool inside = b.x >= o.x + 4 && b.y >= o.y + 4 && b.x + b.w <= o.x + o.w - 4 && b.y + b.h <= o.y + o.h - 4;
      if (covers) o.hidden = true;
      else if (meet && !inside) layoutIssue("fenêtres qui se chevauchent " + boxStr(o.x, o.y, o.w, o.h) + " et " + boxStr(b.x, b.y, b.w, b.h));
    }
    wins_.push_back(b);
  }
  gradV(x + 1, y + 1, w - 2, h - 2, rgb(0x3048b0), rgb(0x0c1452));
  frame(x, y, w, h, rgb(0xd8def2));
  frame(x + 1, y + 1, w - 2, h - 2, rgb(0x5a6aa8, 160));
}
void Gfx::cursor(float x, float y) {
  tri(x, y, x, y + 8, x + 6, y + 4, rgb(0x2a1e00));
  tri(x - 1, y - 1, x - 1, y + 7, x + 5, y + 3, rgb(0xffd34d));
}
