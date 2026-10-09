#include "sprites.hpp"

#include <cmath>

static const Color INK = rgb(0x1a1a24);
static void eyes(const Xf& x, float x1, float x2, float y, float r = 2) {
  for (float e : {x1, x2}) {
    x.ell(e, y, r, r + .6f, INK);
    x.ell(e - .6f, y - .9f, .7f, .7f, rgb(0xffffff));
  }
}

// ---------------------------------------------------------------------------
// Créatures (regard tourné vers la gauche par défaut)
// ---------------------------------------------------------------------------
void drawCreature(Gfx& g, const std::string& id, float x, float y, float s, bool flip, float t) {
  const Species& sp = species(id);
  Color C = rgb(sp.c1), L = rgb(sp.c2), D = shade(C, .3f);
  Xf X{g, x, y + std::round(std::sin(t * 3.1f)), s, flip};
  X.ell(0, 15, 15, 3.5f, rgb(0, 46));
  switch (sp.shape) {
    case Shape::Fox:
      X.ell(13, -2, 6, 9, C, .5f); X.ell(16, -9, 4, 4, L);
      X.tri(-13, -6, -9, -20, -4, -8, C); X.tri(1, -8, 4, -20, 8, -6, C); X.tri(-11, -8, -9, -16, -6, -8, L);
      X.ell(0, 2, 14, 12, C); X.ell(-2, 7, 8, 6, L); X.ell(-7, 13, 4, 2.5f, D); X.ell(6, 13, 4, 2.5f, D);
      eyes(X, -8, -1, -2); X.ell(-13, 2, 1.6f, 1.4f, INK);
      break;
    case Shape::Drop:
      X.tri(-11, -2, 11, -2, 0, -22, C); X.ell(0, 4, 13, 12, C); X.ell(-5, -5, 2.5f, 5, L); X.ell(0, 9, 8, 5, L);
      X.ell(-6, 14, 4, 2.5f, D); X.ell(6, 14, 4, 2.5f, D); eyes(X, -6, 2, 1);
      break;
    case Shape::Bud:
      X.ell(-5, -12, 3.5f, 8, rgb(0x2f7a3e), -.6f); X.ell(5, -12, 3.5f, 8, rgb(0x2f7a3e), .6f); X.ell(0, -12, 4, 5, rgb(0xe98aa8));
      X.ell(0, 3, 13, 12, C); X.ell(-1, 8, 8, 5, L); X.ell(-6, 14, 4, 2.5f, D); X.ell(6, 14, 4, 2.5f, D); eyes(X, -6, 2, 0);
      break;
    case Shape::Bird: {
      Color beak = rgb(0xf0a030);
      X.tri(-2, -9, 0, -17, 4, -9, D); X.ell(0, 2, 13, 11, C);
      float flap = std::sin(t * 9) * .3f;
      X.ell(5, 3, 8, 5, D, .4f + flap); X.ell(-3, 7, 7, 4, L);
      X.tri(-12, -1, -19, 2, -12, 4, beak); X.ell(-5, 13, 3, 2, beak); X.ell(4, 13, 3, 2, beak); eyes(X, -7, -1, -3);
      break;
    }
    case Shape::Mouse:
      X.line(12, 8, 18, 6, 1.5f, D); X.line(18, 6, 20, -4, 1.5f, D);
      X.ell(-8, -10, 5, 5, C); X.ell(-8, -10, 3, 3, L); X.ell(6, -10, 5, 5, C); X.ell(6, -10, 3, 3, L);
      X.ell(0, 3, 13, 11, C); X.ell(-2, 8, 7, 5, L); X.ell(-6, 13, 4, 2.5f, D); X.ell(6, 13, 4, 2.5f, D);
      eyes(X, -6, 1, -1); X.ell(-12, 3, 1.8f, 1.5f, rgb(0xe89ab0));
      break;
    case Shape::Bug: {
      X.ell(4, -9, 6, 9, rgb(0xdcf0ff, 165), -.4f); X.ell(11, -7, 5, 8, rgb(0xdcf0ff, 140), .3f);
      X.line(-6, -6, -10, -14, 1, D); X.line(-2, -7, -3, -15, 1, D);
      X.ell(0, 2, 11, 9, C);
      Color glow = L;
      glow.a = uint8_t(90 + 60 * std::sin(t * 5));
      X.ell(12, 6, 9, 8, glow); X.ell(11, 6, 6, 5, L); X.ell(-1, 6, 6, 3, L); eyes(X, -6, 0, 0);
      break;
    }
    case Shape::Frog:
      X.ell(-7, -6, 5, 5, C); X.ell(5, -6, 5, 5, C); X.ell(0, 4, 15, 10, C); X.ell(0, 8, 10, 5, L);
      X.ell(-10, 13, 5, 2.5f, D); X.ell(10, 13, 5, 2.5f, D); eyes(X, -7, 5, -6);
      X.line(-8, 3, -1, 5, 1, D); X.line(-1, 5, 6, 3, 1, D);
      break;
    case Shape::Mush: {
      X.ell(0, 7, 10, 9, L);
      std::vector<Pt> cap;
      for (int i = 0; i <= 16; i++) {
        float a = 3.14159f + 3.14159f * i / 16;
        cap.push_back({17 * std::cos(a), 13 * std::sin(a)});
      }
      X.poly(cap, C);
      X.ell(-8, -5, 2.5f, 2, rgb(0xffffff)); X.ell(3, -9, 3, 2.2f, rgb(0xffffff)); X.ell(9, -3, 2, 1.6f, rgb(0xffffff));
      X.ell(-5, 14, 4, 2.5f, shade(L, .25f)); X.ell(5, 14, 4, 2.5f, shade(L, .25f)); eyes(X, -5, 2, 5);
      break;
    }
    case Shape::Rock:
      X.poly({{-14, 12}, {-16, -2}, {-8, -12}, {6, -13}, {15, -3}, {14, 12}}, C);
      X.poly({{-8, -12}, {6, -13}, {2, -6}, {-9, -6}}, L);
      X.line(8, -2, 4, 4, 1, D); X.line(4, 4, 9, 9, 1, D); eyes(X, -8, -1, 0);
      break;
    case Shape::Golem: {
      X.poly({{-18, 14}, {-20, -4}, {-11, -17}, {8, -18}, {19, -5}, {18, 14}}, C);
      X.poly({{-11, -17}, {8, -18}, {3, -9}, {-12, -9}}, shade(C, -.25f));
      X.ell(-22, 2, 6, 9, C); X.ell(21, 2, 6, 9, C);
      Color glow = L;
      glow.a = uint8_t(170 + 80 * std::sin(t * 4));
      X.line(10, -6, 4, 2, 1.5f, glow); X.line(4, 2, 11, 9, 1.5f, glow); X.line(-14, 4, -7, 10, 1.5f, glow);
      X.ell(-9, -4, 3, 2, glow); X.ell(0, -4, 3, 2, glow);
      break;
    }
    case Shape::Wisp: {
      Color c = C;
      c.a = 215;
      X.tri(4, 0, 22, 10 + std::sin(t * 5.5f) * 3, 6, 10, c); X.ell(0, 0, 11, 12, c); X.ell(-2, 4, 6, 5, L);
      X.ell(-5, -2, 2, 2.6f, rgb(0xffffff)); X.ell(1, -2, 2, 2.6f, rgb(0xffffff));
      X.ell(-5, -1.5f, 1, 1.4f, INK); X.ell(1, -1.5f, 1, 1.4f, INK);
      break;
    }
    case Shape::Lizard: {
      X.tri(12, 2, 28, -2 + std::sin(t * 4) * 2, 12, 9, C);
      X.ell(2, 4, 15, 8, C); X.ell(0, 8, 10, 3.5f, L);
      for (int i = 0; i < 4; i++) X.tri(-6 + i * 6.f, -3, -3 + i * 6.f, -10, 0 + i * 6.f, -3, L);
      X.ell(-14, -1, 8, 6, C); X.ell(-19, 1, 3, 2, D);
      X.ell(-8, 12, 3.5f, 2.5f, D); X.ell(8, 12, 3.5f, 2.5f, D);
      eyes(X, -16, -11, -3, 1.6f);
      break;
    }
    case Shape::Boss: {
      Color halo = L;
      halo.a = 64;
      X.ell(0, 4, 30, 22, halo);
      X.line(-8, -14, -14, -30, 2.5f, D); X.line(-11, -22, -20, -26, 2.5f, D);
      X.line(4, -14, 10, -30, 2.5f, D); X.line(7, -22, 16, -27, 2.5f, D);
      X.ell(0, 0, 20, 18, C); X.ell(-2, 8, 11, 8, shade(C, -.25f)); X.ell(-12, 15, 5, 3, D); X.ell(9, 15, 5, 3, D);
      Color e = L;
      e.a = uint8_t(150 + 100 * std::sin(t * 4));
      X.ell(-9, -4, 3.5f, 2, e); X.ell(2, -4, 3.5f, 2, e);
      break;
    }
    case Shape::Snake: {
      float w = std::sin(t * 3) * 1.5f;
      X.ell(6, 11, 14, 4.5f, D); X.ell(4, 7, 12, 5, C); X.ell(2, 2, 10, 5, C); X.ell(4, 9, 9, 2.5f, L);
      for (int i = 0; i < 3; i++) X.ell(9 - i * 5.f, 4 - i * 1.f, 1.5f, 3, D);
      X.ell(-6, -5 + w, 5, 7, C); X.ell(-10, -12 + w, 7, 5, C); X.ell(-11, -9 + w, 4, 2, L);
      X.line(-16, -10 + w, -21, -9 + w, 1, rgb(0xd2493f)); X.line(-21, -9 + w, -23, -11 + w, 1, rgb(0xd2493f));
      X.line(-21, -9 + w, -23, -7 + w, 1, rgb(0xd2493f));
      eyes(X, -13, -8, -14 + w, 1.6f);
      break;
    }
    case Shape::Bat: {
      float fl = std::sin(t * 12) * 4, y0 = std::sin(t * 3) * 2 - 5;
      X.tri(-4, y0 - 1, -24, y0 - 9 + fl, -13, y0 + 7, D); X.tri(4, y0 - 1, 24, y0 - 9 + fl, 13, y0 + 7, D);
      X.tri(-6, y0, -20, y0 - 6 + fl, -12, y0 + 4, C); X.tri(6, y0, 20, y0 - 6 + fl, 12, y0 + 4, C);
      X.ell(0, y0, 8, 9, C);
      X.tri(-6, y0 - 6, -5, y0 - 15, -1, y0 - 7, C); X.tri(1, y0 - 7, 5, y0 - 15, 6, y0 - 6, C);
      X.ell(0, y0 + 3, 5, 4, L); eyes(X, -4, 2, y0 - 2, 1.6f);
      X.tri(-2, y0 + 2, -1, y0 + 5, 0, y0 + 2, rgb(0xffffff));
      break;
    }
    case Shape::Wolf:
      X.ell(15, -4, 4, 9, C, .9f); X.ell(18, -10, 3, 4, L);
      X.ell(3, 3, 14, 9, C); X.ell(1, 8, 9, 4, L);
      X.ell(-9, 13, 3, 3, D); X.ell(-3, 13, 3, 3, D); X.ell(8, 13, 3, 3, D); X.ell(14, 12, 3, 3, D);
      X.ell(-11, -5, 8, 7, C); X.tri(-17, -5, -26, -1, -16, 1, C); X.ell(-25, -1.5f, 1.6f, 1.4f, INK);
      X.tri(-15, -9, -14, -19, -9, -10, C); X.tri(-8, -10, -5, -19, -3, -9, C); X.tri(-14, -10, -13.5f, -16, -11, -10, L);
      eyes(X, -15, -9, -6, 1.7f);
      break;
    case Shape::Turtle: {
      X.ell(-16, 3, 6, 5, L); eyes(X, -19, -15, 1, 1.4f);
      X.ell(-9, 12, 4, 3, L); X.ell(9, 12, 4, 3, L);
      std::vector<Pt> shell;
      for (int i = 0; i <= 16; i++) {
        float a = 3.14159f + 3.14159f * i / 16;
        shell.push_back({15 * std::cos(a), 14 * std::sin(a) + 8});
      }
      X.poly(shell, C);
      X.rect(-15, 7, 30, 3, D);
      X.ell(-6, 0, 4, 3, D); X.ell(5, -1, 4, 3, D); X.ell(0, -5, 3, 2, D);
      break;
    }
    case Shape::Ghost: {
      Color c = C;
      c.a = 205;
      float y0 = std::sin(t * 2.5f) * 2 - 3;
      X.ell(0, y0 - 4, 12, 11, c); X.rect(-12, y0 - 4, 24, 12, c);
      for (int i = 0; i < 4; i++) X.tri(-12 + i * 6.f, y0 + 8, -9 + i * 6.f, y0 + 14 + std::sin(t * 6 + i) * 2, -6 + i * 6.f, y0 + 8, c);
      X.ell(-14, y0, 3, 5, c, .5f); X.ell(13, y0, 3, 5, c, -.5f);
      X.ell(-4, y0 - 6, 2.5f, 3.5f, INK); X.ell(4, y0 - 6, 2.5f, 3.5f, INK);
      X.ell(-4, y0 - 7, .9f, .9f, L); X.ell(4, y0 - 7, .9f, .9f, L); X.ell(0, y0 + 1, 3, 2, shade(C, .45f));
      break;
    }
    case Shape::Crystal: {
      Color glow = L;
      glow.a = uint8_t(60 + 40 * std::sin(t * 3));
      X.ell(0, 0, 20, 18, glow);
      X.poly({{-14, 14}, {-17, 0}, {-11, -8}, {-7, 14}}, D); X.poly({{8, 14}, {12, -6}, {17, 2}, {16, 14}}, D);
      X.poly({{-8, 14}, {-10, -6}, {0, -20}, {9, -6}, {7, 14}}, C); X.poly({{0, -20}, {9, -6}, {2, -4}}, L);
      eyes(X, -4, 3, 1, 1.8f);
      break;
    }
    case Shape::Magma: {  // titan de lave (Ignarok) : roche sombre, fissures brûlantes, cornes, crête de flammes
      Color rock = shade(C, .55f), hot = L, core = rgb(0xffe27a);
      float pulse = .5f + .5f * std::sin(t * 4);
      Color heat = L;
      heat.a = uint8_t(40 + 30 * pulse);
      X.ell(2, 2, 34, 26, heat);
      for (int i = 0; i < 4; i++) {  // crête de flammes sur le dos
        float f = std::sin(t * 9 + i * 1.7f) * 3;
        X.tri(-4 + i * 8.f, -14, 0 + i * 8.f, -30 - f - (i == 1 || i == 2 ? 5 : 0), 4 + i * 8.f, -14, hot);
        X.tri(-1 + i * 8.f, -14, 0 + i * 8.f, -22 - f, 2 + i * 8.f, -14, core);
      }
      X.poly({{-18, 16}, {-22, 0}, {-12, -15}, {6, -19}, {22, -10}, {27, 4}, {24, 16}}, rock);  // corps voûté
      X.ell(-14, 12, 7, 6, rock); X.ell(14, 13, 7, 5, rock);  // pattes
      for (int i = 0; i < 3; i++) X.tri(-20 + i * 3.f, 16, -18.5f + i * 3.f, 20, -17 + i * 3.f, 16, rgb(0xf3e3c8));
      Color crack = hot;
      crack.a = uint8_t(150 + 105 * pulse);
      X.line(2, -12, 8, -2, 1.5f, crack); X.line(8, -2, 4, 8, 1.5f, crack); X.line(14, -8, 19, 3, 1.5f, crack);
      X.line(-6, 2, 0, 10, 1.5f, crack); X.ell(8, 6, 6, 4, crack);  // ventre en fusion
      Color horn = rgb(0xe8d7b0);
      X.tri(-14, -13, -1, -28, -8, -10, horn); X.tri(-23, -14, -20, -29, -16, -12, horn);  // cornes
      X.ell(-21, -6, 13, 10, C);  // tête
      X.tri(-34, -2, -21, 1, -29, 8, rock);  // mâchoire
      X.ell(-28, 1, 4.5f, 2.8f, core);       // gueule brûlante
      X.ell(-26, -9, 3, 1.8f, core); X.ell(-18, -9, 3, 1.8f, core);  // yeux
      float drip = std::fmod(t * 1.3f, 1.f);  // goutte de lave qui tombe de la gueule
      Color lava = hot;
      lava.a = uint8_t(255 * (1 - drip));
      X.ell(-27, 4 + drip * 12, 1.4f, 2, lava);
      break;
    }
    case Shape::Human: break;
  }
}

