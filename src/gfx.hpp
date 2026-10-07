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

  // Écran plus large que le jeu (16:9, 16:10…) : l'image fait fullW pixels de large (320 au
  // moins) et tout se place dans la zone de 320 pixels du milieu, décalée de ox pixels. Un
  // décor qui doit remplir tout l'écran va de left() (négatif) à right() (au-delà de 320).
  int fullW = SCREEN_W, ox = 0;
  float left() const { return float(-ox); }
  float right() const { return float(fullW - ox); }

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

  // Vérification de la mise en page (mode test) : textes qui sortent de leur
  // fenêtre ou se chevauchent, fenêtres posées de travers l'une sur l'autre.
  // Les problèmes trouvés depuis clear() sont ajoutés à layoutIssues.
  bool checkLayout = false;
  std::string layoutScene;  // préfixe des messages (dernière capture du mode test)
  std::vector<std::string> layoutIssues;
  void newLayer();          // ce qui suit passe au premier plan (fenêtre modale sur fond assombri)
  void layoutIssue(const std::string& s);  // note un problème (une seule fois)

 private:
  void set(Color c);
  void span(int y, int x0, int x1);
  SDL_Renderer* r_;
  struct Box {
    int x, y, w, h;
    bool hidden = false;  // entièrement recouverte par une fenêtre dessinée ensuite
  };
  struct TextBox {
    size_t win;  // fenêtre qui contient le texte
    Box b;
    std::string s;
  };
  std::vector<Box> wins_;       // fenêtres de l'image en cours
  std::vector<TextBox> texts_;  // textes de l'image en cours
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
