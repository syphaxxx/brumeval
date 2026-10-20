// Cartes des gardiens (2026-10-20, « 20 heures ») : un jeu de cartes à part, comme le Triple Triad de Final Fantasy
// VIII. Chaque créature obtenue donne sa carte (Bestiaire, plus les cartes gagnées : drapeaux « carte:<espèce> », et
// les cinq cartes de départ de data/cartes_jeu.json) ; quatre valeurs de 1 à 10 tirées de ses statistiques de base
// (haut : Attaque, droite : Vitesse, bas : Défense, gauche : Magie). Plateau de 3 × 3 ; chacun pose à tour de rôle une
// carte de sa main de cinq ; une carte posée retourne chaque carte adverse voisine dont le côté qui la touche est plus
// faible (+1 si son type est efficace contre celui de la voisine). À la fin, les cartes de sa couleur sur le plateau
// et celle restée en main comptent. Adversaires : data/cartes_jeu.json (action « cartes » d'un événement, drapeau
// « cartes:<id> » à la première victoire). Main choisie : `cardHand_`, ligne « main_cartes » de la sauvegarde.
#include <algorithm>
#include <array>
#include <cmath>
#include <set>

#include "audio.hpp"
#include "data.hpp"
#include "game.hpp"
#include "i18n.hpp"
#include "sprites.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8);

namespace {
const Json& cardDoc() {
  static Json d = [] {
    Json j = readJson("cartes_jeu.json");
    return j.is_object() ? j : Json::object();
  }();
  return d;
}
const Json& cardFoes() {  // une référence dans le document (value() en ferait une copie, détruite aussitôt)
  static const Json none = Json::array();
  const Json& d = cardDoc();
  return d.contains("adversaires") && d["adversaires"].is_array() ? d["adversaires"] : none;
}
const Json* cardFoe(const std::string& id) {
  for (auto& o : cardFoes())
    if (jget<std::string>(o, "id", "") == id) return &o;
  return nullptr;
}
// Voisins d'une case : direction (0 haut, 1 droite, 2 bas, 3 gauche) -> case, ou -1
int neighbor(int c, int d) {
  int r = c / 3, k = c % 3;
  if (d == 0) return r > 0 ? c - 3 : -1;
  if (d == 1) return k < 2 ? c + 1 : -1;
  if (d == 2) return r < 2 ? c + 3 : -1;
  return k > 0 ? c - 1 : -1;
}
// Le type de la carte a est-il efficace contre la carte b ? (+1 sur le côté qui attaque)
bool strongAgainst(const std::string& a, const std::string& b) {
  const Species &A = species(a), &B = species(b);
  float e = typeEff(A.type, B.type) * (B.type2 >= 0 ? typeEff(A.type, B.type2) : 1.f);
  return e > 1.01f;
}
std::string valueText(int v) { return v >= 10 ? "A" : std::to_string(v); }
}  // namespace

std::array<int, 4> cardValues(const std::string& sp) {
  const Species& s = species(sp);
  auto v = [](int stat) { return std::clamp((int)std::lround((stat - 20) / 7.0), 1, 10); };
  return {v(s.base[B_ATK]), v(s.base[B_SPD]), v(s.base[B_DEF]), v(s.base[B_MAG])};
}

std::vector<std::string> Game::cardCollection() const {
  std::set<std::string> ids;
  for (auto& id : jget(cardDoc(), "depart", std::vector<std::string>{}))
    if (hasSpecies(id)) ids.insert(id);
  for (auto& id : caught)
    if (hasSpecies(id) && !species(id).human && species(id).shape != Shape::Boss) ids.insert(id);
  for (auto& f : flags)
    if (f.rfind("carte:", 0) == 0 && hasSpecies(f.substr(6))) ids.insert(f.substr(6));
  std::vector<std::string> out(ids.begin(), ids.end());
  auto total = [](const std::string& id) {
    auto v = cardValues(id);
    return v[0] + v[1] + v[2] + v[3];
  };
  std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) { return total(a) > total(b); });
  return out;
}

std::vector<std::string> Game::cardDeck() const {
  auto all = cardCollection();
  std::vector<std::string> deck;
  for (auto& id : cardHand_)
    if (std::find(all.begin(), all.end(), id) != all.end() && std::find(deck.begin(), deck.end(), id) == deck.end()) deck.push_back(id);
  for (auto& id : all)  // complétée par les meilleures cartes
    if (deck.size() < 5 && std::find(deck.begin(), deck.end(), id) == deck.end()) deck.push_back(id);
  if (deck.size() > 5) deck.resize(5);
  return deck;
}