// ---------------------------------------------------------------------------
// Humains
// ---------------------------------------------------------------------------
void drawHuman(Gfx& g, const Look& lk, float x, float y, float s, int dir, int step, bool weapon) {
  bool flip = dir == 3;
  int pose = flip ? 2 : dir;
  Xf X{g, flip ? x + 16 * s : x, y, s, flip};
  Color hair = rgb(lk.hair), skin = rgb(lk.skin), top = rgb(lk.top), bot = rgb(lk.bottom);
  X.ell(8, 15, 6, 2, rgb(0, 50));
  if (weapon && lk.weapon == 2) {  // bâton derrière le corps
    X.rect(1, -4, 1, 18, rgb(0x8a5a32));
    X.ell(1.5f, -5, 2.5f, 2.5f, rgb(0xbfe9ff));
  }
  X.rect(5, 12 + (step == 1 ? -1 : 0), 2, 3, bot);
  X.rect(9, 12 + (step == 2 ? -1 : 0), 2, 3, bot);
  if (lk.hat == 1) X.rect(3, 6, 10, 7, top);  // cape
  X.rect(4, 7, 8, 6, top);
  X.rect(4, 12, 8, 1, shade(top, .25f));
  X.rect(3, 8, 1, 4, shade(top, .15f));
  X.rect(12, 8, 1, 4, shade(top, .15f));
  X.rect(4, 2, 8, 6, skin);
  X.rect(4, 1, 8, 2, hair);
  if (pose == 0) X.rect(4, 2, 8, 5, hair);
  else if (pose == 1) {
    X.rect(6, 5, 1, 1, INK);
    X.rect(9, 5, 1, 1, INK);
    X.rect(4, 2, 1, 3, hair);
    X.rect(11, 2, 1, 3, hair);
  } else {
    X.rect(5, 5, 1, 1, INK);
    X.rect(9, 2, 3, 5, hair);
  }
  if (lk.hat == 1) {
    X.rect(3, 0, 10, 3, top);
    X.rect(3, 2, 1, 5, top);
    X.rect(12, 2, 1, 5, top);
  } else if (lk.hat == 2) {
    Color h = shade(top, .35f);
    X.rect(1, 1, 14, 2, h);
    X.tri(3, 1.5f, 13, 1.5f, 10, -9, h);
  } else if (lk.hat == 4) {  // casque
    Color m = rgb(0xb8c0cc);
    X.rect(3, 0, 10, 4, m);
    X.rect(3, 3, 1, 4, m);
    X.rect(12, 3, 1, 4, m);
    X.rect(3, 0, 10, 1, rgb(0xe6ebf2));
  } else if (lk.hat == 5) {  // foulard sur le bas du visage
    if (pose == 1) X.rect(4, 5, 8, 3, rgb(0xa83232));
    else if (pose == 2) X.rect(3, 5, 6, 3, rgb(0xa83232));
  } else if (lk.hat == 3) {
    X.rect(4, 3, 8, 1, rgb(0x3a6fd0));
    if (pose == 2) X.rect(12, 3, 2, 2, rgb(0x3a6fd0));
  }
  if (!weapon) return;
  switch (lk.weapon) {
    case 1:
      X.rect(0, -4, 2, 10, rgb(0xdfe6f2));
      X.rect(-1, 6, 4, 1, rgb(0xffd34d));
      X.rect(0, 7, 2, 3, rgb(0x8a5a32));
      break;
    case 3:
      X.rect(1, -2, 1, 15, rgb(0x6b4a2f));
      X.poly({{2, -4}, {-4, -6}, {-5, 2}, {2, 0}}, rgb(0xc8ccd6));
      break;
    case 4:
      X.rect(1, 7, 1, 4, rgb(0xdfe6f2));
      X.rect(0, 10, 3, 1, rgb(0xffd34d));
      break;
    case 5:  // arc
      X.line(1, -4, -2, 5, 1, rgb(0x8a5a32));
      X.line(-2, 5, 1, 14, 1, rgb(0x8a5a32));
      X.line(1, -4, 1, 14, .5f, rgb(0xf2f0ea));
      break;
    case 6:  // lance
      X.rect(1, -9, 1, 23, rgb(0x6b4a2f));
      X.tri(0, -9, 1.5f, -15, 3, -9, rgb(0xdfe6f2));
      break;
    default: break;
  }
}

