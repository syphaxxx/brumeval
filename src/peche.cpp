// Pêche (étape « 20 heures », 2026-10-20) : face à l'eau, avec la canne à pêche (objet « canne »), Entrée lance la
// ligne. On attend que ça morde (« ! » : Entrée vite), puis on ferre : un curseur va et vient sur une barre, il faut
// appuyer quand il passe dans la zone verte, plusieurs fois ; trois ratés et la ligne casse. Les poissons
// (data/peche.json : eaux, poids, difficulté) sont des objets ; chaque espèce prise pose « peche:<id> » (carnet,
// condition « poissons_min »).
#include <algorithm>
#include <cmath>
#include <set>

#include "audio.hpp"
#include "data.hpp"
#include "game.hpp"
#include "i18n.hpp"
#include "world.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d);

namespace {
struct Fish {
  std::string id;
  std::vector<std::string> waters;  // thèmes ou identifiants de carte ; « * » : partout
  int weight = 10, level = 1;       // poids du tirage, difficulté 1 à 5
};
std::vector<Fish>& fishTable() {
  static std::vector<Fish> t;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    Json doc = readJson("peche.json");
    Json list = doc.value("poissons", Json::array());
    for (auto& o : list) {
      Fish f;
      f.id = jget<std::string>(o, "objet", "");
      f.waters = jget(o, "eaux", std::vector<std::string>{"*"});
      f.weight = std::max(1, jget(o, "poids", 10));
      f.level = std::clamp(jget(o, "difficulte", 1), 1, 5);
      t.push_back(f);
    }
  }
  return t;
}
}  // namespace

int Game::fishCaught() const {
  int n = 0;
  for (auto& f : fishTable())
    if (has("peche:" + f.id)) n++;
  return n;
}
int fishSpecies() { return (int)fishTable().size(); }

std::vector<std::string> checkFishing() {
  std::vector<std::string> err;
  std::set<std::string> seen;
  for (auto& f : fishTable()) {
    if (!hasItem(f.id)) err.push_back(T("data/peche.json : objet inconnu « ") + f.id + " »");
    if (!seen.insert(f.id).second) err.push_back(T("data/peche.json : poisson en double « ") + f.id + " »");
  }
  if (fishTable().empty()) err.push_back(T("data/peche.json : aucun poisson"));
  return err;
}

void Game::startFishing(int wx, int wy) {
  fishing_ = Fishing{};
  fishing_.on = true;
  fishing_.bx = wx, fishing_.by = wy;
  fishing_.wait = 1.5f + frand() * 3.5f;
  // Le poisson est tiré tout de suite (selon l'eau de la carte), pour que la difficulté soit connue au ferrage
  const MapDef& m = M();
  std::string theme = themeName(m.theme);
  // Les poissons nommés pour cette carte (port, récif…) ; sinon ceux de son thème (vallée, forêt…)
  std::vector<const Fish*> pool;
  int total = 0;
  for (int pass = 0; pass < 2 && pool.empty(); pass++)
    for (auto& f : fishTable())
      for (auto& w : f.waters)
        if (pass == 0 ? w == m.id : (w == "*" || w == theme)) {
          pool.push_back(&f);
          total += f.weight;
          break;
        }
  if (pool.empty()) {
    fishing_.on = false;
    return;
  }
  int r = irand(0, total - 1);
  for (auto* f : pool) {
    if (r < f->weight) {
      fishing_.fish = f->id;
      fishing_.level = f->level;
      break;
    }
    r -= f->weight;
  }
  audio::play("lancer");
}

void Game::endFishing(const std::string& msg) {
  fishing_.on = false;
  if (!msg.empty()) sc.say(msg);
}