int Game::cardsWon() const {
  int n = 0;
  for (auto& o : cardFoes()) n += has("cartes:" + jget<std::string>(o, "id", ""));
  return n;
}
int cardFoeCount() { return (int)cardFoes().size(); }

// Cartes retournées si `who` pose `sp` en `c` (apply : les retourner vraiment)
int Game::cardFlips(int c, const std::string& sp, int who, bool apply) {
  CardGame& C = cards_;
  auto mine = cardValues(sp);
  int n = 0;
  for (int d = 0; d < 4; d++) {
    int o = neighbor(c, d);
    if (o < 0 || C.cell[o].empty() || C.owner[o] == who) continue;
    int a = mine[d] + (strongAgainst(sp, C.cell[o]) ? 1 : 0), b = cardValues(C.cell[o])[(d + 2) % 4];
    if (a > b) {
      n++;
      if (apply) C.owner[o] = who, C.flipT[o] = .35f;
    }
  }
  return n;
}

// Meilleur coup pour `who` : (carte de la main, case). level 0 : souvent au hasard, 1 : le plus de cartes retournées,
// 2 : et ne pas offrir ses côtés faibles aux cases encore vides
std::pair<int, int> Game::cardChoice(int who, int level) {
  CardGame& C = cards_;
  std::pair<int, int> best{0, -1};
  float bestScore = -1e9f;
  for (int i = 0; i < (int)C.hand[who].size(); i++)
    for (int c = 0; c < 9; c++) {
      if (!C.cell[c].empty()) continue;
      const std::string& sp = C.hand[who][i];
      float s = cardFlips(c, sp, who, false) * 100.f;
      auto v = cardValues(sp);
      for (int d = 0; d < 4; d++) {
        int o = neighbor(c, d);
        if (o >= 0 && C.cell[o].empty()) s += (level >= 2 ? 6.f : 2.f) * v[d];  // côtés exposés : mieux vaut qu'ils soient forts
        else if (o < 0 && level >= 2) s -= 2.f * v[d];                           // un côté fort contre le bord est perdu
      }
      s += frand() * (level == 0 ? 160.f : level == 1 ? 40.f : 6.f);
      if (s > bestScore) bestScore = s, best = {i, c};
    }
  return best;
}

void Game::startCards(const std::string& foe, std::function<void(int)> done) {
  const Json* f = cardFoe(foe);
  auto deck = cardDeck();
  if (!f) return;
  if (deck.size() < 5) {
    sc.say(T("Il vous faut au moins cinq cartes : chaque créature obtenue donne la sienne."));
    return;
  }
  cards_ = CardGame{};
  CardGame& C = cards_;
  C.on = true;
  C.foe = foe;
  C.foeName = tr(jget<std::string>(*f, "nom", foe));
  C.level = std::clamp(jget(*f, "niveau", 1), 0, 2);
  C.hand[0] = deck;
  for (auto& id : jget(*f, "cartes", std::vector<std::string>{}))
    if (hasSpecies(id) && C.hand[1].size() < 5) C.hand[1].push_back(id);
  C.turn = irand(0, 1);
  C.wait = .8f;
  C.done = std::move(done);
  audio::play("valider");
}

void Game::endCards() {
  CardGame& C = cards_;
  C.on = false;
  cardGames_++;
  int mine = (int)C.hand[0].size(), theirs = (int)C.hand[1].size();
  for (int c = 0; c < 9; c++) (C.owner[c] == 0 ? mine : theirs) += !C.cell[c].empty();
  int result = C.abandon ? -1 : mine > theirs ? 1 : mine < theirs ? -1 : 0;
  const Json* f = cardFoe(C.foe);
  if (result > 0 && f) {
    std::string flag = "cartes:" + C.foe;
    if (!has(flag)) {  // première victoire : la récompense de cet adversaire
      flags.insert(flag);
      int g = jget(*f, "or", 0);
      gold += g;
      std::string card = jget<std::string>(*f, "carte", "");
      std::string msg = T("Première victoire contre ") + C.foeName + " !";
      if (g > 0) msg += T(" Vous recevez ") + std::to_string(g) + T(" pièces d'or.");
      sc.say(msg);
      if (!card.empty() && hasSpecies(card) && !has("carte:" + card)) {
        flags.insert("carte:" + card);
        sc.say(T("Nouvelle carte : ") + species(card).name + " !");
      }
    } else {
      gold += 50;
      sc.say(T("Victoire ! Vous recevez 50 pièces d'or."));
    }
  }
  auto done = C.done;
  if (done) done(result);
}