void drawFighterSprite(Gfx& g, const Fighter& f, float x, float y, float s, bool faceRight, float t) {
  const Species& sp = f.S();
  if (sp.human) {
    float k = 2 * s;
    float bob = std::round(std::sin(t * 2.5f) * .6f);
    drawHuman(g, look(sp.look), x - 8 * k, y - 10 * k + bob, k, faceRight ? 3 : 2, 0, true);
  } else {
    drawCreature(g, f.sp, x, y, s, faceRight, t);
  }
}

// ---------------------------------------------------------------------------
// Tuiles
// ---------------------------------------------------------------------------
// Petit hasard propre à chaque case (cailloux, herbes…). Signé et positif : « sx + h % 13 »
// doit rester un int (un unsigned ferait d'une position négative un nombre énorme,
// et le dessin sortirait de l'image : plantage sur les Mac à processeur Apple).
static int hsh(int x, int y) { return int((unsigned(x) * 73856093u ^ unsigned(y) * 19349663u) & 0x7fffffffu); }

static void ground(Gfx& g, Theme th, int sx, int sy, int h) {
  if (th == Theme::Vallee) {
    g.rect(sx, sy, 16, 16, rgb(0x67b35d));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x57a04f));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 2, rgb(0x57a04f));
  } else if (th == Theme::Cendres) {
    g.rect(sx, sy, 16, 16, rgb(0x6e625c));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x5f544f));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0x857770));
  } else if (th == Theme::Foret) {
    g.rect(sx, sy, 16, 16, rgb(0x3f7d45));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x34693a));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 2, rgb(0x34693a));
    if (h % 7 == 0) g.rect(sx + (h >> 3) % 12 + 2, sy + (h >> 7) % 12 + 2, 2, 1, rgb(0x8a6a3a));  // feuille morte
  } else if (th == Theme::Neige) {
    g.rect(sx, sy, 16, 16, rgb(0xe8eef6));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0xcdd8e8));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0xffffff));
  } else if (th == Theme::Interieur) {  // parquet : quatre lames par case, joints décalés
    g.rect(sx, sy, 16, 16, rgb(0xb07a48));
    for (int i = 0; i < 4; i++) {
      g.rect(sx, sy + i * 4 + 3, 16, 1, rgb(0x8f5f36));
      g.rect(sx + (h >> (i * 3)) % 14 + 1, sy + i * 4, 1, 3, rgb(0x8f5f36));
    }
    g.rect(sx + h % 12 + 2, sy + 1 + (h >> 5) % 2 * 8, 2, 1, rgb(0xc48c58));
  } else {
    g.rect(sx, sy, 16, 16, rgb(0x4a4252));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x3f3847));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0x5a5164));
  }
}

