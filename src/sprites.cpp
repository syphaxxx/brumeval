#include "sprites.hpp"

#include <algorithm>
#include <cmath>

static const Color INK = rgb(0x1a1a24);
// Petit hasard stable tiré d'un identifiant (choix d'un sprite pour l'Expédition)
static unsigned idHash(const std::string& s) {
  unsigned h = 2166136261u;
  for (unsigned char c : s) h = (h ^ c) * 16777619u;
  return h;
}
static void eyes(const Xf& x, float x1, float x2, float y, float r = 2) {
  for (float e : {x1, x2}) {
    x.ell(e, y, r, r + .6f, INK);
    x.ell(e - .6f, y - .9f, .7f, .7f, rgb(0xffffff));
  }
}

// ---------------------------------------------------------------------------
// Créatures (regard tourné vers la gauche par défaut)
// ---------------------------------------------------------------------------
// Échelle d'un sprite : un nombre entier de fois (pixels nets) ; seulement pour les petites vignettes, une
// réduction
static float crisp(float k) { return k >= .8f ? std::max(1.f, std::round(k)) : k; }

// Expédition (3.3) : une créature générée n'a pas de sprite à elle ; elle prend celui d'une créature de l'histoire de
// la même forme (choisi d'après son identifiant), teinté de sa couleur. Le monde généré ne change pas (dessin seul).
static const SpriteRect* borrowedSprite(const Species& sp, Color& tint) {
  static const std::vector<std::vector<const char*>> BY_SHAPE = {
      {"braisenard", "givrelin", "flammenard", "givrenard"},              // renard
      {"slime", "slime_b", "glaconnet", "ecumet", "elem_eau"},            // goutte
      {"ronceau", "ronceflore"},                                          // bourgeon
      {"piafouine", "voltigeon", "zephyrin", "orageon", "orageline"},     // oiseau
      {"mulotin", "rongecrin"},                                           // souris
      {"lucioline", "etincelot", "lucifee", "fulgurin", "araignee"},      // insecte
      {"grenouillon", "crapoison"},                                       // grenouille
      {"champichou", "champignard"},                                      // champignon
      {"elem_terre", "golem_pierre", "ferrolin"},                         // rocher
      {"brumelin", "fumerole", "nuageon"},                                // feu follet
      {"troll_grand", "demon", "golem_grand"},                            // boss
      {"tisonnel", "lezard", "dragonnet", "lezard_b"},                    // lézard
      {"golem_pierre", "golem_feu", "troll"},                             // golem
      {"serpentin", "voltanguille", "fulgoray", "serpentox"},             // serpent
      {"chauve_souris", "noctaile"},                                      // chauve-souris
      {"loup", "loup_b", "loup_garou"},                                   // loup
      {"carapierre", "carapic", "coquillon", "bastillon"},                // tortue
      {"fantome", "fantome_b", "nebulis", "noyelin", "squelette"},        // fantôme
      {"cristallin", "prismalin", "orbrume"},                             // cristal
      {"golem_feu_grand"},                                                // magma
  };
  size_t k = (size_t)sp.shape;
  if (k >= BY_SHAPE.size() || BY_SHAPE[k].empty()) return nullptr;
  const auto& v = BY_SHAPE[k];
  tint = mix(rgb(sp.c1), rgb(0xffffff), .55f);
  return findSprite(v[idHash(sp.id) % v.size()]);
}