void Game::updateCards(float dt) {
  CardGame& C = cards_;
  C.t += dt;
  for (auto& f : C.flipT) f = std::max(0.f, f - dt);
  int placed = 0;
  for (auto& c : C.cell) placed += !c.empty();
  if (C.over) {  // le score est affiché : Entrée pour finir
    if (C.t > .6f && (in.confirm || in.cancel || in.mclick[0] || pilot_)) endCards();
    return;
  }
  if (placed == 9) {
    C.over = true, C.t = 0;
    int mine = (int)C.hand[0].size();
    for (int c = 0; c < 9; c++) mine += C.owner[c] == 0 && !C.cell[c].empty();
    audio::play(mine > 5 ? "victoire" : mine < 5 ? "rate" : "retour");
    return;
  }
  auto place = [&](int who, int i, int c) {
    std::string sp = C.hand[who][(size_t)i];
    C.cell[c] = sp, C.owner[c] = who, C.flipT[c] = .25f;
    int n = cardFlips(c, sp, who, true);
    C.hand[who].erase(C.hand[who].begin() + i);
    audio::play(n > 0 ? "coup" : "pas");
    C.turn = 1 - who, C.wait = .7f, C.phase = 0;
    C.sel = std::min(C.sel, std::max(0, (int)C.hand[0].size() - 1));
  };
  if (C.turn == 1 || pilot_) {  // l'adversaire (ou le joueur automatique) réfléchit un instant
    if ((C.wait -= dt) > 0) return;
    auto [i, c] = cardChoice(C.turn, C.turn == 1 ? C.level : 2);
    if (c >= 0) place(C.turn, i, c);
    return;
  }
  if ((C.wait -= dt) > 0) return;
  // Souris : une carte de la main, puis une case
  int hover = -1, hoverCell = -1;  // seulement si la souris sert (sinon une vieille position garderait le curseur)
  for (int i = 0; in.mouseOn && i < (int)C.hand[0].size(); i++)
    if (in.mx >= 8 && in.mx < 52 && in.my >= 24 + i * 32 && in.my < 24 + i * 32 + (i + 1 == (int)C.hand[0].size() ? 52 : 32)) hover = i;
  if (in.mouseOn && in.mx >= 88 && in.mx < 232 && in.my >= 26 && in.my < 26 + 3 * 56) hoverCell = (in.my - 26) / 56 * 3 + (in.mx - 88) / 48;
  if (C.phase == 0) {
    if (in.press[UP]) C.sel = (C.sel + (int)C.hand[0].size() - 1) % (int)C.hand[0].size(), audio::play("curseur");
    if (in.press[DOWN]) C.sel = (C.sel + 1) % (int)C.hand[0].size(), audio::play("curseur");
    if (in.mclick[0] && hover >= 0) C.sel = hover, in.confirm = true;
    if (in.confirm) {
      C.phase = 1;
      if (!C.cell[C.cursor].empty())
        for (int c = 0; c < 9; c++)
          if (C.cell[c].empty()) {
            C.cursor = c;
            break;
          }
      audio::play("valider");
    } else if (in.cancel || in.menu) {
      C.abandon = true, C.over = true, C.t = 1;  // abandonner : c'est perdu
      audio::play("retour");
    }
    return;
  }
  int r = C.cursor / 3, k = C.cursor % 3;
  if (in.press[UP]) r = (r + 2) % 3;
  if (in.press[DOWN]) r = (r + 1) % 3;
  if (in.press[LEFT]) k = (k + 2) % 3;
  if (in.press[RIGHT]) k = (k + 1) % 3;
  if (r * 3 + k != C.cursor) C.cursor = r * 3 + k, audio::play("curseur");
  if (hoverCell >= 0 && in.moved && in.mouseOn) C.cursor = hoverCell;  // la souris ne compte que si elle a bougé
  if (in.mclick[0] && hover >= 0) {
    C.sel = hover;
    return;
  }
  if (in.mclick[0] && hoverCell >= 0) C.cursor = hoverCell;  // un clic pose là où il est, même sans avoir bougé
  if ((in.confirm || (in.mclick[0] && hoverCell >= 0)) && C.cell[C.cursor].empty()) place(0, C.sel, C.cursor);
  else if (in.cancel || in.mclick[2]) C.phase = 0, audio::play("retour");
}

