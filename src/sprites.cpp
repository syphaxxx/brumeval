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
static unsigned hsh(int x, int y) { return unsigned(x * 73856093) ^ unsigned(y * 19349663); }

static void ground(Gfx& g, Theme th, int sx, int sy, unsigned h) {
  if (th == Theme::Vallee) {
    g.rect(sx, sy, 16, 16, rgb(0x67b35d));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x57a04f));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 2, rgb(0x57a04f));
  } else if (th == Theme::Cendres) {
    g.rect(sx, sy, 16, 16, rgb(0x6e625c));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x5f544f));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0x857770));
  } else {
    g.rect(sx, sy, 16, 16, rgb(0x4a4252));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x3f3847));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0x5a5164));
  }
}

void drawTile(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float t) {
  char c = m.rows[y][x];
  unsigned h = hsh(x, y);
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
      bool dry = c == 'g';
      g.rect(sx, sy, 16, 16, rgb(dry ? 0x7a6a50 : 0x4a9446));
      for (int i = 0; i < 5; i++) {
        int bx = sx + 1 + i * 3 + ((h >> i) & 1);
        int sw = (int)std::lround(std::sin(t * 2.2f + x * .7f + i) * .8f);
        g.rect(bx, sy + 6, 2, 9, rgb(dry ? 0x9a7a3a : 0x2f6e35));
        g.rect(bx + sw, sy + 3 + ((h >> (i + 3)) & 3), 1, 4, rgb(dry ? 0xe0c070 : 0x7cc46a));
      }
      break;
    }
    case 'T':
      ground(g, th, sx, sy, h);
      g.rect(sx + 6, sy + 10, 4, 5, rgb(0x6b4a2f));
      g.ellipse(sx + 8, sy + 7, 7, 6, rgb(0x2e6b3a));
      g.ellipse(sx + 6, sy + 5, 4, 3, rgb(0x3f8a4a));
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
      if (th == Theme::Vallee) {
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
      if (th == Theme::Grotte) {
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