void drawCreature(Gfx& g, const std::string& id, float x, float y, float s, bool flip, float t, bool chroma) {
  const Species& sp = species(id);
  s *= sp.size;
  Color tint = rgb(0xffffff);
  const SpriteRect* own = sp.sprite.empty() ? borrowedSprite(sp, tint) : nullptr;
  if (!sp.sprite.empty() && !(chroma && (own = findSprite(sp.sprite + "_chroma")))) own = findSprite(sp.sprite);
  if (chroma && own && g.spriteAngle == 0 && std::fmod(t * .7f + x * .013f, 1.f) < .3f) {  // chromatique : des éclats dorés
    float ph = std::fmod(t * .7f + x * .013f, 1.f) / .3f, k = crisp(s);
    for (int i = 0; i < 3; i++) {
      float sx = x + (i - 1) * own->w * k * .35f, sy = y + 16 * s - own->h * k * (.55f + .25f * (i % 2)), r = 1 + 2.5f * std::sin(ph * 3.14159f);
      g.rect(sx - r, sy, 2 * r + 1, 1, rgb(0xffe680)), g.rect(sx, sy - r, 1, 2 * r + 1, rgb(0xffe680));
    }
  }
  if (const SpriteRect* r = own) {
    // Sprite (regard vers la gauche) : centré en x, posé sur l'ombre (y + 15)
    float k = crisp(s);
    float x0 = std::round(x - r->w * k / 2), y0 = std::round(y + 16 * s - r->h * k);
    bool flies = sp.shape == Shape::Bird || sp.shape == Shape::Bat || sp.shape == Shape::Ghost || sp.shape == Shape::Bug ||
                 sp.shape == Shape::Wisp;
    if (flies && g.spriteAngle == 0) {  // en vol : tout le corps flotte, l'ombre rétrécit quand il monte
      float up = std::round((std::sin(t * 3.1f) * .5f + .5f) * 2 * std::min(k, 1.f));
      g.ellipse(x, y + 15 * s, std::max(5.f, r->w * k * (.36f - up * .02f)), 3 * s, rgb(0, 40));
      g.sprite(*r, x0, y0 - 1 - up, flip, k, tint);
      return;
    }
    // Au sol : il respire. Le bas du corps reste posé, le haut (tête et épaules) descend d'un pixel puis remonte,
    // comme les créatures au repos des jeux en pixel art ; décalé d'une créature à l'autre (selon x)
    g.ellipse(x, y + 15 * s, std::max(6.f, r->w * k * .38f), 3 * s, rgb(0, 46));
    bool down = std::sin(t * 2.6f + x * .37f) > .2f;
    int h = r->h, head = std::max(1, h * 9 / 20);
    if (h < 10 || k < 1 || !down || g.spriteAngle != 0) {  // petit, en réduction (carte) ou couché (K.O.) : pas de respiration
      g.sprite(*r, x0, y0, flip, k, tint);
      return;
    }
    g.sprite(r->sub(0, head, r->w, h - head), x0, y0 + head * k, flip, k, tint);
    g.sprite(r->sub(0, 0, r->w, head), x0, y0 + k, flip, k, tint);
    return;
  }
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
// Sprite d'un personnage : (x, y) = coin haut-gauche de sa case de 16x16 agrandie s fois. Sur la carte (s < 1,5) :
// <sprite>_<direction>_<image> (gauche = droite retournée et inversement ; direction absente : de face) ; en plus
// grand (combat, menus) : <sprite>_combat, tourné vers la gauche.
static int breathPx_ = 0;  // combat : le haut du héros descend de ce nombre de pixels (respiration, drawFighterSprite)
static bool humanSprite(Gfx& g, const Look& lk, float x, float y, float s, int dir, int step, bool run) {
  // Expédition (3.3) : un personnage généré (lk_…) prend le dessin d'un personnage de la planche selon son arme
  static const char* BY_WEAPON[] = {"homme", "aventuriere", "mage", "soldat", "aventuriere", "archere", "soldat"};
  std::string borrowed;
  Color tint = rgb(0xffffff);
  if (lk.sprite.empty() && lk.id.rfind("lk_", 0) == 0) {
    borrowed = BY_WEAPON[std::clamp(lk.weapon, 0, 6)];
    if (lk.weapon == 0 && idHash(lk.id) % 2) borrowed = "femme";
    tint = mix(rgb(lk.top), rgb(0xffffff), .5f);
  }
  if (lk.sprite.empty() && borrowed.empty()) return false;
  const std::string& p = lk.sprite.empty() ? borrowed : lk.sprite;
  if (s >= 1.5f) {
    const SpriteRect* r = findSprite(p + "_combat");
    float k = std::max(1.f, std::round(s / 2));
    if (!r) {  // pas de grand sprite : celui de la carte, agrandi
      r = findSprite(p + "_bas_0");
      k = std::max(1.f, std::round(s * .75f));
    }
    if (!r) return false;
    float cx = x + 8 * s, bottom = y + 16 * s;
    g.ellipse(cx, bottom - 1, r->w * k * .36f, 2 * k, rgb(0, 50));
    float x0 = std::round(cx - r->w * k / 2), y0 = std::round(bottom - r->h * k);
    if (breathPx_ > 0 && r->h >= 16) {  // respiration : la tête et les épaules descendent, les pieds restent posés
      int head = r->h * 2 / 5;
      g.sprite(r->sub(0, head, r->w, r->h - head), x0, y0 + head * k, dir == 3, k, tint);
      g.sprite(r->sub(0, 0, r->w, head), x0, y0 + breathPx_, dir == 3, k, tint);
    } else
      g.sprite(*r, x0, y0, dir == 3, k, tint);
    return true;
  }
  static const char* NAMES[] = {"haut", "bas", "gauche", "droite"};
  auto frame = [&](const std::string& d) -> const SpriteRect* {
    // pas de marche : 0 immobile, 1 et 2 en alternance ; 4 images (0 et 2 au repos) ou 2 images
    bool four = findSprite(p + "_" + d + "_3") != nullptr;
    int f = step == 0 ? 0 : step == 1 ? 1 : (four ? 3 : 1);
    const SpriteRect* r = findSprite(p + "_" + d + "_" + std::to_string(f));
    return r ? r : findSprite(p + "_" + d + "_0");
  };
  bool flip = false;
  const SpriteRect* r = nullptr;
  if (run && (dir == 2 || dir == 3) && step) {  // course de profil : <sprite>_course_k (regard vers la gauche)
    r = findSprite(p + "_course_" + std::to_string(SDL_GetTicks() / 70 % 4));  // quatre images, vite
    flip = dir == 3;
  }
  if (!r) r = frame(NAMES[dir & 3]);
  if (!r && (dir == 2 || dir == 3)) {
    r = frame(NAMES[dir == 2 ? 3 : 2]);
    flip = r != nullptr;
  }
  if (!r) r = frame("bas");
  float hop = 0;
  if (!r && (r = findSprite(p))) {  // animal : un seul sprite, tourné vers la gauche ; il trottine en marchant (3.4)
    flip = dir == 3;
    hop = step ? -1.f * s : 0.f;
  }
  if (!r) return false;
  g.ellipse(x + 8 * s, y + 15 * s, 7 * s, 2.5f * s, rgb(0, 34));  // ombre douce (2026-10-21) : un halo, puis le cœur
  g.ellipse(x + 8 * s, y + 15 * s, 5 * s, 1.6f * s, rgb(0, 52));
  g.sprite(*r, std::round(x + (16 * s - r->w * s) / 2), std::round(y + 16 * s - r->h * s + hop), flip, s, tint);
  return true;
}

void drawHuman(Gfx& g, const Look& lk, float x, float y, float s, int dir, int step, bool weapon, bool run) {
  if (humanSprite(g, lk, x, y, s, dir, step, run)) return;
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

bool drawFighterIcon(Gfx& g, const Fighter& f, float cx, float cy, float w, float h, bool faceRight) {
  const Species& sp = f.S();
  Color tint = rgb(0xffffff);
  if (sp.human) {
    const Look& lk = look(sp.look);
    std::string p = lk.sprite;
    if (p.empty() && lk.id.rfind("lk_", 0) == 0) {  // Expédition : comme humanSprite
      static const char* BY_WEAPON[] = {"homme", "aventuriere", "mage", "soldat", "aventuriere", "archere", "soldat"};
      p = BY_WEAPON[std::clamp(lk.weapon, 0, 6)];
      tint = mix(rgb(lk.top), rgb(0xffffff), .5f);
    }
    const SpriteRect* r = p.empty() ? nullptr : findSprite(p + "_bas_0");
    if (!r) return false;
    int hh = std::min(r->h, 15);  // tête et épaules : le haut du sprite de la carte, réduit pour tenir
    float k = std::min(1.f, h / hh);
    g.sprite(r->sub(0, 0, r->w, hh), std::round(cx - r->w * k / 2), std::round(cy - hh * k / 2), faceRight, k, tint);
    return true;
  }
  const SpriteRect* r = sp.sprite.empty() ? borrowedSprite(sp, tint) : nullptr;
  if (!sp.sprite.empty() && !(f.chroma && (r = findSprite(sp.sprite + "_chroma")))) r = findSprite(sp.sprite);  // chromatique
  if (!r) return false;
  float k = std::min({1.f, h / r->h, w / r->w});
  g.sprite(*r, std::round(cx - r->w * k / 2), std::round(cy - r->h * k / 2), faceRight, k, tint);
  return true;
}

bool drawFighterPose(Gfx& g, const Fighter& f, const std::string& pose, float x, float y, bool faceRight) {
  const Species& sp = f.S();
  if (!sp.human) return false;
  const Look& lk = look(sp.look);
  const SpriteRect* r = lk.sprite.empty() ? nullptr : findSprite(lk.sprite + "_" + pose);
  if (!r) return false;
  g.ellipse(x, y + 11, r->w * .36f, 2, rgb(0, 50));
  g.sprite(*r, std::round(x - r->w / 2.f), std::round(y + 12 - r->h), faceRight);  // regard vers la gauche dans l'atlas
  return true;
}

float creatureFit(const std::string& id, float maxH, float maxW) {
  const Species& sp = species(id);
  Color tint;
  const SpriteRect* r = sp.sprite.empty() ? borrowedSprite(sp, tint) : findSprite(sp.sprite);
  if (!r) return 1.2f;  // dessin en code : comme avant
  float h = r->h * sp.size, k = h * 2 <= maxH ? 2.f / sp.size : std::min(1.f, maxH / h) / sp.size;
  if (maxW > 0 && r->w * sp.size * k > maxW) k = maxW / (r->w * sp.size);  // et pas plus large que la place
  return k;
}

float fighterTop(const Fighter& f, float s) {
  const Species& sp = f.S();
  if (sp.human) {
    const Look& lk = look(sp.look);
    if (const SpriteRect* r = lk.sprite.empty() ? nullptr : findSprite(lk.sprite + "_combat")) return r->h * std::max(1.f, std::round(s)) - 12 * s;
    return 22 * s;
  }
  Color tint;
  if (const SpriteRect* r = sp.sprite.empty() ? borrowedSprite(sp, tint) : findSprite(sp.sprite)) return r->h * crisp(s * sp.size) - 16 * s;
  return 18 * s * sp.size;
}

void drawFighterSprite(Gfx& g, const Fighter& f, float x, float y, float s, bool faceRight, float t) {
  const Species& sp = f.S();
  if (sp.human) {
    float k = 2 * s;
    breathPx_ = g.spriteAngle == 0 && std::sin(t * 2.2f + x * .31f) > .3f ? 1 : 0;  // respire (pas couché : K.O.)
    drawHuman(g, look(sp.look), x - 8 * k, y - 10 * k, k, faceRight ? 3 : 2, 0, true);
    breathPx_ = 0;
  } else {
    drawCreature(g, f.sp, x, y, s, faceRight, t, f.chroma);
  }
}

// ---------------------------------------------------------------------------
// Tuiles
// ---------------------------------------------------------------------------
// Petit hasard propre à chaque case (cailloux, herbes…). Signé et positif : « sx + h % 13 »
// doit rester un int (un unsigned ferait d'une position négative un nombre énorme,
// et le dessin sortirait de l'image : plantage sur les Mac à processeur Apple).
static int hsh(int x, int y) { return int((unsigned(x) * 73856093u ^ unsigned(y) * 19349663u) & 0x7fffffffu); }

// Texture de 32x32 de l'atlas (tex_…) : chaque case en prend un quart selon sa position (moins de répétition)
static bool texTile(Gfx& g, const char* id, int x, int y, int sx, int sy, Color tint = {255, 255, 255, 255}) {
  const SpriteRect* r = findSprite(id);
  if (!r) return false;
  if (r->w >= 32) g.sprite(r->sub((x & 1) * 16, (y & 1) * 16, 16, 16), sx, sy, false, 1, tint);  // 32x32 : un quart par case
  else g.sprite(*r, sx, sy, false, 1, tint);
  return true;
}

// Sol des grottes (2026-10-21) : la teinte des dalles suit la roche de la carte (« teinte_murs »), posée par drawTile
static uint32_t caveFloor = 0x8a84a0;
static int roomStyle = 0;
static bool tileNight = false;
static bool murky = false;  // eau de marais (champ « eau » de la carte)
void setTileNight(bool night) { tileNight = night; }  // intérieurs (2026-10-21) : 0 maison, 1 chapelle, 2 auberge, 3 boutique, 4 soin

static void ground(Gfx& g, Theme th, int sx, int sy, int h, int x, int y) {
  if (th == Theme::Vallee) {
    g.rect(sx, sy, 16, 16, rgb(0x67b35d));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x57a04f));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 2, rgb(0x57a04f));
    if (h % 5 == 0) {  // touffe en « v »
      int tx = sx + 3 + (h >> 3) % 9, ty = sy + 4 + (h >> 6) % 8;
      g.rect(tx, ty, 1, 2, rgb(0x4e9446)), g.rect(tx + 2, ty, 1, 2, rgb(0x4e9446)), g.rect(tx + 1, ty + 1, 1, 2, rgb(0x4e9446));
    }
    if (h % 17 == 0) g.rect(sx + 2 + (h >> 5) % 11, sy + 2 + (h >> 9) % 11, 1, 1, rgb((h >> 2) % 2 ? 0xfff2a0 : 0xffffff));  // fleurette
  } else if (th == Theme::Cendres) {
    // (2026-10-21) la roche volcanique n'est plus un aplat : nuance par case, plaques de cendre claire, fissures,
    // gravier ; rarement une braise qui rougeoit encore
    int v = hsh(x / 3, y / 3) % 3 - 1;  // nuance par grandes plaques (une case seule ferait un damier)
    g.rect(sx, sy, 16, 16, rgb(uint32_t((0x6e + v * 2) << 16 | (0x62 + v * 2) << 8 | (0x5c + v * 2))));
    if (h % 7 == 0) g.ellipse(sx + 5 + (h >> 3) % 7, sy + 6 + (h >> 6) % 5, 5, 2.5f, rgb(0x8a7e76, 120));  // cendre claire
    if (h % 5 == 1) {  // une fissure
      int fx = sx + 2 + (h >> 7) % 9, fy = sy + 3 + (h >> 10) % 9;
      g.rect(fx, fy, 3, 1, rgb(0x4a403c)), g.rect(fx + 3, fy + 1, 2, 1, rgb(0x4a403c)), g.rect(fx + 4, fy + 2, 1, 2, rgb(0x4a403c));
    }
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x5f544f));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0x857770));
    g.rect(sx + (h >> 11) % 13 + 1, sy + (h >> 2) % 12 + 2, 1, 1, rgb(0x8f8278));
    if (h % 61 == 0) g.rect(sx + 4 + (h >> 4) % 8, sy + 5 + (h >> 9) % 8, 2, 1, rgb(0xd9541e));  // une braise
  } else if (th == Theme::Foret) {
    g.rect(sx, sy, 16, 16, rgb(0x3f7d45));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x34693a));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 2, rgb(0x34693a));
    if (h % 7 == 0) g.rect(sx + (h >> 3) % 12 + 2, sy + (h >> 7) % 12 + 2, 2, 1, rgb(0x8a6a3a));  // feuille morte
  } else if (th == Theme::Neige) {
    // Neige (3.3) : des rides de vent, des congères avec leur ombre, des brins secs qui dépassent et des cristaux
    g.rect(sx, sy, 16, 16, rgb(0xe8eef6));
    // (2026-10-21) de grandes ondulations de la neige : par endroits un peu bleutée, par endroits plus claire
    int dune = hsh(x / 3, y / 2) % 5;
    if (dune == 0) g.rect(sx, sy, 16, 16, rgb(0xb8cce6, 16));
    else if (dune == 1) g.rect(sx, sy, 16, 16, rgb(0xffffff, 28));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0xcdd8e8));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0xffffff));
    int k = (h >> 5) % 13;
    if (k < 3) {  // rides de vent : deux petites vagues
      int rx = sx + 2 + (h >> 9) % 7, ry = sy + 4 + (h >> 13) % 8;
      g.rect(rx, ry, 4, 1, rgb(0xd2dcea)), g.rect(rx + 4, ry - 1, 3, 1, rgb(0xd2dcea));
      g.rect(rx + 2, ry + 3, 3, 1, rgb(0xd8e1ee)), g.rect(rx + 5, ry + 2, 2, 1, rgb(0xd8e1ee));
    } else if (k == 3) {  // congère : une bosse blanche et son ombre bleutée
      float cx = sx + 5 + (h >> 9) % 6, cy = sy + 8 + (h >> 12) % 4;
      g.ellipse(cx + 1, cy + 2, 5, 2.5f, rgb(0xbccbe0));
      g.ellipse(cx, cy, 5, 2.5f, rgb(0xf8fbff));
    } else if (k == 4) {  // brins secs
      int bx = sx + 4 + (h >> 9) % 8, by = sy + 6 + (h >> 12) % 6;
      g.rect(bx, by, 1, 3, rgb(0x7a7660)), g.rect(bx + 2, by + 1, 1, 2, rgb(0x8a8468)), g.rect(bx - 1, by + 1, 1, 2, rgb(0x6a6650));
      g.rect(bx - 2, by + 3, 6, 1, rgb(0xd0dae8));
    } else if (k == 5) {  // cristal qui brille
      int gx = sx + 3 + (h >> 9) % 10, gy = sy + 3 + (h >> 12) % 10;
      g.rect(gx, gy, 1, 1, rgb(0xffffff)), g.rect(gx - 1, gy, 3, 1, rgb(0xc4e4ff, 170)), g.rect(gx, gy - 1, 1, 3, rgb(0xc4e4ff, 170));
    }
  } else if (th == Theme::Interieur && roomStyle == 1) {  // chapelle : grandes dalles de pierre
    g.rect(sx, sy, 16, 16, rgb((x + y) % 2 ? 0xa8a296 : 0xb2ac9f));
    g.rect(sx, sy + 15, 16, 1, rgb(0x857f74)), g.rect(sx + 15, sy, 1, 16, rgb(0x857f74));
    g.rect(sx, sy, 16, 1, rgb(0xc8c2b4));
    if (h % 7 == 0) g.rect(sx + 3 + (h >> 4) % 8, sy + 5 + (h >> 7) % 6, 3, 1, rgb(0x938d80));  // une fêlure
  } else if (th == Theme::Interieur && (roomStyle == 3 || roomStyle == 4)) {  // boutique, soin : un carrelage en damier
    bool odd = (x + y) % 2;
    uint32_t a = roomStyle == 3 ? 0xc8b48a : 0xe4ecf2, b = roomStyle == 3 ? 0x9a7e56 : 0xa8c4dc;
    g.rect(sx, sy, 16, 16, rgb(odd ? a : b));
    g.rect(sx, sy, 16, 1, rgb(0xffffff, 40)), g.rect(sx, sy, 1, 16, rgb(0xffffff, 30));
    g.rect(sx, sy + 15, 16, 1, rgb(0x000000, 30));
    if (!odd) g.rect(sx + 3, sy + 3, 2, 1, rgb(0xffffff, 60));  // un reflet sur le carreau
  } else if (th == Theme::Interieur) {  // parquet : quatre lames par case, joints décalés ; plus sombre à l'auberge
    g.rect(sx, sy, 16, 16, rgb(roomStyle == 2 ? 0x8e5a34 : 0xb07a48));
    for (int i = 0; i < 4; i++) {
      g.rect(sx, sy + i * 4 + 3, 16, 1, rgb(0x8f5f36));
      g.rect(sx + (h >> (i * 3)) % 14 + 1, sy + i * 4, 1, 3, rgb(0x8f5f36));
    }
    g.rect(sx + h % 12 + 2, sy + 1 + (h >> 5) % 2 * 8, 2, 1, rgb(0xc48c58));
  } else {
    if (texTile(g, (h >> 3) % 3 ? "sol_3_5" : "sol_2_5", x, y, sx, sy, rgb(caveFloor))) return;  // grotte (3.3) : dalles sombres
    g.rect(sx, sy, 16, 16, rgb(0x4a4252));
    g.rect(sx + h % 13, sy + (h >> 4) % 13, 2, 1, rgb(0x3f3847));
    g.rect(sx + (h >> 8) % 12 + 2, sy + (h >> 12) % 12 + 2, 1, 1, rgb(0x5a5164));
  }
}