// Une carte : cadre de la couleur du joueur (bleu) ou de l'adversaire (rouge), la créature, ses quatre valeurs
void Game::drawCard(float x, float y, float w, float h, const std::string& sp, int who, bool lit, float flip) {
  float sx = flip > 0 ? std::abs(std::cos(flip / .35f * 3.14159f)) : 1.f;  // se retourne : elle s'amincit puis revient
  float cw = std::max(2.f, w * sx), cx = x + (w - cw) / 2;
  g.rect(cx - 1, y - 1, cw + 2, h + 2, lit ? GOLD : rgb(0x0a0f33));
  g.gradV(cx, y, cw, h, who == 0 ? rgb(0x3a62c8) : rgb(0xc83a4a), who == 0 ? rgb(0x1a2a6a) : rgb(0x6a1a2a));
  if (sx < .5f) return;
  const Species& s = species(sp);
  if (const SpriteRect* r = s.sprite.empty() ? nullptr : findSprite(s.sprite)) {
    float k = std::min({1.f, (w - 8) / r->w, (h - 18) / r->h});
    g.sprite(*r, std::round(x + (w - r->w * k) / 2), std::round(y + h - 5 - r->h * k), false, k);
  } else
    drawCreature(g, sp, x + w / 2, y + h - 22, .8f, false, 0);
  g.rect(x + 2, y + h - 3, w - 4, 2, rgb(types()[(size_t)s.type].color));  // le type, en couleur
  auto v = cardValues(sp);
  g.rect(x + 1, y + 1, 21, 23, rgb(0x0a0f22, 140));  // fond sombre sous les valeurs
  g.text(x + 10, y + 1, valueText(v[0]), WHITE, 1);
  g.text(x + 17, y + 8, valueText(v[1]), WHITE, 1);
  g.text(x + 10, y + 15, valueText(v[2]), WHITE, 1);
  g.text(x + 3, y + 8, valueText(v[3]), WHITE, 1);
}

void Game::drawCards() {
  const CardGame& C = cards_;
  g.rect(g.left(), 0, (float)g.fullW, 240, rgb(0x0a0f22, 242));
  g.newLayer();
  g.window(88, 4, 144, 18);
  g.text(160, 8, C.foeName, GOLD, 1);
  // Le plateau
  for (int c = 0; c < 9; c++) {
    float x = 88 + (c % 3) * 48, y = 26 + (c / 3) * 56;
    g.rect(x, y, 47, 55, rgb(0x1c2448));
    g.frame(x, y, 47, 55, rgb(0x3a4a8a));
    if (!C.cell[c].empty()) drawCard(x + 2, y + 2, 43, 51, C.cell[c], C.owner[c], false, C.flipT[c]);
    if (C.phase == 1 && C.turn == 0 && !C.over && c == C.cursor) g.frame(x + 1, y + 1, 45, 53, C.cell[c].empty() ? GOLD : rgb(0xd2493f));
  }
  // Les mains : la sienne à gauche, celle de l'adversaire à droite (cartes visibles)
  for (int who = 0; who < 2; who++)
    for (int i = 0; i < (int)C.hand[who].size(); i++) {
      bool lit = who == 0 && C.turn == 0 && !C.over && i == C.sel;
      float x = who == 0 ? 8.f + (lit ? 6 : 0) : 268.f, y = 24.f + i * 32;
      drawCard(x, y, 44, 52, C.hand[who][(size_t)i], who, lit, 0);
    }
  int mine = (int)C.hand[0].size(), theirs = (int)C.hand[1].size();
  for (int c = 0; c < 9; c++)
    if (!C.cell[c].empty()) (C.owner[c] == 0 ? mine : theirs)++;
  g.window(20, 206, 280, 30);
  g.text(30, 210, std::to_string(mine), rgb(0x8ab0ff));
  g.text(290, 210, std::to_string(theirs), rgb(0xff8a9a), 2);
  std::string status, hint;
  if (C.over) {
    status = C.abandon ? T("Partie abandonnée.") : mine > theirs ? T("Gagné !") : mine < theirs ? T("Perdu…") : T("Égalité.");
    hint = T("Entrée : continuer");
  } else if (C.turn == 1)
    status = C.foeName + T(" réfléchit…");
  else if (C.phase == 0)
    status = T("Choisissez une carte"), hint = T("Échap : abandonner");
  else
    status = T("Choisissez une case"), hint = T("Échap : changer de carte");
  g.text(160, 210, status, C.over ? GOLD : WHITE, 1);
  if (!hint.empty()) g.text(160, 222, hint, MUTED, 1);
}