void drawTile(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float t) {
  char c = m.rows[y][x];
  int h = hsh(x, y);
  Theme th = m.theme;
  auto at = [&](int xx, int yy) { return (xx < 0 || yy < 0 || xx >= m.w() || yy >= m.h()) ? ' ' : m.rows[yy][xx]; };
  switch (c) {
    case '.': ground(g, th, sx, sy, h); break;
    case 'F': {
      ground(g, th, sx, sy, h);
      const uint32_t cols[] = {0xf7e27a, 0xf29bb5, 0xffffff, 0xb59cf0};
      g.rect(sx + 3, sy + 4, 2, 2, rgb(cols[h % 4]));
      g.rect(sx + 10, sy + 9, 2, 2, rgb(cols[(h >> 3) % 4]));
      g.rect(sx + 6, sy + 12, 2, 2, rgb(cols[(h >> 6) % 4]));
      break;
    }
    case ',':
    case 'g': {
      bool dry = c == 'g', dark = th == Theme::Foret;
      g.rect(sx, sy, 16, 16, rgb(dry ? 0x7a6a50 : dark ? 0x2f6236 : 0x4a9446));
      for (int i = 0; i < 5; i++) {
        int bx = sx + 1 + i * 3 + ((h >> i) & 1);
        int sw = (int)std::lround(std::sin(t * 2.2f + x * .7f + i) * .8f);
        g.rect(bx, sy + 6, 2, 9, rgb(dry ? 0x9a7a3a : dark ? 0x1f4a26 : 0x2f6e35));
        g.rect(bx + sw, sy + 3 + ((h >> (i + 3)) & 3), 1, 4, rgb(dry ? 0xe0c070 : dark ? 0x5a9a4a : 0x7cc46a));
      }
      break;
    }
    case 'n': {  // neige profonde (rencontres)
      g.rect(sx, sy, 16, 16, rgb(0xd4e0f0));
      for (int i = 0; i < 4; i++) {
        float bx = sx + 2 + i * 4 + ((h >> i) & 1);
        g.ellipse(bx, sy + 11 - ((h >> (i + 2)) & 3), 3, 3, rgb(0xffffff));
        g.rect(bx - 2, sy + 13, 4, 1, rgb(0xb4c6e0));
      }
      break;
    }
    case 'i': {  // glace
      g.rect(sx, sy, 16, 16, rgb(0xa8d8f0));
      g.rect(sx + 2 + h % 6, sy + 3, 5, 1, rgb(0xe6f6ff));
      g.rect(sx + 8, sy + 9 + (h >> 4) % 4, 4, 1, rgb(0xe6f6ff));
      g.rect(sx + (h >> 6) % 12, sy + 13, 3, 1, rgb(0x8ac0e0));
      break;
    }
    case 'z': {  // marais
      g.rect(sx, sy, 16, 16, rgb(0x4a6a4a));
      float ph = t * 1.2f + x * .9f + y * .5f;
      g.ellipse(sx + 5 + std::sin(ph) * 1.5f, sy + 6, 3, 1.5f, rgb(0x6a8a5a));
      g.ellipse(sx + 11, sy + 12, 3, 1.5f, rgb(0x3a5a3a));
      g.rect(sx + 3 + h % 4, sy + 2, 1, 6, rgb(0x6a8a3a));
      g.rect(sx + 12, sy + 6 + (h >> 3) % 3, 1, 6, rgb(0x6a8a3a));
      g.rect(sx + 12, sy + 5 + (h >> 3) % 3, 1, 2, rgb(0x8a5a32));
      break;
    }
    case 'T':
      ground(g, th, sx, sy, h);
      g.rect(sx + 6, sy + 10, 4, 5, rgb(0x6b4a2f));
      if (th == Theme::Neige) {  // sapin enneigé
        g.tri(sx + 1, sy + 12, sx + 8, sy - 1, sx + 15, sy + 12, rgb(0x2e5a46));
        g.tri(sx + 3, sy + 6, sx + 8, sy - 1, sx + 13, sy + 6, rgb(0xf2f6fb));
        g.rect(sx + 2, sy + 11, 12, 1, rgb(0xf2f6fb));
      } else if (th == Theme::Foret) {  // grand arbre sombre
        g.ellipse(sx + 8, sy + 7, 8, 7, rgb(0x1f4a2a));
        g.ellipse(sx + 5, sy + 4, 4, 3, rgb(0x2e6b3a));
        g.ellipse(sx + 11, sy + 6, 3, 2, rgb(0x2a5e34));
      } else {
        g.ellipse(sx + 8, sy + 7, 7, 6, rgb(0x2e6b3a));
        g.ellipse(sx + 6, sy + 5, 4, 3, rgb(0x3f8a4a));
      }
      break;
    case 'd':
      ground(g, th, sx, sy, h);
      g.rect(sx + 7, sy + 4, 2, 11, rgb(0x4a3a30));
      g.line(sx + 8, sy + 8, sx + 3, sy + 3, 1.5f, rgb(0x4a3a30));
      g.line(sx + 8, sy + 6, sx + 13, sy + 2, 1.5f, rgb(0x4a3a30));
      break;
    case '~':
    case 'B': {
      g.rect(sx, sy, 16, 16, rgb(0x3b7fc4));
      int o = int(t * 4 + x + y) % 10;
      g.rect(sx + o, sy + 4, 4, 1, rgb(0x6aa8e0));
      g.rect(sx + (o + 5) % 12, sy + 11, 4, 1, rgb(0x6aa8e0));
      if (c == 'B') {
        bool horiz = at(x - 1, y) == '=' || at(x + 1, y) == '=' || at(x - 1, y) == 'B' || at(x + 1, y) == 'B';
        g.rect(sx, sy, 16, 16, rgb(0xa0703f));
        for (int i = 0; i < 16; i += 4) {
          if (horiz) g.rect(sx + i, sy, 1, 16, rgb(0x7a5230));
          else g.rect(sx, sy + i, 16, 1, rgb(0x7a5230));
        }
        if (horiz) {
          g.rect(sx, sy, 16, 2, rgb(0x6b4a2f));
          g.rect(sx, sy + 14, 16, 2, rgb(0x6b4a2f));
        } else {
          g.rect(sx, sy, 2, 16, rgb(0x6b4a2f));
          g.rect(sx + 14, sy, 2, 16, rgb(0x6b4a2f));
        }
      }
      break;
    }
    case '=':
      if (th == Theme::Neige) {
        g.rect(sx, sy, 16, 16, rgb(0xc4cedc));
        g.rect(sx + h % 14, sy + (h >> 5) % 14, 2, 1, rgb(0xaab6c8));
        g.rect(sx + (h >> 9) % 14, sy + (h >> 13) % 14, 1, 1, rgb(0xdfe6f0));
      } else if (th == Theme::Vallee || th == Theme::Foret) {
        g.rect(sx, sy, 16, 16, rgb(0xd9c08a));
        g.rect(sx + h % 14, sy + (h >> 5) % 14, 2, 1, rgb(0xc4a873));
        g.rect(sx + (h >> 9) % 14, sy + (h >> 13) % 14, 1, 1, rgb(0xc4a873));
      } else {
        g.rect(sx, sy, 16, 16, rgb(0x8a7f78));
        g.rect(sx, sy + 7, 16, 1, rgb(0x786d66));
        g.rect(sx + 7 + (y % 2) * 4, sy, 1, 7, rgb(0x786d66));
        g.rect(sx + 3 + (y % 2) * 6, sy + 8, 1, 8, rgb(0x786d66));
      }
      break;
    case 'R':
      ground(g, th, sx, sy, h);
      g.ellipse(sx + 8, sy + 10, 6, 5, rgb(0x8a8a92));
      g.ellipse(sx + 7, sy + 8, 4, 3, rgb(0xa9a9b3));
      break;
    case 'W': {
      g.rect(sx, sy, 16, 16, rgb(0xd9c08a));
      int l = at(x - 1, y) == 'W' ? 0 : 2, r = at(x + 1, y) == 'W' ? 16 : 14;
      int tp = at(x, y - 1) == 'W' ? 0 : 2, bt = at(x, y + 1) == 'W' ? 16 : 14;
      g.rect(sx + l, sy + tp, r - l, bt - tp, rgb(0xa8a8b4));
      g.rect(sx + l + (l ? 2 : 0), sy + tp + (tp ? 2 : 0), r - l - (l ? 2 : 0) - (r < 16 ? 2 : 0),
             bt - tp - (tp ? 2 : 0) - (bt < 16 ? 2 : 0), rgb(0x5ea0dc));
      if (l == 0 && tp == 0) {
        g.rect(sx - 1, sy - 6 + std::sin(t * 6) * 1, 2, 7, rgb(0xcfeaff, 200));
        g.rect(sx - 3, sy - 2, 6, 1, rgb(0xcfeaff, 160));
      }
      break;
    }
    case '#':
      if (th == Theme::Interieur) {
        if (at(x, y + 1) == '#' || at(x, y + 1) == ' ') {  // dessus du mur, vu d'en haut
          g.rect(sx, sy, 16, 16, rgb(0x3a2a22));
          if (at(x, y + 1) == '#' && at(x, y + 2) != '#' && at(x, y + 2) != ' ') g.rect(sx, sy + 14, 16, 2, rgb(0x5a4232));
        } else {  // face du mur : papier peint rayé et lambris
          g.rect(sx, sy, 16, 16, rgb(0xe8d8b0));
          for (int i = 2; i < 16; i += 5) g.rect(sx + i, sy, 1, 11, rgb(0xd6c294));
          g.rect(sx, sy + 11, 16, 5, rgb(0x8a5a32));
          g.rect(sx, sy + 11, 16, 1, rgb(0xa8743f));
          if ((x * 7 + y) % 9 == 0) {  // un cadre de temps en temps
            g.rect(sx + 4, sy + 2, 8, 7, rgb(0x7a5230));
            g.rect(sx + 5, sy + 3, 6, 5, rgb(0x7fb0d8));
            g.rect(sx + 5, sy + 6, 6, 2, rgb(0x5f9a52));
          }
        }
      } else if (th == Theme::Neige) {
        g.rect(sx, sy, 16, 16, rgb(0x7fa8c8));
        g.rect(sx, sy + 5, 16, 1, rgb(0x6a92b4));
        g.rect(sx, sy + 11, 16, 1, rgb(0x6a92b4));
        g.rect(sx + (h % 10), sy, 1, 5, rgb(0x9ec4e0));
        g.rect(sx + ((h >> 4) % 12), sy + 6, 1, 5, rgb(0x9ec4e0));
        if (at(x, y - 1) != '#') g.rect(sx, sy, 16, 3, rgb(0xf2f6fb));
      } else if (th == Theme::Grotte) {
        g.rect(sx, sy, 16, 16, rgb(0x241e2e));
        g.ellipse(sx + 4 + h % 8, sy + 4 + (h >> 4) % 8, 4, 3, rgb(0x322a3e));
        g.rect(sx + (h >> 8) % 14, sy + (h >> 12) % 14, 2, 2, rgb(0x1a1522));
      } else {
        g.rect(sx, sy, 16, 16, rgb(0x8a7a66));
        g.rect(sx, sy + 5, 16, 1, rgb(0x6e604f));
        g.rect(sx, sy + 11, 16, 1, rgb(0x6e604f));
        g.rect(sx + (h % 10), sy, 1, 5, rgb(0x6e604f));
        g.rect(sx + ((h >> 4) % 12), sy + 6, 1, 5, rgb(0x6e604f));
        if (at(x, y - 1) != '#') g.rect(sx, sy, 16, 2, rgb(0x5f9a52));
      }
      break;
    case 'a': ground(g, th, sx, sy, h); break;
    case 'l':
    case 'b': {
      g.rect(sx, sy, 16, 16, rgb(0xc8441a));
      float ph = t * 1.5f + (x * 3 + y * 5) * .4f;
      g.ellipse(sx + 4 + std::sin(ph) * 2, sy + 5, 3, 2, rgb(0xffb347));
      g.ellipse(sx + 11, sy + 11 + std::cos(ph) * 2, 3, 2, rgb(0xff8a2a));
      g.rect(sx + (h % 12), sy + (h >> 4) % 14, 3, 1, rgb(0x8a2a14));
      if (c == 'b') {
        g.rect(sx, sy + 1, 16, 14, rgb(0x9a9090));
        g.rect(sx, sy + 1, 16, 1, rgb(0xb8b0aa));
        g.rect(sx, sy + 14, 16, 1, rgb(0x6a6260));
        g.rect(sx + 5, sy + 1, 1, 14, rgb(0x7a7270));
        g.rect(sx + 11, sy + 1, 1, 14, rgb(0x7a7270));
      }
      break;
    }
    case 'm':
      ground(g, th, sx, sy, h);
      g.tri(sx - 1, sy + 16, sx + 8, sy - 1, sx + 17, sy + 16, rgb(0x5a4a44));
      g.tri(sx + 8, sy - 1, sx + 17, sy + 16, sx + 10, sy + 16, rgb(0x4a3c37));
      g.tri(sx + 5, sy + 5, sx + 8, sy - 1, sx + 10, sy + 4, rgb(0x8a7a72));
      break;
    case 'k':
      ground(g, th, sx, sy, h);
      g.tri(sx - 1, sy + 16, sx + 8, sy - 2, sx + 17, sy + 16, rgb(0x5a4a44));
      g.ellipse(sx + 8, sy + 13, 5, 7, rgb(0x120e18));
      g.rect(sx + 3, sy + 13, 10, 3, rgb(0x120e18));
      break;
    case 'c': ground(g, th, sx, sy, h); break;
    case 'x': {
      ground(g, th, sx, sy, h);
      Color glow = rgb(0x7fd6ff, uint8_t(60 + 40 * std::sin(t * 2 + x)));
      g.ellipse(sx + 8, sy + 9, 8, 7, glow);
      g.poly({{sx + 5.f, sy + 15.f}, {sx + 4.f, sy + 6.f}, {sx + 7.f, sy + 1.f}, {sx + 9.f, sy + 7.f}, {sx + 8.f, sy + 15.f}}, rgb(0x7fd6ff));
      g.poly({{sx + 9.f, sy + 15.f}, {sx + 10.f, sy + 7.f}, {sx + 13.f, sy + 4.f}, {sx + 13.f, sy + 15.f}}, rgb(0xa8e6ff));
      break;
    }
    // Meubles des intérieurs
    case 'p': {  // tapis
      ground(g, th, sx, sy, h);
      bool l = at(x - 1, y) == 'p', r = at(x + 1, y) == 'p', u = at(x, y - 1) == 'p', d = at(x, y + 1) == 'p';
      int x0 = l ? 0 : 1, x1 = r ? 16 : 15, y0 = u ? 0 : 1, y1 = d ? 16 : 15;
      g.rect(sx + x0, sy + y0, x1 - x0, y1 - y0, rgb(0xa83a3a));
      if (!l) g.rect(sx + 2, sy + y0, 1, y1 - y0, rgb(0xd9a53a));
      if (!r) g.rect(sx + 13, sy + y0, 1, y1 - y0, rgb(0xd9a53a));
      if (!u) g.rect(sx + x0, sy + 2, x1 - x0, 1, rgb(0xd9a53a));
      if (!d) g.rect(sx + x0, sy + 13, x1 - x0, 1, rgb(0xd9a53a));
      if ((x + y) % 2) g.rect(sx + 7, sy + 7, 2, 2, rgb(0xd9a53a));
      break;
    }
    case 't':  // table
      ground(g, th, sx, sy, h);
      g.rect(sx + 3, sy + 10, 2, 5, rgb(0x6b4a2f));
      g.rect(sx + 11, sy + 10, 2, 5, rgb(0x6b4a2f));
      g.rect(sx + 1, sy + 4, 14, 7, rgb(0x9a6a3a));
      g.rect(sx + 1, sy + 4, 14, 1, rgb(0xb88a52));
      g.rect(sx + 1, sy + 10, 14, 1, rgb(0x6b4a2f));
      if (h % 3 == 0) g.ellipse(sx + 8, sy + 6, 2, 1.5f, rgb(0xefe2c6));  // une tasse
      break;
    case 'h':  // lit
      ground(g, th, sx, sy, h);
      g.rect(sx + 1, sy + 1, 14, 15, rgb(0x7a5230));
      g.rect(sx + 2, sy + 2, 12, 13, rgb(0xf2ece0));
      g.rect(sx + 3, sy + 3, 10, 3, rgb(0xffffff));
      g.rect(sx + 2, sy + 7, 12, 8, rgb(0x5a7ac8));
      g.rect(sx + 2, sy + 7, 12, 1, rgb(0x7a9ae0));
      break;
    case 'e': {  // étagère (contre le mur) : livres et fioles
      g.rect(sx, sy, 16, 16, rgb(0x6b4a2f));
      static const uint32_t cols[] = {0xc84a3a, 0x3a6ab0, 0x5f9a52, 0xd9a53a, 0x8a5ab8};
      for (int r = 0; r < 3; r++) {
        g.rect(sx + 1, sy + 1 + r * 5, 14, 4, rgb(0x3a2a22));
        for (int i = 0; i < 4; i++)
          g.rect(sx + 2 + i * 3, sy + 2 + r * 5 + ((h >> (r * 4 + i)) & 1), 2, 3 - ((h >> (r * 4 + i)) & 1), rgb(cols[(h >> (r * 3 + i)) % 5]));
      }
      break;
    }
    case 'o':  // comptoir
      g.rect(sx, sy, 16, 16, rgb(0x8a5f3a));
      g.rect(sx, sy, 16, 5, rgb(0xb88a52));
      g.rect(sx, sy + 5, 16, 1, rgb(0x6b4a2f));
      g.rect(sx + 4, sy + 8, 8, 6, rgb(0x7a5230));
      break;
    case 'v':  // tonneau
      ground(g, th, sx, sy, h);
      g.ellipse(sx + 8, sy + 14, 6, 2, rgb(0, 50));
      g.rect(sx + 3, sy + 3, 10, 11, rgb(0x8a5a2a));
      g.ellipse(sx + 8, sy + 3, 5, 2, rgb(0xa06a32));
      g.rect(sx + 3, sy + 6, 10, 1, rgb(0x4a4a52));
      g.rect(sx + 3, sy + 11, 10, 1, rgb(0x4a4a52));
      break;
    default: ground(g, th, sx, sy, h);
  }
}