void Game::updateFishing(float dt) {
  Fishing& F = fishing_;
  F.t += dt;
  if (in.cancel || in.menu || in.mclick[2]) return endFishing(T("Vous remontez la ligne."));
  bool hit = in.confirm || in.mclick[0];
  switch (F.state) {
    case 0:  // ça ne mord pas encore
      if (hit) return endFishing(T("Trop tôt : le poisson a eu peur."));
      if (F.t >= F.wait) {
        F.state = 1, F.t = 0;
        audio::play("curseur");
      }
      break;
    case 1:  // « ! » : il faut ferrer vite
      if (hit) {
        F.state = 2, F.t = 0;
        F.need = 2 + (F.level + 1) / 2;
        F.zoneW = 40.f - 5 * F.level;
        F.speed = 70.f + 28 * F.level;
        F.zone = 10 + frand() * (120 - F.zoneW - 20);
        F.cursor = 0, F.dir = 1;
        audio::play("valider");
      } else if (F.t > .9f - .08f * F.level)
        return endFishing(T("Le poisson s'est décroché…"));
      break;
    case 2: {  // le combat : le curseur va et vient, Entrée quand il est dans la zone verte
      F.cursor += F.dir * F.speed * dt;
      if (F.cursor > 120) F.cursor = 240 - F.cursor, F.dir = -1;
      if (F.cursor < 0) F.cursor = -F.cursor, F.dir = 1;
      if (!hit) break;
      if (F.cursor >= F.zone && F.cursor <= F.zone + F.zoneW) {
        F.hits++;
        audio::play("coup");
        F.flash = .25f;
        if (F.hits >= F.need) {
          bool fresh = !has("peche:" + F.fish);
          flags.insert("peche:" + F.fish);
          items[F.fish]++;
          sc.call([] { audio::play("capture"); });
          return endFishing(T("Vous attrapez : ") + item(F.fish).name + " !" + (fresh ? T(" (nouveau dans le carnet)") : ""));
        }
        F.zone = 10 + frand() * (120 - F.zoneW - 20);  // le poisson change de direction
        F.speed *= 1.08f;
      } else {
        F.misses++;
        audio::play("rate");
        if (F.misses >= 3) return endFishing(T("La ligne casse ! Le poisson s'échappe."));
      }
      break;
    }
  }
  F.flash = std::max(0.f, F.flash - dt);
}

void Game::drawFishing() {
  const Fishing& F = fishing_;
  // La ligne et le bouchon, sur la case d'eau visée
  float camX = camX_, camY = camY_;
  float px0 = px * 16 - camX + 8, py0 = py * 16 - camY;
  float bx = F.bx * 16 - camX + 8, by = F.by * 16 - camY + 8 + (F.state == 1 ? 2.f : std::sin(time * 3) * 1.f);
  g.line(px0, py0, bx, by - 2, 1, rgb(0xe8e8e8, 200));
  g.ellipse(bx, by + 2, 5 + std::fmod(time * 4, 4.f), 2, rgb(0xffffff, 70));
  g.ellipse(bx, by, 2.5f, 2.5f, rgb(0xd2493f));
  g.rect(bx - 2.5f, by - 1, 5, 1, WHITE);
  if (F.state == 1) {  // ça mord
    g.window(bx - 7, by - 24, 14, 15);
    g.text(bx, by - 22, "!", GOLD, 1);
  }
  if (F.state != 2) {
    g.window(20, 204, 280, 26);
    g.text(160, 210, F.state == 0 ? T("Attendez que ça morde… (Échap : remonter)") : T("Ça mord ! Entrée !"), F.state == 0 ? WHITE : GOLD, 1);
    return;
  }
  g.window(40, 186, 240, 46);
  g.text(48, 191, T("Ferrez : Entrée dans la zone verte"), WHITE);
  float x0 = 100, y0 = 208;
  g.rect(x0 - 1, y0 - 1, 122, 10, rgb(0x0a0f33));
  g.rect(x0, y0, 120, 8, rgb(0x2a3a7a));
  g.rect(x0 + F.zone, y0, F.zoneW, 8, F.flash > 0 ? rgb(0xc8ffc8) : rgb(0x3fbf5f));
  g.rect(x0 + F.cursor - 1, y0 - 3, 3, 14, GOLD);
  for (int i = 0; i < F.need; i++) g.rect(48 + i * 9, 210, 6, 6, i < F.hits ? rgb(0x3fbf5f) : rgb(0x445080));
  for (int i = 0; i < 3; i++) g.rect(232 + i * 9, 210, 6, 6, i < F.misses ? rgb(0xd2493f) : rgb(0x445080));
}