// Le joueur de cartes : jouer, choisir sa main, lire les règles
void Game::cardsMenu(const std::string& foe, std::function<void(int)> done) {
  Menu m;
  const Json* f = cardFoe(foe);
  m.title = f ? tr(jget<std::string>(*f, "nom", foe)) : foe;
  m.x = 8, m.y = 8, m.w = 200;
  m.items.push_back({T("Jouer"), has("cartes:" + foe) ? "★" : "", T("Une partie de Cartes des gardiens, avec votre main de cinq cartes."), true, [this, foe, done] {
                       menus.clear();
                       startCards(foe, done);
                     }});
  m.items.push_back({T("Choisir ma main"), std::to_string(cardCollection().size()) + T(" cartes"), T("Les cinq cartes que vous jouez. Chaque créature obtenue donne sa carte."), true,
                     [this] { cardHandMenu(); }});
  m.items.push_back({T("Règles"), "", T("Comment on joue."), true, [this] {
                       menus.clear();
                       sc.say(T("Chacun pose à son tour une carte de sa main sur le plateau de trois cases sur trois."));
                       sc.say(T("Chaque carte a quatre valeurs (haut, droite, bas, gauche ; A vaut 10), tirées des statistiques de sa créature."));
                       sc.say(T("Une carte posée retourne chaque carte adverse voisine dont le côté qui la touche est plus faible. Un type efficace contre la voisine donne +1."));
                       sc.say(T("À la fin, on compte les cartes de sa couleur sur le plateau, plus celle restée en main. Le plus grand nombre gagne."));
                     }});
  m.items.push_back({T("Partir"), "", "", true, [this] { menus.pop(); }});
  m.onCancel = m.items.back().act;
  menus.push(m);
}

void Game::cardHandMenu(int sel) {
  auto all = cardCollection();
  std::vector<std::string> deck;  // la main choisie (une main incomplète est complétée par les meilleures cartes)
  for (auto& id : cardHand_.empty() ? cardDeck() : cardHand_)
    if (std::find(all.begin(), all.end(), id) != all.end() && std::find(deck.begin(), deck.end(), id) == deck.end() && deck.size() < 5) deck.push_back(id);
  cardHand_ = deck;
  Menu m;
  m.title = T("Ma main : ") + std::to_string(deck.size()) + "/5";
  m.x = 8, m.y = 8, m.w = 230, m.rows = 10;
  m.sel = sel;
  for (size_t i = 0; i < all.size(); i++) {
    const std::string id = all[i];
    bool in = std::find(deck.begin(), deck.end(), id) != deck.end();
    auto v = cardValues(id);
    std::string vals = valueText(v[0]) + " " + valueText(v[1]) + " " + valueText(v[2]) + " " + valueText(v[3]);
    MenuItem it{species(id).name, (in ? "★ " : "") + vals,
                T("Haut, droite, bas, gauche. Entrée : la mettre dans la main ou l'en retirer. Type : ") + typesName(species(id)) + ".", true,
                [this, id, i] {
                  auto d = cardHand_;
                  auto at = std::find(d.begin(), d.end(), id);
                  if (at != d.end()) d.erase(at);
                  else if (d.size() >= 5) d.back() = id;  // la main est pleine : la dernière carte laisse sa place
                  else d.push_back(id);
                  cardHand_ = d;
                  menus.pop();
                  cardHandMenu((int)i);
                }};
    if (in) it.color = 0xffe066;
    it.shrink = true;
    m.items.push_back(it);
  }
  m.onCancel = [this] { menus.pop(); };
  menus.push(m);
}

std::vector<std::string> checkCardsData() {
  std::vector<std::string> err;
  std::set<std::string> ids;
  for (auto& id : jget(cardDoc(), "depart", std::vector<std::string>{}))
    if (!hasSpecies(id)) err.push_back(T("data/cartes_jeu.json : carte de départ inconnue « ") + id + " »");
  if (jget(cardDoc(), "depart", std::vector<std::string>{}).size() < 5) err.push_back(T("data/cartes_jeu.json : il faut cinq cartes de départ"));
  for (auto& o : cardFoes()) {
    std::string id = jget<std::string>(o, "id", "");
    if (id.empty() || !ids.insert(id).second) err.push_back(T("data/cartes_jeu.json : adversaire sans identifiant ou en double « ") + id + " »");
    auto deck = jget(o, "cartes", std::vector<std::string>{});
    if (deck.size() != 5) err.push_back(T("data/cartes_jeu.json : ") + id + T(" doit avoir cinq cartes"));
    for (auto& c : deck)
      if (!hasSpecies(c)) err.push_back(T("data/cartes_jeu.json : ") + id + T(", carte inconnue « ") + c + " »");
    std::string r = jget<std::string>(o, "carte", "");
    if (!r.empty() && !hasSpecies(r)) err.push_back(T("data/cartes_jeu.json : ") + id + T(", carte en récompense inconnue « ") + r + " »");
  }
  return err;
}
bool cardFoeExists(const std::string& id) { return cardFoe(id) != nullptr; }