void drawBuilding(Gfx& g, const Building& b, int sx, int sy) {
  int W = b.w * 16, H = b.h * 16;
  int roofH = H * 45 / 100;
  bool stone = b.kind == "forge" || b.kind == "boutique2" || b.kind == "auberge" || b.kind == "soin";
  Color wall = rgb(stone && b.kind != "soin" ? 0xc9b8a6 : 0xefe2c6);
  Color roof = rgb(b.roof);
  g.rect(sx + 2, sy + roofH - 2, W - 4, H - roofH + 1, wall);
  g.rect(sx + 2, sy + H - 2, W - 4, 1, shade(wall, .2f));
  g.poly({{sx - 2.f, sy + roofH + 1.f}, {sx + 4.f, sy + 1.f}, {sx + W - 4.f, sy + 1.f}, {sx + W + 2.f, sy + roofH + 1.f}}, roof);
  g.rect(sx - 2, sy + roofH - 1, W + 4, 3, shade(roof, .3f));
  for (int i = 8; i < W - 4; i += 8) g.rect(sx + i, sy + 3, 1, roofH - 4, shade(roof, .12f));
  int dx = (b.doorX() - b.x) * 16 + 4;
  g.rect(sx + dx, sy + H - 12, 8, 11, rgb(0x6b4a2f));
  g.rect(sx + dx + 1, sy + H - 11, 6, 10, rgb(0x8a5f3a));
  g.rect(sx + dx + 5, sy + H - 6, 1, 1, rgb(0xffd34d));
  Color win = rgb(0x7fc1e8);
  if (dx > 14) g.rect(sx + 6, sy + roofH + 4, 7, 6, win);
  if (dx + 8 < W - 14) g.rect(sx + W - 13, sy + roofH + 4, 7, 6, win);
  // Enseigne
  int ex = sx + W / 2, ey = sy + roofH / 2 + 1;
  if (b.kind == "soin") {
    g.rect(ex - 1, ey - 4, 3, 9, rgb(0xffffff));
    g.rect(ex - 4, ey - 1, 9, 3, rgb(0xffffff));
  } else if (b.kind == "boutique1" || b.kind == "boutique2") {
    g.ellipse(ex, ey + 1, 4, 4, rgb(0xff6a8a));
    g.rect(ex - 1, ey - 5, 3, 3, rgb(0xffffff));
  } else if (b.kind == "forge") {
    g.rect(ex - 5, ey - 2, 10, 3, rgb(0x2a2a30));
    g.rect(ex - 2, ey + 1, 4, 3, rgb(0x2a2a30));
    g.rect(ex - 3, ey + 4, 6, 1, rgb(0x2a2a30));
  } else if (b.kind == "chapelle") {
    g.tri(ex, ey - 6, ex - 4, ey + 2, ex + 4, ey + 2, rgb(0xffe38a));
    g.tri(ex, ey + 5, ex - 4, ey - 3, ex + 4, ey - 3, rgb(0xffe38a));
  } else if (b.kind == "auberge") {
    g.rect(ex - 4, ey - 2, 7, 6, rgb(0xffd34d));
    g.rect(ex + 3, ey - 1, 2, 3, rgb(0xffd34d));
  }
}

void drawChest(Gfx& g, int sx, int sy, bool open) {
  g.ellipse(sx + 8, sy + 14, 7, 2, rgb(0, 50));
  g.rect(sx + 2, sy + 7, 12, 8, rgb(0x8a5a2a));
  g.rect(sx + 2, sy + 10, 12, 1, rgb(0xd9a53a));
  if (open) {
    g.rect(sx + 2, sy + 3, 12, 4, rgb(0x5a3a1a));
    g.rect(sx + 3, sy + 7, 10, 2, rgb(0x2a1a0a));
  } else {
    g.rect(sx + 2, sy + 4, 12, 4, rgb(0xa06a32));
    g.rect(sx + 7, sy + 8, 2, 3, rgb(0xffd34d));
  }
}

void drawSign(Gfx& g, int sx, int sy) {
  g.rect(sx + 7, sy + 8, 2, 7, rgb(0x6b4a2f));
  g.rect(sx + 2, sy + 2, 12, 8, rgb(0xb88a52));
  g.frame(sx + 2, sy + 2, 12, 8, rgb(0x7a5230));
  g.rect(sx + 4, sy + 4, 8, 1, rgb(0x7a5230));
  g.rect(sx + 4, sy + 7, 6, 1, rgb(0x7a5230));
}
