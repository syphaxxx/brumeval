// Petit moteur de dessin 2D au-dessus de SDL_Renderer.
// Tout est dessiné dans une image de 320x240 pixels, agrandie à l'écran.
#pragma once
#include <SDL.h>

#include <cstdint>
#include <string>
#include <vector>

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

struct Color {
  uint8_t r = 0, g = 0, b = 0, a = 255;
};
Color rgb(uint32_t hex, uint8_t a = 255);
Color shade(Color c, float k);  // k > 0 assombrit, k < 0 éclaircit
Color mix(Color a, Color b, float t);

struct Pt {
  float x, y;
};

class Gfx {
 public:
  explicit Gfx(SDL_Renderer* r) : r_(r) {}
  SDL_Renderer* renderer() const { return r_; }

  float alpha = 1.f;  // opacité globale appliquée à tout ce qui est dessiné

  void clear(Color c);
  void rect(float x, float y, float w, float h, Color c);
  void frame(float x, float y, float w, float h, Color c);  // contour 1 px
  void ellipse(float cx, float cy, float rx, float ry, Color c, float rot = 0);
  void poly(const std::vector<Pt>& pts, Color c);
  void tri(float x1, float y1, float x2, float y2, float x3, float y3, Color c);
  void line(float x1, float y1, float x2, float y2, float w, Color c);
  void gradV(float x, float y, float w, float h, Color top, Color bottom);

  // Texte (police pixel intégrée, accents français compris)
  void text(float x, float y, const std::string& s, Color c, int align = 0, bool shadow = true);  // align : 0 gauche, 1 centre, 2 droite
  void textBig(float x, float y, const std::string& s, Color c, int scale, int align = 0);
  static int textW(const std::string& s);
  static std::vector<std::string> wrap(const std::string& s, int width);
  static constexpr int LINE_H = 12;

  // Fenêtre bleue façon JRPG
  void window(float x, float y, float w, float h);
  void cursor(float x, float y);  // petite main/flèche dorée

 private:
  void set(Color c);
  void span(int y, int x0, int x1);
  SDL_Renderer* r_;
};

// Transformation locale -> écran pour dessiner les sprites (décalage, échelle, miroir)
struct Xf {
  Gfx& g;
  float ox, oy, s;
  bool flip;
  float X(float x) const { return ox + (flip ? -x : x) * s; }
  float Y(float y) const { return oy + y * s; }
  void ell(float x, float y, float rx, float ry, Color c, float rot = 0) const {
    g.ellipse(X(x), Y(y), rx * s, ry * s, c, flip ? -rot : rot);
  }
  void rect(float x, float y, float w, float h, Color c) const {
    float x0 = flip ? X(x + w) : X(x);
    g.rect(x0, Y(y), w * s, h * s, c);
  }
  void tri(float a, float b, float c, float d, float e, float f, Color col) const {
    g.tri(X(a), Y(b), X(c), Y(d), X(e), Y(f), col);
  }
  void poly(std::vector<Pt> p, Color c) const {
    for (auto& q : p) q = {X(q.x), Y(q.y)};
    g.poly(p, c);
  }
  void line(float a, float b, float c, float d, float w, Color col) const { g.line(X(a), Y(b), X(c), Y(d), w * s, col); }
};