// Hautes herbes (3.3) : deux touffes de la planche par case, qui ondulent un peu. front : seulement le bas des touffes,
// redessiné par-dessus celui qui marche dedans (ses pieds disparaissent dans l'herbe)
bool tallGrass(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float t, bool front) {
  char c = m.rows[y][x];
  if (c != ',' && c != 'g') return false;
  const char* kind = c == 'g' ? "grande_herbe_seche_" : m.theme == Theme::Foret ? "grande_herbe_sombre_" : "grande_herbe_";
  int h = hsh(x, y);
  for (int i = 0; i < 2; i++) {
    const SpriteRect* r = findSprite(std::string(kind) + std::to_string(1 + ((h >> i) & 1)));
    if (!r) return false;
    // ondulation, plus une rafale qui traverse le champ d'ouest en est (une vague tous les ~4 s)
    float gust = std::pow(std::max(0.f, std::sin(x * .35f + y * .12f - t * 1.6f)), 8.f) * 2.2f;
    int sway = (int)std::lround(std::sin(t * 2.2f + x * .7f + y * .3f + i * 1.7f) * .6f + gust);
    int px = sx - 2 + i * 8 + sway, py = sy + 16 - r->h + i;
    if (!front) g.sprite(*r, px, py, (h >> (i + 4)) & 1);
    else {
      int cut = 6;  // le bas de la touffe
      g.sprite(r->sub(0, r->h - cut, r->w, cut), px, py + r->h - cut, false);
    }
  }
  return true;
}

// Sprite de l'objet haut d'une case (nullptr : aucun, ou dessin en code)
static const SpriteRect* propSprite(const MapDef& m, int x, int y) {
  char c = m.rows[y][x];
  int h = hsh(x, y);
  Theme th = m.theme;
  const char* id = nullptr;
  switch (c) {
    case 'T': {
      int v = (h >> 3) % 20;
      if (th == Theme::Neige) id = v < 5 ? "sapin_neige_petit" : "sapin_neige";
      else if (th == Theme::Foret) id = v < 10 ? "sapin" : v < 16 ? "arbre" : "sapin_petit";
      else if (th == Theme::Cendres) id = v < 12 ? "arbre_automne" : "arbre_automne_petit";
      else if (th == Theme::Vallee) id = v < 12 ? "arbre" : v < 16 ? "arbre_petit" : v == 16 ? "arbre_automne" : v == 17 ? "arbre_rose" : "sapin";
      break;
    }
    case 'R': id = th == Theme::Grotte ? nullptr : (h >> 2) % 3 == 0 ? "rochers" : (h >> 2) % 3 == 1 ? "pierres_moussues" : "rocher"; break;
    case 'v': id = (h >> 2) % 2 ? "tonneau" : "tonneau_2"; break;
    // Meubles des intérieurs (3.3) : une bibliothèque pour deux cases d'étagère, des tables et des chaises
    case 'e': {
      int run = 0;  // place dans la rangée d'étagères
      while (x - run - 1 >= 0 && m.rows[y][x - run - 1] == 'e') run++;
      id = run % 2 == 0 ? "bibliotheque" : nullptr;
      if (!id) return nullptr;
      break;
    }
    case 't': id = "banc"; break;
    case 'j': id = "chaise"; break;
    case 'L': id = th == Theme::Vallee ? "reverbere" : "lampadaire"; break;
    case 'Y': id = "puits"; break;
    case 'C': id = (h >> 2) % 3 == 0 ? "caisse" : (h >> 2) % 3 == 1 ? "caisse_2" : "sac"; break;
    case 'U': {
      static const char* B[] = {"buisson", "buisson_2", "buisson_baies", "buisson_3", "buisson_fleurs", "buisson_4"};
      id = B[(h >> 2) % 6];
      break;
    }
    case 'Q': id = "banc"; break;
    case 'K': id = "charrette"; break;
    case 'M': id = (h >> 2) % 3 ? "etal" : "etal_2"; break;
    case 'X': id = "feu_bois"; break;
    case 'G': id = "tableau_annonces"; break;
    case 'J': id = "banniere"; break;
  }
  if (id && c == 'T' && !m.trees.empty())  // arbres d'une autre sorte (« arbres » de la carte : « pierre »…)
    if (const SpriteRect* r = findSprite(std::string(id) + "_" + m.trees)) return r;
  return id ? findSprite(id) : nullptr;
}
bool hasProp(const MapDef& m, int x, int y) { return propSprite(m, x, y) != nullptr; }
void drawPropIcon(Gfx& g, const MapDef& m, int x, int y, int sx, int sy) {
  const SpriteRect* r = propSprite(m, x, y);
  if (!r) return;
  float k = std::min(1.f, 16.f / std::max(r->w, r->h));
  g.sprite(*r, std::round(sx + 8 - r->w * k / 2), std::round(sy + 16 - r->h * k), false, k);
}
void drawProp(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float t) {
  const SpriteRect* r = propSprite(m, x, y);
  if (!r) return;
  int h = hsh(x, y);
  char c = m.rows[y][x];
  int jx = c == 'T' ? (h >> 7) % 5 - 2 : 0;  // arbres un peu décalés : une forêt moins alignée
  if (c == 'e' && x + 1 < m.w() && m.rows[y][x + 1] == 'e') jx = 8;  // bibliothèque à cheval sur deux étagères
  if (c == 'j') jx = x > 0 && m.rows[y][x - 1] == 't' ? -2 : 2;  // la chaise contre sa table
  if (c != 'C' && c != 'U') g.ellipse(sx + 8 + jx, sy + 14, std::min(8.f, r->w * .4f), 2.5f, rgb(0, 45));
  bool flip = (c == 'T' && (h >> 11) % 2) || (c == 'j' && x > 0 && m.rows[y][x - 1] == 't');  // chaise tournée vers la table
  float x0 = sx + 8 + jx - r->w / 2, y0 = sy + 16 - r->h + (c == 'T' ? 1 : 0);
  if (c == 'T' && r->h >= 16) {  // le vent : le haut de l'arbre penche d'un pixel par moments, en vague sur la carte
    int sway = (int)std::round(std::sin(t * 1.3f + x * .45f + y * .2f) * .65f);
    if (sway != 0) {
      int top = r->h * 9 / 20;
      g.sprite(r->sub(0, top, r->w, r->h - top), x0, y0 + top, flip);
      g.sprite(r->sub(0, 0, r->w, top), x0 + sway, y0, flip);
      return;
    }
  }
  g.sprite(*r, x0, y0, flip);
}

// Fenêtre d'un mur d'intérieur (3.3) : la face d'un mur sur deux ou trois, sauf là où pend un cadre
static bool interiorWindow(int x, int y) { return (x * 7 + y) % 9 != 0 && (x * 5 + y * 3) % 4 == 1; }

// Relief des murs (2026-10-21, graphismes) : dans les grottes, donjons et lieux de glace, un mur n'est plus une dalle
// vue de dessus. Là où il continue vers le bas, on voit son dessus (sombre, ou enneigé dans la glace), bordé d'une
// arête claire du côté du sol ; là où il touche le sol par en dessous, on voit sa face, éclairée en haut et dans
// l'ombre au pied ; le sol au pied d'un mur reçoit son ombre (plus marquée sous une face que sur les côtés).
static void wallDepth(Gfx& g, const MapDef& m, int x, int y, int sx, int sy) {
  auto wall = [&](int xx, int yy) {
    if (xx < 0 || yy < 0 || xx >= m.w() || yy >= m.h()) return true;
    char c = m.rows[yy][xx];
    return c == '#' || c == ' ';
  };
  bool ice = m.theme == Theme::Neige;
  int h = hsh(x, y);
  uint32_t base = ice ? 0 : m.wallTint ? m.wallTint : 0x46405a;  // la roche, en plus sombre (dessus) ou plus clair (arêtes)
  auto shade = [&](float k) {
    int r = std::min(255, int(((base >> 16) & 255) * k)), gg = std::min(255, int(((base >> 8) & 255) * k)), b = std::min(255, int((base & 255) * k));
    return rgb(uint32_t(r << 16 | gg << 8 | b));
  };
  if (wall(x, y)) {
    if (m.rows[y][x] == ' ') return;
    if (!wall(x, y + 1)) {  // la face du mur, tournée vers nous
      Color cap = ice ? rgb(0xe6eef8) : shade(.6f), rim = ice ? rgb(0xffffff) : shade(1.3f);
      g.rect(sx, sy, 16, 4, cap);
      g.rect(sx, sy + 4, 16, 1, rim);
      for (int i = 0; i < 11; i++) g.rect(sx, sy + 5 + i, 16, 1, rgb(0x000000, uint8_t(i * (ice ? 5 : 9))));  // l'ombre monte du pied
      g.rect(sx, sy + 14, 16, 2, rgb(ice ? 0x2a4a6a : 0x0c0910, ice ? 90 : 140));
      if (ice && (h >> 3) % 3 == 0) g.rect(sx + 3 + (h >> 5) % 9, sy + 6, 1, 6 + (h >> 9) % 3, rgb(0xffffff, 120));  // reflet de glace
      if (!ice && (h >> 3) % 4 == 0) {  // un éclat de roche
        Color c = shade(1.9f);
        c.a = 90;
        g.rect(sx + 2 + (h >> 5) % 11, sy + 7 + (h >> 9) % 4, 2, 1, c);
      }
      if (!wall(x - 1, y)) g.rect(sx, sy, 1, 16, rgb(0x000000, 60));
      if (!wall(x + 1, y)) g.rect(sx + 15, sy, 1, 16, rgb(0x000000, 80));
    } else {  // le dessus du mur
      Color top = ice ? rgb(0xdfe9f5) : shade(.48f);
      // dessus du mur : la roche (ou la neige) en texture, assombrie ; sinon un aplat
      if (!texTile(g, ice ? "tex_neige" : "tex_pierre", x, y, sx, sy, ice ? rgb(0xf0f6ff) : shade(.62f))) g.rect(sx, sy, 16, 16, top);
      if (ice) g.rect(sx + (h % 11), sy + 3 + (h >> 4) % 9, 4, 1, rgb(0xc8d8ec));
      else g.rect(sx, sy, 16, 16, rgb(0x000000, 35));  // le dessus est moins éclairé que la face
      Color edge = ice ? rgb(0xa8c4e0) : shade(1.1f);
      if (!wall(x, y - 1)) g.rect(sx, sy, 16, 1, edge);
      if (!wall(x - 1, y)) g.rect(sx, sy, 1, 16, edge);
      if (!wall(x + 1, y)) g.rect(sx + 15, sy, 1, 16, edge);
    }
    return;
  }
  // Le sol au pied des murs : leur ombre
  if (wall(x, y - 1))
    for (int i = 0; i < 6; i++) g.rect(sx, sy + i, 16, 1, rgb(0x000000, uint8_t((6 - i) * (ice ? 9 : 16))));
  if (wall(x - 1, y)) g.rect(sx, sy, 2, 16, rgb(0x000000, ice ? 30 : 55));
  if (wall(x + 1, y)) g.rect(sx + 14, sy, 2, 16, rgb(0x000000, ice ? 20 : 40));
}

static void drawTileBase(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float t);
void drawTile(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float t) {
  caveFloor = 0x8a84a0;
  murky = m.water == "marais";
  roomStyle = m.style == "chapelle" ? 1 : m.style == "auberge" ? 2 : m.style == "boutique" ? 3 : m.style == "soin" ? 4 : 0;
  if (m.wallTint) {  // la roche de la carte, en plus clair pour le sol
    auto ch = [&](int s) { return uint32_t(std::min(255, ((int(m.wallTint) >> s) & 255) * 2 - 4)); };
    caveFloor = ch(16) << 16 | ch(8) << 8 | ch(0);
  }
  drawTileBase(g, m, x, y, sx, sy, t);
  if (m.theme == Theme::Grotte || m.theme == Theme::Neige) return wallDepth(g, m, x, y, sx, sy);
  if (m.theme == Theme::Cendres && m.rows[y][x] != 'l') {  // (2026-10-21) le sol qui borde la lave rougeoit, en pulsant
    auto lava = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < m.w() && yy < m.h() && m.rows[yy][xx] == 'l'; };
    float k = .75f + .25f * std::sin(t * 2.2f + x * .7f + y * .5f);
    for (int i = 0; i < 6; i++) {
      uint8_t a = uint8_t((6 - i) * 13 * k);
      Color c = rgb(0xff7a2a, a);
      if (lava(x, y - 1)) g.rect(sx, sy + i, 16, 1, c);
      if (lava(x, y + 1)) g.rect(sx, sy + 15 - i, 16, 1, c);
      if (lava(x - 1, y)) g.rect(sx + i, sy, 1, 16, c);
      if (lava(x + 1, y)) g.rect(sx + 15 - i, sy, 1, 16, c);
    }
  }
  if (m.theme != Theme::Interieur || m.rows[y][x] == '#' || m.rows[y][x] == ' ') return;
  // Intérieurs (3.3) : l'ombre des murs sur le sol, et la lumière qui tombe d'une fenêtre
  auto wall = [&](int xx, int yy) { return xx < 0 || yy < 0 || xx >= m.w() || yy >= m.h() || m.rows[yy][xx] == '#'; };
  if (wall(x, y - 1)) {
    if (interiorWindow(x, y - 1) && !tileNight)  // le soleil entre par la fenêtre (pas la nuit)
      g.poly({{sx + 4.f, sy + 0.f}, {sx + 12.f, sy + 0.f}, {sx + 15.f, sy + 14.f}, {sx + 3.f, sy + 14.f}}, rgb(0xfff2c0, 34));
    g.rect(sx, sy, 16, 3, rgb(0x1a0e06, 70)), g.rect(sx, sy + 3, 16, 2, rgb(0x1a0e06, 30));
  }
  if (wall(x - 1, y)) g.rect(sx, sy, 3, 16, rgb(0x1a0e06, 55));
  if (wall(x + 1, y)) g.rect(sx + 14, sy, 2, 16, rgb(0x1a0e06, 40));
}

static void drawTileBase(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float t) {
  char c = m.rows[y][x];
  int h = hsh(x, y);
  Theme th = m.theme;
  if (propSprite(m, x, y) || (c == 'e' && x > 0 && m.rows[y][x - 1] == 'e' && findSprite("bibliotheque"))) {
    // l'objet est dessiné avec les personnages (drawProp) : ici le sol seul, pavé s'il est posé sur une place
    auto at2 = [&](int xx, int yy) { return (xx < 0 || yy < 0 || xx >= m.w() || yy >= m.h()) ? ' ' : m.rows[yy][xx]; };
    int paved = (at2(x - 1, y) == 'q') + (at2(x + 1, y) == 'q') + (at2(x, y - 1) == 'q') + (at2(x, y + 1) == 'q');
    if (!(paved >= 2 && g.sprite(th == Theme::Neige ? "sol_1_3" : th == Theme::Cendres ? "sol_0_5" : "sol_0_3", sx, sy))) ground(g, th, sx, sy, h, x, y);
    return;
  }
  auto at = [&](int xx, int yy) { return (xx < 0 || yy < 0 || xx >= m.w() || yy >= m.h()) ? ' ' : m.rows[yy][xx]; };
  switch (c) {
    case '.': {
      ground(g, th, sx, sy, h, x, y);
      // Petits détails du sol (3.3, non bloquants) : touffes et fleurettes dans l'herbe, cailloux sur la cendre
      int d = (h >> 6) % 29;
      const char* deco = nullptr;
      if (th == Theme::Vallee) deco = d < 3 ? "touffe_1" : d < 5 ? "touffe_3" : d == 5 ? "fleurette_1" : d == 6 ? "fleurette_3" : nullptr;
      else if (th == Theme::Foret) deco = d < 3 ? "touffe_2" : d < 5 ? "touffe_4" : d == 5 ? "fleurette_2" : nullptr;
      else if (th == Theme::Cendres) deco = d < 2 ? "caillou" : d == 2 ? "cailloux" : nullptr;
      if (const SpriteRect* r = deco ? findSprite(deco) : nullptr)
        g.sprite(*r, sx + 2 + (h >> 11) % (15 - std::min(13, r->w)), sy + 15 - r->h - (h >> 15) % 4, (h >> 13) % 2);
      break;
    }
    case 'q': {  // pavés (places et rues des villes)
      if (!g.sprite(th == Theme::Neige ? "sol_1_3" : th == Theme::Cendres ? "sol_0_5" : "sol_0_3", sx, sy)) {
        g.rect(sx, sy, 16, 16, rgb(0xa8a49c));
        g.frame(sx, sy, 16, 16, rgb(0x8a867e));
      }
      // (2026-10-25) une bordure de pierres là où la place touche l'herbe ou la terre, et des brins qui la débordent
      for (int s = 0; s < 4; s++) {
        int nx = x + (s == 1) - (s == 3), ny = y + (s == 2) - (s == 0);
        char n = at(nx, ny);
        if (n != '.' && n != ',' && n != 'g' && n != 'F' && n != '=') continue;
        bool horiz = s % 2 == 0;
        int ex = s == 1 ? sx + 14 : sx, ey = s == 2 ? sy + 14 : sy;
        g.rect(ex, ey, horiz ? 16 : 2, horiz ? 2 : 16, rgb(0x8a847a));
        for (int i = 0; i < 4; i++) g.rect(ex + (horiz ? i * 4 + 1 : 0), ey + (horiz ? 0 : i * 4 + 1), horiz ? 2 : 1, horiz ? 1 : 2, rgb(0xc8c2b6));
        if (s != 2) g.rect(ex + (s == 1 ? -1 : horiz ? 0 : 2), ey + (horiz ? 2 : 0), horiz ? 16 : 1, horiz ? 1 : 16, rgb(0x000000, 35));  // l'ombre du rebord
        uint32_t blade = th == Theme::Neige ? 0xeef3fa : th == Theme::Foret ? 0x3f7a35 : th == Theme::Vallee ? 0x5aa83a : 0;
        if (n == '=' || !blade) continue;
        int hh = hsh(nx * 3 + s, ny * 5);
        for (int k = 0; k < 2 + hh % 2; k++) {  // deux ou trois brins qui débordent sur la bordure
          int o = 2 + (hh >> (3 + k * 4)) % 12;
          if (horiz) g.rect(sx + o, ey + (s == 0 ? 0 : 1), 1, 2, rgb(blade)), g.rect(sx + o + 1, ey + (s == 0 ? 0 : 1), 1, 1, rgb(blade));
          else g.rect(ex + (s == 3 ? 0 : 1), sy + o, 2, 1, rgb(blade)), g.rect(ex + (s == 3 ? 0 : 1), sy + o + 1, 1, 1, rgb(blade));
        }
      }
      break;
    }
    case 'H': {  // clôture
      ground(g, th, sx, sy, h, x, y);
      if (const SpriteRect* r = findSprite("cloture")) g.sprite(*r, sx + 8 - r->w / 2, sy + 15 - r->h);
      else {
        g.rect(sx, sy + 6, 16, 2, rgb(0x8a5a32));
        g.rect(sx, sy + 11, 16, 2, rgb(0x8a5a32));
        g.rect(sx + 3, sy + 4, 2, 11, rgb(0x6b4a2f)), g.rect(sx + 11, sy + 4, 2, 11, rgb(0x6b4a2f));
      }
      break;
    }
    case 'F': {
      ground(g, th, sx, sy, h, x, y);
      static const char* FL[] = {"fleurs_1", "fleurs_2", "fleurs_3", "fleurs_4"};
      if (const SpriteRect* a = findSprite(FL[h % 4])) {
        const SpriteRect* b = findSprite(FL[(h >> 4) % 4]);
        g.sprite(*a, sx + 1 + (h >> 8) % 3, sy + 7 - a->h / 2 + (h >> 10) % 3);
        if (b) g.sprite(*b, sx + 8 + (h >> 12) % 3, sy + 13 - b->h);
        break;
      }
      const uint32_t cols[] = {0xf7e27a, 0xf29bb5, 0xffffff, 0xb59cf0};
      g.rect(sx + 3, sy + 4, 2, 2, rgb(cols[h % 4]));
      g.rect(sx + 10, sy + 9, 2, 2, rgb(cols[(h >> 3) % 4]));
      g.rect(sx + 6, sy + 12, 2, 2, rgb(cols[(h >> 6) % 4]));
      break;
    }
    case ',':
    case 'g': {
      bool dry = c == 'g', dark = th == Theme::Foret;
      if (findSprite("grande_herbe_1")) {  // le sol du lieu, puis les touffes
        ground(g, th, sx, sy, h, x, y);
        tallGrass(g, m, x, y, sx, sy, t, false);
        break;
      }
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
      if (!texTile(g, "tex_neige", x, y, sx, sy)) g.rect(sx, sy, 16, 16, rgb(0xd4e0f0));
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
    case 'z': {  // marais ; (2026-10-22) boue et flaques, quenouilles, bords qui se fondent dans l'herbe voisine
      ground(g, th, sx, sy, h, x, y);
      auto bog = [&](int xx, int yy) { char n = at(xx, yy); return n == 'z' || n == '~' || n == ' '; };
      int ix = bog(x - 1, y) ? 0 : 3, iy = bog(x, y - 1) ? 0 : 3, ex = bog(x + 1, y) ? 16 : 13, ey = bog(x, y + 1) ? 16 : 13;
      g.rect(sx + ix, sy + iy, ex - ix, ey - iy, rgb(0x4e6248));                       // la boue
      if (h % 3 != 0) {  // une flaque, pas partout
        g.ellipse(sx + 5 + h % 6, sy + 6 + (h >> 4) % 5, 3 + (h >> 9) % 3, 2, rgb(0x3e5a4a));
        float ph = t * 1.2f + x * .9f + y * .5f;
        g.ellipse(sx + 5 + h % 6 + std::sin(ph), sy + 5.5f + (h >> 4) % 5, 1.5f, .6f, rgb(0x8aa87a, 140));  // son reflet
      }
      if (h % 4 == 1) g.rect(sx + 9 + (h >> 7) % 4, sy + 10, 3, 1, rgb(0x3a4a34));
      int reeds = (h >> 11) % 5 == 0 ? 3 : (h >> 11) % 5 < 3 ? 1 : 0;  // des touffes de quenouilles éparses
      for (int k = 0; k < reeds; k++) {  // quenouilles : une tige qui ondule, un épi brun
        float qx = sx + 2 + (h >> (k * 3 + 2)) % 12, qy = sy + 3 + (h >> (5 + k)) % 5, sw = std::sin(t * 1.5f + x + k) * .6f;
        g.line(qx, qy + 9, qx + sw, qy + 1, 1, rgb(0x6a8a3a));
        g.rect(qx + sw - .5f, qy, 2, 3, rgb(0x7a4a28));
      }
      break;
    }
    case 'T':
      ground(g, th, sx, sy, h, x, y);
      g.ellipse(sx + 9, sy + 14, 7, 2.5f, rgb(0x000000, 40));  // ombre douce au pied
      g.rect(sx + 6, sy + 10, 4, 5, rgb(0x6b4a2f));
      if (th == Theme::Neige) {  // sapin enneigé
        g.tri(sx + 1, sy + 12, sx + 8, sy - 1, sx + 15, sy + 12, rgb(0x2e5a46));
        g.tri(sx + 3, sy + 6, sx + 8, sy - 1, sx + 13, sy + 6, rgb(0xf2f6fb));
        g.rect(sx + 2, sy + 11, 12, 1, rgb(0xf2f6fb));
      } else {
        bool dark = th == Theme::Foret;
        int v = (h >> 4) % 3;  // 0 : rond, 1 : plus petit et clair, 2 : fruitier (vallée) ou large (forêt)
        uint32_t base = dark ? (v == 1 ? 0x24563a : 0x1f4a2a) : (v == 1 ? 0x3a7f42 : 0x2e6b3a);
        float r = v == 1 ? 6.f : dark && v == 2 ? 8.5f : 7.5f, cy = v == 1 ? 8.f : 7.f;
        g.ellipse(sx + 8, cy + sy + 1.5f, r, r - 1.5f, shade(rgb(base), .25f));       // dessous dans l'ombre
        g.ellipse(sx + 8, cy + sy, r, r - 1.5f, rgb(base));
        g.ellipse(sx + 6, cy + sy - 2.5f, r * .55f, r * .4f, rgb(dark ? 0x2f6a3c : 0x45944e));  // reflet en haut à gauche
        g.rect(sx + 5 + (h % 3), int(cy) + sy - 4, 2, 1, rgb(dark ? 0x4a8a4e : 0x6cba64));
        if (v == 2 && !dark)
          for (int i = 0; i < 3; i++) g.rect(sx + 4 + ((h >> (i * 3)) % 8), sy + 5 + ((h >> (i * 2 + 7)) % 5), 2, 2, rgb(0xe0503a));  // fruits
        if (dark && v == 2) g.ellipse(sx + 12, sy + 9, 3, 2, rgb(0x2a5e36));
      }
      break;
    case 'd':
      ground(g, th, sx, sy, h, x, y);
      g.rect(sx + 7, sy + 4, 2, 11, rgb(0x4a3a30));
      g.line(sx + 8, sy + 8, sx + 3, sy + 3, 1.5f, rgb(0x4a3a30));
      g.line(sx + 8, sy + 6, sx + 13, sy + 2, 1.5f, rgb(0x4a3a30));
      break;
    case '~':
    case 'B': {
      g.rect(sx, sy, 16, 16, rgb(murky ? 0x3e6a52 : 0x3576bd));
      if (c == '~') {
        // (2026-10-25) le large : plus sombre loin des berges (par carrés de 4 px), et une houle lente qui le traverse
        auto water = [&](char n) { return n == '~' || n == 'B' || n == ' '; };  // ' ' : hors de la carte
        uint32_t deep = murky ? 0x2c5040 : 0x1f5596, swell = murky ? 0x6a8a62 : 0x6aaee6;
        for (int by = 0; by < 16; by += 4)
          for (int bx = 0; bx < 16; bx += 4) {
            float cx = x * 16 + bx + 2.f, cy = y * 16 + by + 2.f, d2 = 48 * 48;
            for (int yy = y - 3; yy <= y + 3; yy++)
              for (int xx = x - 3; xx <= x + 3; xx++)
                if (!water(at(xx, yy))) {
                  float ex = std::clamp(cx, xx * 16.f, xx * 16 + 16.f) - cx, ey = std::clamp(cy, yy * 16.f, yy * 16 + 16.f) - cy;
                  d2 = std::min(d2, ex * ex + ey * ey);
                }
            float k = std::clamp((std::sqrt(d2) - 10) / 30, 0.f, 1.f);
            if (k > 0) g.rect(sx + bx, sy + by, 4, 4, rgb(deep, uint8_t(255 * k)));
            float s = std::sin(cx * .035f + cy * .06f - t * .8f);
            if (s > .75f) g.rect(sx + bx, sy + by, 4, 4, rgb(swell, uint8_t((s - .75f) * 4 * 50 * k)));
          }
        // Eau (3.3) : plus claire près des berges (peu profonde), ombre de la berge en haut, écume en tirets qui
        // ondule, vaguelettes qui dérivent et reflets qui scintillent
        bool up = !water(at(x, y - 1)), dn = !water(at(x, y + 1)), lf = !water(at(x - 1, y)), rt = !water(at(x + 1, y));
        Color sh = rgb(murky ? 0x7a9a6a : 0x6fb2e6, 120), sh2 = rgb(murky ? 0x7a9a6a : 0x6fb2e6, 60);
        if (up) g.rect(sx, sy, 16, 4, sh), g.rect(sx, sy + 4, 16, 2, sh2);
        if (dn) g.rect(sx, sy + 12, 16, 4, sh), g.rect(sx, sy + 10, 16, 2, sh2);
        if (lf) g.rect(sx, sy, 4, 16, sh), g.rect(sx + 4, sy, 2, 16, sh2);
        if (rt) g.rect(sx + 12, sy, 4, 16, sh), g.rect(sx + 10, sy, 2, 16, sh2);
        // coins : une berge en diagonale seulement
        if (!up && !lf && !water(at(x - 1, y - 1))) g.rect(sx, sy, 4, 4, sh);
        if (!up && !rt && !water(at(x + 1, y - 1))) g.rect(sx + 12, sy, 4, 4, sh);
        if (!dn && !lf && !water(at(x - 1, y + 1))) g.rect(sx, sy + 12, 4, 4, sh);
        if (!dn && !rt && !water(at(x + 1, y + 1))) g.rect(sx + 12, sy + 12, 4, 4, sh);
        if (up) g.rect(sx, sy, 16, 1, rgb(0x1d4a7a, 170));  // la berge du haut fait de l'ombre
        // vaguelettes : deux petits arcs par case, qui dérivent lentement
        for (int k = 0; k < 2; k++) {
          int wy = 4 + k * 6 + int(h >> (4 + k * 3)) % 3;
          int wx = int(t * 3 + (h >> (k * 5)) % 16 + k * 7) % 14;
          if (wx > 11) continue;  // l'arc passe dans la case voisine : il réapparaît de l'autre côté
          uint32_t wv = murky ? 0x8aa87a : 0x8cc4ee;
          g.rect(sx + wx + 1, sy + wy, 3, 1, rgb(wv, 190));
          g.rect(sx + wx, sy + wy + 1, 1, 1, rgb(wv, 120)), g.rect(sx + wx + 4, sy + wy + 1, 1, 1, rgb(wv, 120));
        }
        if (murky && h % 5 == 0) {  // un nénuphar, parfois en fleur
          float lx = sx + 4 + (h >> 4) % 7, ly = sy + 5 + (h >> 8) % 6;
          g.ellipse(lx, ly, 3.5f, 2.2f, rgb(0x4e8a3e));
          g.tri(lx, ly, lx + 3.5f, ly - 1, lx + 3.5f, ly + 1, rgb(0x3e6a52));  // l'encoche
          if (h % 3 == 0) g.ellipse(lx - 1, ly - 1, 1.2f, 1, rgb(0xf0b8d0));
        }
        // reflet qui scintille de temps en temps
        if (std::sin(t * 1.7f + h % 97) > .93f) {
          int gx = sx + 3 + (h >> 7) % 10, gy = sy + 3 + (h >> 11) % 10;
          g.rect(gx, gy, 1, 1, rgb(0xffffff)), g.rect(gx - 1, gy, 3, 1, rgb(0xe8f6ff, 150)), g.rect(gx, gy - 1, 1, 3, rgb(0xe8f6ff, 150));
        }
        // écume en tirets le long des berges, qui avance doucement
        int ph = int(t * 2 + x * 3 + y * 5);
        uint8_t a = uint8_t(160 + 60 * std::sin(t * 2 + x * .8f + y * .6f));
        for (int i = 0; i < 16; i += 2) {
          bool on = (i / 2 + ph) % 4 != 0;
          if (!on) continue;
          if (up) g.rect(sx + i, sy + 1, 2, 1, rgb(0xe4f4ff, a));
          if (dn) g.rect(sx + i, sy + 14, 2, 1, rgb(0xe4f4ff, a));
          if (lf) g.rect(sx + 1, sy + i, 1, 2, rgb(0xe4f4ff, a));
          if (rt) g.rect(sx + 14, sy + i, 1, 2, rgb(0xe4f4ff, a));
        }
      }
      if (c == 'B') {
        bool horiz = at(x - 1, y) == '=' || at(x + 1, y) == '=' || at(x - 1, y) == 'B' || at(x + 1, y) == 'B';
        // (2026-10-21) un vrai pont : son ombre sur l'eau, des planches éclairées et clouées, deux poutres et leurs poteaux
        // (dessiné dans un repère « horizontal » : u le long du pont, v en travers ; tourné pour un pont vertical)
        auto R = [&](float u, float v, float w, float hh, Color c) { horiz ? g.rect(sx + u, sy + v, w, hh, c) : g.rect(sx + v, sy + u, hh, w, c); };
        R(0, 13, 16, 3, rgb(0x0e2a4a, 120));  // l'ombre sur l'eau
        R(0, 2, 16, 11, rgb(0xa0703f));
        for (int i = 0; i < 16; i += 4) {
          R(i, 2, 1, 11, rgb(0x7a5230));
          R(i + 1, 3, 3, 1, rgb(0xc08a52));  // le bord éclairé de la planche
          R(i + 2, 5, 1, 1, rgb(0x4a3420)), R(i + 2, 10, 1, 1, rgb(0x4a3420));  // les clous
        }
        R(0, 1, 16, 2, rgb(0x6b4a2f)), R(0, 12, 16, 2, rgb(0x5a3e26));  // les poutres
        R(1, -1, 2, 4, rgb(0x5a3e26)), R(9, -1, 2, 4, rgb(0x5a3e26));     // les poteaux
        R(1, 11, 2, 4, rgb(0x4a321e)), R(9, 11, 2, 4, rgb(0x4a321e));
        R(1, -1, 2, 1, rgb(0x8a6440)), R(9, -1, 2, 1, rgb(0x8a6440));
      }
      break;
    }
    case '=':
      if (th == Theme::Neige) {  // neige tassée (3.3) : des traces de pas, des bords adoucis contre la neige fraîche
        g.rect(sx, sy, 16, 16, rgb(0xd0d9e6));
        g.rect(sx + h % 14, sy + (h >> 5) % 14, 2, 1, rgb(0xb8c4d6));
        g.rect(sx + (h >> 9) % 14, sy + (h >> 13) % 14, 1, 1, rgb(0xe4eaf3));
        bool horiz = at(x - 1, y) == '=' || at(x + 1, y) == '=';
        for (int i = 0; i < 2; i++) {  // deux empreintes décalées dans le sens du chemin
          int a = 3 + i * 7 + (h >> (3 + i)) % 2, b = i ? 9 : 5;
          if (horiz) g.rect(sx + a, sy + b, 2, 1, rgb(0xb4c0d2)), g.rect(sx + a, sy + b + 1, 2, 1, rgb(0xc4cedd));
          else g.rect(sx + b, sy + a, 1, 2, rgb(0xb4c0d2)), g.rect(sx + b + 1, sy + a, 1, 2, rgb(0xc4cedd));
        }
        auto soft = [&](char n) { return n == '.' || n == ',' || n == 'T' || n == 'F'; };
        bool up = soft(at(x, y - 1)), dn = soft(at(x, y + 1)), lf = soft(at(x - 1, y)), rt = soft(at(x + 1, y));
        Color snow = rgb(0xeaf0f8);
        for (int i = 0; i < 3; i++) {  // la neige fraîche déborde un peu sur le chemin
          int k = 1 + ((h >> (i * 4)) % 13), d = 1 + (h >> (i * 3 + 7)) % 2;
          if (up) g.rect(sx + k, sy, 3, d, snow);
          if (dn) g.rect(sx + k, sy + 16 - d, 3, d, snow);
          if (lf) g.rect(sx, sy + k, d, 3, snow);
          if (rt) g.rect(sx + 16 - d, sy + k, d, 3, snow);
        }
        if (up) g.rect(sx, sy + 1, 16, 1, rgb(0xbac6d8, 140));  // petite ombre sous le bord de neige
      } else if (th == Theme::Vallee || th == Theme::Foret) {
        g.rect(sx, sy, 16, 16, rgb(0xd9c08a));
        g.rect(sx + h % 14, sy + (h >> 5) % 14, 2, 1, rgb(0xc4a873));
        g.rect(sx + (h >> 9) % 14, sy + (h >> 13) % 14, 1, 1, rgb(0xc4a873));
        if (h % 6 == 0) g.rect(sx + 3 + (h >> 4) % 10, sy + 3 + (h >> 8) % 10, 2, 2, rgb(0xb8a07a));  // caillou
        // Bords : seulement contre l'herbe et la nature (pas contre un pont, l'eau ou une autre route)
        auto soft = [&](char n) { return n != '=' && n != 'B' && n != '~' && n != 'W' && n != ' '; };
        Color grass = rgb(th == Theme::Foret ? 0x3f7d45 : 0x67b35d), edge = rgb(0xc0a470);
        bool up = soft(at(x, y - 1)), dn = soft(at(x, y + 1)), lf = soft(at(x - 1, y)), rt = soft(at(x + 1, y));
        if (up) g.rect(sx, sy, 16, 1, edge);
        if (dn) g.rect(sx, sy + 15, 16, 1, edge);
        if (lf) g.rect(sx, sy, 1, 16, edge);
        if (rt) g.rect(sx + 15, sy, 1, 16, edge);
        for (int i = 0; i < 2; i++) {  // touffes qui mordent sur le chemin
          int k = 2 + ((h >> (i * 4)) % 11);
          if (up) g.rect(sx + k, sy, 2, 1 + i, grass);
          if (dn) g.rect(sx + k, sy + 15 - i, 2, 1 + i, grass);
          if (lf) g.rect(sx, sy + k, 1 + i, 2, grass);
          if (rt) g.rect(sx + 15 - i, sy + k, 1 + i, 2, grass);
        }
        auto corner = [&](int cx, int cy) {  // coin arrondi
          g.rect(sx + cx, sy + cy, 2, 1, grass);
          g.rect(sx + cx + (cx ? 1 : 0), sy + cy + (cy ? -1 : 1), 1, 1, grass);
        };
        if (up && lf) corner(0, 0);
        if (up && rt) corner(14, 0);
        if (dn && lf) corner(0, 15);
        if (dn && rt) corner(14, 15);
      } else {
        g.rect(sx, sy, 16, 16, rgb(0x8a7f78));
        g.rect(sx, sy + 7, 16, 1, rgb(0x786d66));
        g.rect(sx + 7 + (y % 2) * 4, sy, 1, 7, rgb(0x786d66));
        g.rect(sx + 3 + (y % 2) * 6, sy + 8, 1, 8, rgb(0x786d66));
      }
      break;
    case 'P':  // plaque de l'énigme (3.0) : une dalle gravée ; (2026-10-21) biseautée, avec un cercle de runes
      ground(g, th, sx, sy, h, x, y);
      g.rect(sx + 1, sy + 2, 14, 13, rgb(0x000000, 50));                                   // son ombre
      g.rect(sx + 1, sy + 1, 14, 13, rgb(0x8a8270));
      g.rect(sx + 1, sy + 1, 14, 1, rgb(0xc8bea4)), g.rect(sx + 1, sy + 1, 1, 13, rgb(0xb8ae94));  // arêtes éclairées
      g.rect(sx + 1, sy + 13, 14, 1, rgb(0x5a5242)), g.rect(sx + 14, sy + 1, 1, 13, rgb(0x6a6252));
      g.rect(sx + 3, sy + 3, 10, 9, rgb(0xa89e88));
      g.ellipse(sx + 8, sy + 7.5f, 4, 3.5f, rgb(0x6a6252));                                 // le cercle gravé
      g.ellipse(sx + 8, sy + 7.5f, 2.6f, 2.2f, rgb(0xa89e88));
      g.rect(sx + 7, sy + 4, 2, 1, rgb(0x6a6252)), g.rect(sx + 7, sy + 11, 2, 1, rgb(0x6a6252));
      g.rect(sx + 4, sy + 7, 1, 2, rgb(0x6a6252)), g.rect(sx + 11, sy + 7, 1, 2, rgb(0x6a6252));
      break;
    case 'R':  // (2026-10-21) un rocher ombré, avec son reflet et une fente
      ground(g, th, sx, sy, h, x, y);
      g.ellipse(sx + 8, sy + 14, 6.5f, 2, rgb(0x000000, 60));
      g.ellipse(sx + 8, sy + 10, 6.5f, 5.5f, rgb(0x5e5a66));
      g.ellipse(sx + 7.5f, sy + 9, 5.5f, 4.5f, rgb(0x8a8692));
      g.ellipse(sx + 6.5f, sy + 7.5f, 3, 2, rgb(0xb0acb8));
      g.rect(sx + 9 + (h >> 4) % 2, sy + 9, 1, 3, rgb(0x46424e));
      break;
    case 'W': {
      bool paved = at(x - 1, y) == 'q' || at(x + 1, y) == 'q' || at(x, y - 1) == 'q' || at(x, y + 1) == 'q';
      if (!(paved && g.sprite("sol_0_3", sx, sy))) g.rect(sx, sy, 16, 16, rgb(0xd9c08a));  // sur une place : les pavés autour
      // la fontaine est dessinée une fois, depuis son coin bas-droit (les autres cases du bloc sont déjà dessinées)
      if (at(x + 1, y) == 'W' || at(x, y + 1) == 'W') break;
      int bw = 16, bh = 16;  // taille du bloc de fontaine (1x1 ou 2x2…)
      while (at(x - bw / 16, y) == 'W') bw += 16;
      while (at(x, y - bh / 16) == 'W') bh += 16;
      float ox0 = sx + 16 - bw, oy0 = sy + 16 - bh;
      float cx = ox0 + bw / 2.f, cy = oy0 + bh / 2.f + 1, rx = bw / 2.f - 1, ry = bh / 2.f - 2;
      // (2026-10-21) un bassin qui a de l'épaisseur, une vasque sur sa colonne, un jet qui retombe en gerbe
      g.ellipse(cx, cy + 3, rx, ry, rgb(0x000000, 50));                  // ombre
      g.ellipse(cx, cy + 2, rx, ry, rgb(0x6e6e7c));                      // la paroi du bassin, vue de face
      g.ellipse(cx, cy, rx, ry, rgb(0x9a9aa8));                          // margelle
      g.ellipse(cx, cy - 1, rx - 1, ry - 1, rgb(0xc8c8d4));              // son dessus, éclairé
      g.ellipse(cx, cy, rx - 3, ry - 3, rgb(0x2f6aa8));                  // l'eau
      g.ellipse(cx - 1, cy - 1, rx - 5, ry - 4.5f, rgb(0x4a8fd0));
      for (int i = 0; i < 2; i++) {                                      // ronds dans l'eau
        float ring = std::fmod(t * .8f + i * .5f, 1.f);
        g.ellipse(cx, cy, 3 + ring * (rx - 5), 1.5f + ring * (ry - 4.5f), rgb(0xcfeaff, uint8_t(110 * (1 - ring))));
      }
      for (int i = 0; i < 3; i++)                                        // reflets qui scintillent
        if (std::fmod(t * 1.7f + i * .37f, 1.f) < .35f)
          g.rect(cx - rx * .5f + i * rx * .45f, cy - ry * .2f + (i % 2) * 2, 1, 1, rgb(0xffffff, 230));
      g.rect(cx - 2, cy - 7, 4, 7, rgb(0x8a8a98));                       // la colonne
      g.rect(cx - 2, cy - 7, 1, 7, rgb(0xb4b4c0));
      g.ellipse(cx, cy - 8, 5, 2, rgb(0x9a9aa8));                        // la vasque
      g.ellipse(cx, cy - 8.5f, 3.5f, 1.2f, rgb(0x6aaee8));
      float jh = 12 + std::sin(t * 5) * 2, top = cy - 8 - jh;
      g.rect(cx - 1, top, 3, jh, rgb(0xd8f0ff, 210));                    // le jet
      g.rect(cx, top, 1, jh, rgb(0xffffff, 230));
      g.ellipse(cx + .5f, top, 3, 2, rgb(0xffffff, 230));
      for (int i = 0; i < 10; i++) {                                     // la gerbe : des gouttes en arc qui retombent
        float k = std::fmod(t * 1.1f + i * .1f, 1.f), dir = (i % 2 ? 1 : -1) * (.45f + (i % 5) * .12f);
        float dx = dir * k * (rx - 3), dy = -k * 5 + k * k * (jh + 13);
        g.rect(cx + dx, top + dy, 2, 1, rgb(0xf0faff, uint8_t(240 * (1 - k * .5f))));
        g.rect(cx + dx, top + dy + 1, 1, 1, rgb(0xbfe0ff, uint8_t(200 * (1 - k * .5f))));
        if (k > .9f) g.ellipse(cx + dx, top + dy + 1, 1.5f, .8f, rgb(0xffffff, 120));  // elle touche l'eau
      }
      break;
    }
    case '#':
      if (th == Theme::Interieur) {
        if (at(x, y + 1) == '#' || at(x, y + 1) == ' ') {  // dessus du mur, vu d'en haut
          g.rect(sx, sy, 16, 16, rgb(0x3a2a22));
          if (at(x, y + 1) == '#' && at(x, y + 2) != '#' && at(x, y + 2) != ' ') g.rect(sx, sy + 14, 16, 2, rgb(0x5a4232));
        } else if (roomStyle == 1) {  // chapelle : pierre claire et vitraux en ogive
          g.rect(sx, sy, 16, 16, rgb(0xd2ccbe));
          g.rect(sx, sy + 5, 16, 1, rgb(0xb8b2a4)), g.rect(sx, sy + 10, 16, 1, rgb(0xb8b2a4));
          g.rect(sx + (y % 2 ? 4 : 11), sy, 1, 5, rgb(0xb8b2a4)), g.rect(sx + (y % 2 ? 11 : 4), sy + 5, 1, 5, rgb(0xb8b2a4));
          g.rect(sx, sy + 13, 16, 3, rgb(0x8a8476));
          if (interiorWindow(x, y)) {  // un vitrail : rouge, bleu, or, dans une ogive de plomb
            g.rect(sx + 4, sy + 3, 8, 10, rgb(0x3a3430));
            g.tri(sx + 4, sy + 3, sx + 8, sy - 1, sx + 12, sy + 3, rgb(0x3a3430));
            g.rect(sx + 5, sy + 3, 3, 4, rgb(0xd04a4a)), g.rect(sx + 8, sy + 3, 3, 4, rgb(0x4a7ad0));
            g.rect(sx + 5, sy + 7, 3, 5, rgb(0xe8c040)), g.rect(sx + 8, sy + 7, 3, 5, rgb(0x5aa85a));
            g.tri(sx + 5, sy + 3, sx + 8, sy + .5f, sx + 11, sy + 3, rgb(0xb070d0));
            g.rect(sx + 5, sy + 4, 1, 1, rgb(0xffffff, 160));
          }
        } else {  // face du mur : papier peint rayé et lambris (couleurs selon le style de la pièce)
          uint32_t paper = roomStyle == 2 ? 0xb8705a : roomStyle == 3 ? 0xb8c89a : roomStyle == 4 ? 0xeef2f6 : 0xe8d8b0;
          uint32_t stripe = roomStyle == 2 ? 0xa45e4a : roomStyle == 3 ? 0xa4b486 : roomStyle == 4 ? 0xc8dcec : 0xd6c294;
          g.rect(sx, sy, 16, 16, rgb(paper));
          for (int i = 2; i < 16; i += 5) g.rect(sx + i, sy, 1, 11, rgb(stripe));
          g.rect(sx, sy + 11, 16, 5, rgb(roomStyle == 4 ? 0x6a9ac8 : 0x8a5a32));
          g.rect(sx, sy + 11, 16, 1, rgb(roomStyle == 4 ? 0x8ab4dc : 0xa8743f));
          if ((x * 7 + y) % 9 == 0) {  // un cadre de temps en temps
            g.rect(sx + 4, sy + 2, 8, 7, rgb(0x7a5230));
            g.rect(sx + 5, sy + 3, 6, 5, rgb(0x7fb0d8));
            g.rect(sx + 5, sy + 6, 6, 2, rgb(0x5f9a52));
          } else if (interiorWindow(x, y)) {  // une fenêtre (3.3) : ciel, croisillon, rebord ; la lumière tombe au sol
            g.rect(sx + 3, sy + 1, 10, 10, rgb(0x6b4a2f));
            if (tileNight) {  // la nuit : un ciel sombre et une étoile
              g.gradV(sx + 4, sy + 2, 8, 8, rgb(0x0e1638), rgb(0x24305e));
              g.rect(sx + 10, sy + 3, 1, 1, rgb(0xffffff));
            } else
              g.gradV(sx + 4, sy + 2, 8, 8, rgb(0xbfe4ff), rgb(0x86c0ea));
            g.rect(sx + 7, sy + 2, 2, 8, rgb(0x6b4a2f)), g.rect(sx + 4, sy + 5, 8, 1, rgb(0x6b4a2f));
            g.rect(sx + 5, sy + 3, 1, 2, rgb(0xffffff, 200));  // reflet
            g.rect(sx + 2, sy + 10, 12, 1, rgb(0xa8743f));      // rebord
          }
        }
      } else if (th == Theme::Neige && texTile(g, "tex_pierre", x, y, sx, sy, rgb(0xb4cdec))) {  // (3.3) pierre glacée
        if (at(x, y - 1) != '#') g.rect(sx, sy, 16, 3, rgb(0xf2f6fb));  // neige sur le haut du mur
      } else if (th == Theme::Grotte && texTile(g, "tex_pierre", x, y, sx, sy, rgb(m.wallTint ? m.wallTint : 0x46405a))) {  // (3.3) roche sombre
        if (at(x, y + 1) != '#' && at(x, y + 1) != ' ') g.rect(sx, sy + 14, 16, 2, rgb(0x16121c, 160));  // pied du mur dans l'ombre
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
      } else if (texTile(g, "tex_pierre", x, y, sx, sy, th == Theme::Cendres ? rgb(0x8a7a70) : rgb(0xc0aa8c))) {  // (3.3) falaise
        if (at(x, y - 1) != '#' && th != Theme::Cendres) g.rect(sx, sy, 16, 2, rgb(0x5f9a52));  // l'herbe du dessus
      } else {
        g.rect(sx, sy, 16, 16, rgb(0x8a7a66));
        g.rect(sx, sy + 5, 16, 1, rgb(0x6e604f));
        g.rect(sx, sy + 11, 16, 1, rgb(0x6e604f));
        g.rect(sx + (h % 10), sy, 1, 5, rgb(0x6e604f));
        g.rect(sx + ((h >> 4) % 12), sy + 6, 1, 5, rgb(0x6e604f));
        if (at(x, y - 1) != '#') g.rect(sx, sy, 16, 2, rgb(0x5f9a52));
      }
      break;
    case 'a': ground(g, th, sx, sy, h, x, y); break;
    case 'l':
    case 'b': {
      if (!texTile(g, "tex_lave", x, y, sx, sy)) g.rect(sx, sy, 16, 16, rgb(0xc8441a));
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
      ground(g, th, sx, sy, h, x, y);
      if (const SpriteRect* a = findSprite((h >> 3) % 2 ? "rochers" : "pierres_moussues")) {  // éboulis de rochers (3.3)
        const SpriteRect* b = findSprite((h >> 5) % 2 ? "rocher" : "rochers");
        Color tint = th == Theme::Neige ? rgb(0xdce6f2) : th == Theme::Cendres ? rgb(0xb8aaa0) : rgb(0xffffff);
        g.sprite(*a, sx + 8 - a->w / 2 + (h >> 7) % 3 - 1, sy + 9 - a->h, (h >> 9) & 1, 1, tint);
        if (b) g.sprite(*b, sx + 8 - b->w / 2 + (h >> 11) % 5 - 2, sy + 16 - b->h, (h >> 13) & 1, 1, tint);
        break;
      }
      g.tri(sx - 1, sy + 16, sx + 8, sy - 1, sx + 17, sy + 16, rgb(0x5a4a44));
      g.tri(sx + 8, sy - 1, sx + 17, sy + 16, sx + 10, sy + 16, rgb(0x4a3c37));
      g.tri(sx + 5, sy + 5, sx + 8, sy - 1, sx + 10, sy + 4, rgb(0x8a7a72));
      break;
    case 'k':  // escalier qui descend (2026-10-21) : un cadre de pierre, des marches qui s'enfoncent dans le noir
      ground(g, th, sx, sy, h, x, y);
      g.rect(sx + 1, sy + 1, 14, 15, rgb(0x5a4a44));
      g.rect(sx + 1, sy + 1, 14, 1, rgb(0x8a7a70)), g.rect(sx + 1, sy + 1, 1, 15, rgb(0x7a6a60));
      g.rect(sx + 3, sy + 3, 10, 13, rgb(0x0c0a10));
      for (int i = 0; i < 4; i++) {  // de plus en plus sombres et étroites
        uint8_t v = uint8_t(150 - i * 32);
        g.rect(sx + 3 + i, sy + 13 - i * 3, 10 - i * 2, 2, rgb(uint32_t(v << 16 | (v - 10) << 8 | (v - 20))));
        g.rect(sx + 3 + i, sy + 13 - i * 3, 10 - i * 2, 1, rgb(uint32_t((v + 30) << 16 | (v + 20) << 8 | (v + 10))));
      }
      break;
    case 'c':
      if (th == Theme::Neige) {  // dalles de glace (3.3, Temple) : joints, reflet en biais, fêlures
        g.rect(sx, sy, 16, 16, rgb((x + y) % 2 ? 0xd8e6f4 : 0xdfeaf6));
        g.rect(sx, sy + 15, 16, 1, rgb(0xb4c8de)), g.rect(sx + 15, sy, 1, 16, rgb(0xb4c8de));
        g.rect(sx, sy, 16, 1, rgb(0xf2f7fc)), g.rect(sx, sy, 1, 16, rgb(0xf2f7fc));
        for (int i = 0; i < 4; i++) g.rect(sx + 3 + i, sy + 9 - i * 2, 1, 2, rgb(0xffffff, 150));  // reflet
        if (h % 5 == 0) {  // fêlure
          int fx = sx + 4 + (h >> 4) % 6, fy = sy + 4 + (h >> 7) % 6;
          g.rect(fx, fy, 2, 1, rgb(0xa8bed6)), g.rect(fx + 2, fy + 1, 2, 1, rgb(0xa8bed6)), g.rect(fx + 3, fy + 2, 1, 2, rgb(0xa8bed6));
        }
      } else ground(g, th, sx, sy, h, x, y);
      break;
    case 'x': {
      ground(g, th, sx, sy, h, x, y);
      Color glow = rgb(0x7fd6ff, uint8_t(60 + 40 * std::sin(t * 2 + x)));
      g.ellipse(sx + 8, sy + 9, 8, 7, glow);
      // (2026-10-21) une grappe de trois cristaux à facettes (côté clair, côté sombre), et un éclat qui passe
      g.ellipse(sx + 8, sy + 15, 6, 1.5f, rgb(0x000000, 60));
      auto shard = [&](float bx, float w, float hgt, float lean) {
        float base = sy + 15.f, top = base - hgt;
        g.poly({{bx, base}, {bx + lean, top}, {bx + w / 2 + lean, top - 2}, {bx + w / 2, base}}, rgb(0xa8e6ff));
        g.poly({{bx + w / 2, base}, {bx + w / 2 + lean, top - 2}, {bx + w + lean, top}, {bx + w, base}}, rgb(0x4aa8e0));
        g.line(bx + lean + .5f, top + 1, bx + .5f, sy + 14, 1, rgb(0xe8f8ff, 200));
      };
      shard(sx + 2, 5, 8, -1);
      shard(sx + 9, 5, 9, 1);
      shard(sx + 5, 6, 13, 0);
      if (std::fmod(t * .8f + x * .37f + y * .21f, 3.f) < .25f) {
        float px0 = sx + 8, py0 = sy + 4;
        g.rect(px0 - 2, py0, 5, 1, rgb(0xffffff, 220)), g.rect(px0, py0 - 2, 1, 5, rgb(0xffffff, 220));
      }
      break;
    }
    // Meubles des intérieurs
    case 'p': {  // tapis
      ground(g, th, sx, sy, h, x, y);
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
      ground(g, th, sx, sy, h, x, y);
      g.rect(sx + 3, sy + 10, 2, 5, rgb(0x6b4a2f));
      g.rect(sx + 11, sy + 10, 2, 5, rgb(0x6b4a2f));
      g.rect(sx + 1, sy + 4, 14, 7, rgb(0x9a6a3a));
      g.rect(sx + 1, sy + 4, 14, 1, rgb(0xb88a52));
      g.rect(sx + 1, sy + 10, 14, 1, rgb(0x6b4a2f));
      if (h % 3 == 0) g.ellipse(sx + 8, sy + 6, 2, 1.5f, rgb(0xefe2c6));  // une tasse
      break;
    case 'h':  // lit
      ground(g, th, sx, sy, h, x, y);
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
      ground(g, th, sx, sy, h, x, y);
      g.ellipse(sx + 8, sy + 14, 6, 2, rgb(0, 50));
      g.rect(sx + 3, sy + 3, 10, 11, rgb(0x8a5a2a));
      g.ellipse(sx + 8, sy + 3, 5, 2, rgb(0xa06a32));
      g.rect(sx + 3, sy + 6, 10, 1, rgb(0x4a4a52));
      g.rect(sx + 3, sy + 11, 10, 1, rgb(0x4a4a52));
      break;
    default: ground(g, th, sx, sy, h, x, y);
  }
}

// Bâtiment dessiné avec un sprite : le genre donne le dessin (maison, taverne…), la largeur la plus grande qui
// tient dans l'emprise (64, 80 ou 96 px : jamais agrandi), et la couleur du toit une variante de la maison.
struct BuildingLook {
  const SpriteRect* r = nullptr;
  float door = .5f;             // position de la porte dans le sprite (fraction de la largeur)
  float chimX = -1, chimY = 0;  // haut de la cheminée (fractions ; -1 : pas de cheminée)
  int x = 0, y = 0;             // coin haut-gauche à l'écran
};
static BuildingLook buildingLook(const Building& b, int sx, int sy) {
  BuildingLook L;
  std::string base;
  if (b.decor()) {  // décor : le sprite tel quel, centré sur l'emprise, posé sur son bas
    if ((L.r = findSprite(b.look))) L.x = sx + b.w * 8 - L.r->w / 2, L.y = sy + b.h * 16 + 2 - L.r->h;
    return L;
  }
  if (!b.look.empty() && b.look != "maison") {  // dessin choisi dans la carte (« dessin »)
    base = b.look;
    if (base == "boutique") L.door = .66f, L.chimX = .72f, L.chimY = .07f;
    if (base == "eglise") L.door = .59f;
    if (base == "entrepot") L.chimX = -1;
  } else if (b.kind == "maison" || b.kind == "soin") {
    base = "maison", L.door = .47f, L.chimX = .8f, L.chimY = .04f;
    if (b.kind == "maison") {  // couleur du toit : rouge, bleu, vert ou ardoise
      Color c = rgb(b.roof);
      int mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b});
      if (mx - mn < 40) base = "maison_ardoise";
      else if (c.b == mx) base = "maison_bleu";
      else if (c.g == mx) base = "maison_vert";
      else if (c.b > c.g + 20) base = "maison_ardoise";  // toit violet
    }
  } else if (b.kind == "boutique1" || b.kind == "boutique2") base = "boutique", L.door = .66f, L.chimX = .72f, L.chimY = .07f;
  else if (b.kind == "auberge") base = "taverne";
  else if (b.kind == "chapelle") base = "eglise", L.door = .59f;
  else if (b.kind == "forge") base = "entrepot", L.chimX = .25f, L.chimY = .1f;
  else return L;
  for (int w : {96, 80, 64})
    if (w <= b.w * 16 || w == 64)
      if ((L.r = findSprite(base + "_" + std::to_string(w)))) break;
  if (!L.r) return L;
  int doorCx = sx + (b.doorX() - b.x) * 16 + 8;
  L.x = doorCx - int(std::lround(L.door * L.r->w));
  L.y = sy + b.h * 16 + 2 - L.r->h;  // posé sur le bas de l'emprise
  return L;
}

void drawBuilding(Gfx& g, const Building& b, int sx, int sy) {
  if (BuildingLook L = buildingLook(b, sx, sy); L.r) {
    g.sprite(*L.r, L.x, L.y);
    // Enseigne pendue à côté de la porte : on reconnaît le lieu d'un coup d'œil
    int ex = L.x + int(L.door * L.r->w) + 9, ey = sy + b.h * 16 - 22;
    auto plaque = [&](Color bg) {
      g.rect(ex, ey - 3, 1, 3, rgb(0x3a2a22));
      g.rect(ex - 4, ey, 9, 9, rgb(0x3a2a22));
      g.rect(ex - 3, ey + 1, 7, 7, bg);
    };
    if (b.decor()) return;
    if (b.kind == "soin") {
      plaque(rgb(0xffffff));
      g.rect(ex - 1, ey + 2, 3, 5, rgb(0xd2493f)), g.rect(ex - 2, ey + 3, 5, 3, rgb(0xd2493f));
    } else if (b.kind == "boutique1" || b.kind == "boutique2") {
      plaque(rgb(0x3a6ab0));
      g.ellipse(ex + .5f, ey + 5, 2, 2, rgb(0xff6a8a));
      g.rect(ex, ey + 2, 1, 1, rgb(0xffffff));
    } else if (b.kind == "forge") {
      plaque(rgb(0xd9a53a));
      g.rect(ex - 2, ey + 3, 5, 2, rgb(0x2a2a30)), g.rect(ex - 1, ey + 5, 3, 2, rgb(0x2a2a30));
    }
    return;
  }
  int W = b.w * 16, H = b.h * 16;
  int roofH = H * 45 / 100;
  bool stone = b.kind == "forge" || b.kind == "boutique2" || b.kind == "auberge" || b.kind == "soin";
  Color wall = rgb(stone && b.kind != "soin" ? 0xc9b8a6 : 0xefe2c6);
  Color roof = rgb(b.roof);
  g.rect(sx + 4, sy + H - 1, W - 2, 3, rgb(0x000000, 45));                 // ombre au sol
  g.rect(sx + W - 2, sy + roofH + 2, 3, H - roofH - 1, rgb(0x000000, 35));  // ombre sur le côté
  g.rect(sx + 2, sy + roofH - 2, W - 4, H - roofH + 1, wall);
  g.rect(sx + 2, sy + roofH + 1, W - 4, 2, shade(wall, .12f));  // ombre sous l'avancée du toit
  if (stone)                                                     // pierres apparentes
    for (int r = 0; r * 6 + roofH + 6 < H - 6; r++)
      for (int i = 6 + (r % 2) * 5; i < W - 6; i += 11) g.rect(sx + i, sy + roofH + 6 + r * 6, 4, 1, shade(wall, .1f));
  g.rect(sx + 2, sy + H - 5, W - 4, 4, rgb(0x9a8f86));  // soubassement
  g.rect(sx + 2, sy + H - 5, W - 4, 1, rgb(0xb8aea4));
  g.rect(sx + 2, sy + H - 2, W - 4, 1, shade(wall, .3f));
  // Cheminée (pas sur la chapelle), derrière le toit
  if (b.kind != "chapelle") {
    int cx = sx + W - 18 - (b.x % 2) * 8;
    g.rect(cx, sy - 5, 6, 10, rgb(0x8a5a4a));
    g.rect(cx - 1, sy - 6, 8, 2, rgb(0x6a4038));
  }
  g.poly({{sx - 2.f, sy + roofH + 1.f}, {sx + 4.f, sy + 1.f}, {sx + W - 4.f, sy + 1.f}, {sx + W + 2.f, sy + roofH + 1.f}}, roof);
  for (int r = 1; r * 4 < roofH - 2; r++) {  // rangées de tuiles, joints décalés
    float k = float(r * 4) / roofH, lx = sx + 4 - 6 * k, rx = sx + W - 4 + 6 * k;
    g.rect(lx, sy + 1 + r * 4, rx - lx, 1, shade(roof, .14f));
    for (float i = lx + 3 + (r % 2) * 4; i < rx - 2; i += 8) g.rect(i, sy + 2 + r * 4, 1, 3, shade(roof, .1f));
  }
  g.rect(sx + 4, sy + 1, W - 8, 2, shade(roof, -.2f));  // faîtage éclairé
  g.rect(sx - 2, sy + roofH - 1, W + 4, 3, shade(roof, .3f));
  int dx = (b.doorX() - b.x) * 16 + 4;
  g.rect(sx + dx - 1, sy + H - 13, 10, 12, rgb(0x5a3e28));  // encadrement
  g.rect(sx + dx, sy + H - 12, 8, 11, rgb(0x6b4a2f));
  g.rect(sx + dx + 1, sy + H - 11, 6, 10, rgb(0x8a5f3a));
  g.rect(sx + dx + 1, sy + H - 7, 6, 1, rgb(0x6b4a2f));
  g.rect(sx + dx + 5, sy + H - 6, 1, 1, rgb(0xffd34d));
  g.rect(sx + dx - 2, sy + H - 1, 12, 2, rgb(0xa89e94));  // marche
  auto window = [&](int wx) {
    int wy = sy + roofH + 5;
    g.rect(wx - 3, wy, 3, 7, shade(roof, .1f));  // volets de la couleur du toit
    g.rect(wx + 8, wy, 3, 7, shade(roof, .1f));
    g.rect(wx - 1, wy - 1, 9, 9, rgb(0xf8f0e0));
    g.rect(wx, wy, 7, 7, rgb(0x7fc1e8));
    g.rect(wx, wy, 7, 2, rgb(0xb8e0f8));  // reflet
    g.rect(wx + 3, wy, 1, 7, rgb(0xf8f0e0));
    g.rect(wx, wy + 3, 7, 1, rgb(0xf8f0e0));
    if (!stone) {  // jardinière fleurie
      g.rect(wx - 1, wy + 8, 9, 2, rgb(0x8a5a32));
      for (int i = 0; i < 3; i++) g.rect(wx + i * 3, wy + 7, 2, 1, rgb(i == 1 ? 0xf29bb5 : 0xffe066));
    }
  };
  if (dx > 18) window(sx + 9);
  if (dx + 8 < W - 18) window(sx + W - 16);
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

void drawBuildingLights(Gfx& g, const Building& b, int sx, int sy, float night) {
  if (night <= 0) return;
  if (b.decor()) return;
  if (BuildingLook L = buildingLook(b, sx, sy); L.r) {  // sprite : façade éclairée et lumière devant la porte
    float dx = L.x + L.door * L.r->w, by = sy + b.h * 16;
    g.ellipse(dx, by - 18, L.r->w * .42f, 14, rgb(0xffc86a, uint8_t(34 * night)));
    g.ellipse(dx, by - 8, 5, 8, rgb(0xffd27a, uint8_t(70 * night)));
    g.ellipse(dx, by + 3, 13, 5, rgb(0xffc86a, uint8_t(55 * night)));
    return;
  }
  int W = b.w * 16, H = b.h * 16, roofH = H * 45 / 100;
  int dx = (b.doorX() - b.x) * 16 + 4;
  auto lit = [&](int wx) {
    int wy = sy + roofH + 5;
    g.ellipse(wx + 3.5f, wy + 3.5f, 10, 8, rgb(0xffc86a, uint8_t(40 * night)));
    g.rect(wx, wy, 7, 7, rgb(0xffd27a, uint8_t(230 * night)));
    g.rect(wx + 3, wy, 1, 7, rgb(0x6a4a2a, uint8_t(200 * night)));
    g.rect(wx, wy + 3, 7, 1, rgb(0x6a4a2a, uint8_t(200 * night)));
  };
  if (dx > 18) lit(sx + 9);
  if (dx + 8 < W - 18) lit(sx + W - 16);
  g.ellipse(sx + dx + 4, sy + H + 2, 12, 5, rgb(0xffc86a, uint8_t(55 * night)));  // lumière devant la porte
}

void drawChimneySmoke(Gfx& g, const Building& b, int sx, int sy, float t) {
  if (b.kind == "chapelle" || b.decor()) return;
  int W = b.w * 16, cx = sx + W - 18 - (b.x % 2) * 8 + 3;
  if (BuildingLook L = buildingLook(b, sx, sy); L.r) {
    if (L.chimX < 0) return;
    cx = L.x + int(L.chimX * L.r->w) - 3;
    sy = L.y + int(L.chimY * L.r->h) + 2;
  }
  for (int i = 0; i < 4; i++) {
    float k = std::fmod(t * .35f + i * .25f + b.x * .13f, 1.f);  // chaque bouffée monte puis s'efface
    float x = cx + std::sin(t * 1.3f + i * 2.f) * 2 + k * 6, y = sy - 7 - k * 22;
    g.ellipse(x, y, 2.5f + k * 4, 2 + k * 3, rgb(0xd8d4d0, uint8_t(120 * (1 - k))));
  }
}

// Ombres portées (2026-10-25) : (kx, ky) = décalage de l'ombre par pixel de hauteur, selon le soleil
void drawPropShadow(Gfx& g, const MapDef& m, int x, int y, int sx, int sy, float kx, float ky, float a) {
  const SpriteRect* r = propSprite(m, x, y);
  char c = m.rows[y][x];
  if (!r || c == 'e' || c == 't' || c == 'j' || r->h < 12) return;  // ni les meubles, ni ce qui est presque à plat
  int h = hsh(x, y), jx = c == 'T' ? (h >> 7) % 5 - 2 : 0;  // le même décalage que drawProp
  float fx = sx + 8 + jx, fy = sy + 14, H = r->h * .7f;   // l'ombre du haut tombe à 70 % de sa hauteur
  float tx = fx + kx * H, ty = fy + ky * H;
  Color col = rgb(0x0a1030, uint8_t(64 * a));
  g.line(fx, fy, tx - (tx > fx ? 1 : -1) * r->w * .3f, ty, std::max(2.f, r->w * .14f), col);  // le tronc, le poteau
  g.ellipse(tx, ty, r->w * .36f, std::max(2.f, r->w * .2f), col);                               // la couronne, le haut
}
void drawBuildingShadow(Gfx& g, const Building& b, int sx, int sy, float kx, float ky, float a) {
  float x0 = sx + 2, x1 = sx + b.w * 16 - 2, by = sy + b.h * 16, H = b.h * 16 * .6f;
  if (BuildingLook L = buildingLook(b, sx, sy); L.r) x0 = L.x + 4, x1 = L.x + L.r->w - 4, by = L.y + L.r->h - 3, H = L.r->h * .6f;
  float dx = std::clamp(kx * H, -22.f, 22.f), dy = ky * H, F = H * .45f;  // F : profondeur du bâtiment au sol (vue de trois quarts)
  Color col = rgb(0x0a1030, uint8_t(64 * a));
  if (dx >= 0) g.poly({{x0, by - F}, {x1, by - F}, {x1 + dx, by - F + dy}, {x1 + dx, by + dy}, {x0 + dx, by + dy}, {x0, by}}, col);
  else g.poly({{x0, by - F}, {x1, by - F}, {x1, by}, {x1 + dx, by + dy}, {x0 + dx, by + dy}, {x0 + dx, by - F + dy}}, col);
}

void drawChest(Gfx& g, int sx, int sy, bool open, float t) {
  g.ellipse(sx + 8, sy + 14, 7, 2, rgb(0, 50));
  if (const SpriteRect* r = findSprite("coffre")) {  // ouvert : assombri, couvercle rabattu
    float x0 = sx + 8 - r->w / 2, y0 = sy + 15 - r->h;
    if (open) {  // (2026-10-21) vraiment ouvert : le couvercle relevé derrière, l'intérieur sombre, la caisse devant
      int lid = r->h * 2 / 5;
      g.sprite(r->sub(0, 0, r->w, lid), x0, y0 - lid + 2, false, 1, rgb(0x9a8a7a));
      g.sprite(r->sub(0, lid, r->w, r->h - lid), x0, y0 + lid, false, 1, rgb(0xd8d0c8));
      g.rect(x0 + 2, y0 + lid, r->w - 4, 2, rgb(0x1a1008));
    } else
      g.sprite(*r, x0, y0);
    float k = t < 0 || open ? 1 : std::fmod(t * .45f + sx * .013f + sy * .007f, 1.f) / .12f;  // un éclat toutes les ~2 s
    if (k < 1) {
      float cx = sx + 8 - r->w / 2.f + 3 + k * (r->w - 6), cy = sy + 15 - r->h + 3, s2 = 2.5f * std::sin(k * 3.14159f);
      g.rect(cx - s2, cy, 2 * s2 + 1, 1, rgb(0xffffff, 230)), g.rect(cx, cy - s2, 1, 2 * s2 + 1, rgb(0xffffff, 230));
    }
    return;
  }
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
  if (const SpriteRect* r = findSprite("poteau")) {
    g.ellipse(sx + 8, sy + 14, 5, 2, rgb(0, 50));
    g.sprite(*r, sx + 8 - r->w / 2, sy + 15 - r->h);
    return;
  }
  g.rect(sx + 7, sy + 8, 2, 7, rgb(0x6b4a2f));
  g.rect(sx + 2, sy + 2, 12, 8, rgb(0xb88a52));
  g.frame(sx + 2, sy + 2, 12, 8, rgb(0x7a5230));
  g.rect(sx + 4, sy + 4, 8, 1, rgb(0x7a5230));
  g.rect(sx + 4, sy + 7, 6, 1, rgb(0x7a5230));
}

void drawMapCritters(Gfx& g, const MapDef& m, int tx0, int tx1, int ty0, int ty1, int camX, int camY, float t, float night) {
  if (m.theme == Theme::Interieur || m.theme == Theme::Grotte) return;
  static const uint32_t WINGS[] = {0xffffff, 0xfff27a, 0x9fd8ff, 0xffb0d8};
  for (int y = std::max(0, ty0); y <= ty1 && y < m.h(); y++)
    for (int x = std::max(0, tx0); x <= tx1 && x < m.w(); x++) {
      char c = m.rows[y][x];
      int h = hsh(x, y);
      float cx = x * 16 - camX + 8.f, cy = y * 16 - camY + 8.f;
      if (c == 'F' && h % 7 == 0 && night < .5f && m.theme != Theme::Neige) {  // un papillon qui butine
        float ph = h % 100 * .1f;
        float bx = cx + std::sin(t * .7f + ph) * 10, by = cy - 6 + std::cos(t * .9f + ph * 1.3f) * 5;
        bool open = std::fmod(t * 7 + ph, 1.f) < .5f;
        Color w = rgb(WINGS[(h >> 3) % 4]);
        if (open) g.rect(std::round(bx) - 2, std::round(by), 2, 2, w), g.rect(std::round(bx) + 1, std::round(by), 2, 2, w);
        else g.rect(std::round(bx) - 1, std::round(by) - 1, 1, 2, w), g.rect(std::round(bx) + 1, std::round(by) - 1, 1, 2, w);
        g.rect(std::round(bx), std::round(by), 1, 2, rgb(0x3a2a1a));
      } else if (c == '~' && h % 29 == 0) {  // un poisson saute, puis des ronds dans l'eau
        float ph = std::fmod(t * .45f + h % 13 * .7f, 5.f);
        if (ph < .6f) {
          float u = ph / .6f;
          float fx = cx - 5 + u * 10, fy = cy - std::sin(u * 3.1416f) * 8;
          g.rect(std::round(fx) - 1, std::round(fy), 3, 1, rgb(0xd8e8f0));
          g.rect(std::round(fx) + (u < .5f ? 2 : -2), std::round(fy) - 1, 1, 1, rgb(0xa8c0d0));
        } else if (ph < 1.4f) {
          float u = (ph - .6f) / .8f;
          g.ellipse(cx + 5, cy + 1, 2 + u * 6, 1 + u * 2, rgb(0xffffff, uint8_t(150 * (1 - u))));
          if (u < .3f) g.rect(cx + 4, cy - 2, 1, 2, rgb(0xffffff, 200)), g.rect(cx + 6, cy - 3, 1, 2, rgb(0xffffff, 200));
        }
      }
    }
}
