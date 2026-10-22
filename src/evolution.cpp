// Évolution des créatures (2.2) : au niveau prévu (especes.json, « evolution »), une créature peut
// devenir une espèce plus forte. Après chaque combat gagné, le jeu demande « Évoluer ? » ; un Non
// est gardé (Fighter::evolveLater) et se rattrape par Échap > Évolution. Méthodes de Game.
#include <algorithm>
#include <cmath>

#include "i18n.hpp"
#include "audio.hpp"
#include "game.hpp"
#include "sprites.hpp"

void Game::offerEvolutions() {
  if (!offerEvolve_ || mode != Mode::Map || inExpedition()) return;  // écran de fin, combat suivant… : au prochain combat
  for (auto& f : team)
    if (canEvolve(*f) && !f->evolveLater) {
      FighterP who = f;
      ask(f->name() + T(" peut évoluer en ") + species(f->S().evolveTo).name + T(". Évoluer ?"), [this, who] {
        evolveMember(who);
        sc.call([this] { offerEvolutions(); });  // la suivante
      }, [this, who] {
        who->evolveLater = true;
        sc.say(who->name() + T(" n'évolue pas. Échap > Évolution pour changer d'avis."));
        sc.call([this] { offerEvolutions(); });
      });
      return;
    }
}

// L'effet : l'ancienne et la nouvelle forme alternent de plus en plus vite dans la lumière, puis
// l'évolution, un message, et l'effet disparaît
void Game::evolveMember(const FighterP& f) {
  if (!f || !canEvolve(*f)) return;
  std::string before = f->name();
  sc.call([this, f] {
    evoFrom_ = f->sp, evoTo_ = f->S().evolveTo, evoT_ = time, evoChroma_ = f->chroma;
    audio::play("statut");
  });
  sc.wait(1.8f);
  sc.call([this, f] {
    evolve(*f);
    seen.insert(f->sp), caught.insert(f->sp);
    audio::play("niveau");
  });
  sc.say(T("Félicitations ! ") + before + T(" a évolué en ") + species(f->S().evolveTo).name + " !");
  sc.call([this] { evoFrom_.clear(), evoTo_.clear(); });
}

void Game::evolutionMenu() {
  panelMode_ = 0;
  Menu m;
  m.title = T("Évolution");
  m.x = 60, m.y = 40, m.w = 200, m.rows = 6;
  for (auto& f : team)
    if (canEvolve(*f) && f->evolveLater) {
      FighterP who = f;
      m.items.push_back({f->name(), T("N.") + std::to_string(f->lvl), T("Entrée : évoluer en ") + species(f->S().evolveTo).name + ".", true,
                         [this, who] {
                           menus.clear();
                           evolveMember(who);
                         }});
    }
  if (m.items.empty()) m.items.push_back({"(aucune)", "", "", false, nullptr});
  m.onCancel = [this] {
    panelMode_ = 1;
    menus.pop();
  };
  menus.push(m);
}

void Game::drawEvolution() {
  if (!hasSpecies(evoFrom_) || !hasSpecies(evoTo_)) return;
  float k = time - evoT_;
  g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x000010, 170));
  g.newLayer();
  // Rayons de lumière qui tournent
  for (int i = 0; i < 12; i++) {
    float a = i * 0.5236f + k * .8f;
    g.line(160, 96, 160 + std::cos(a) * 120, 96 + std::sin(a) * 120, 3, rgb(0xfff2b0, uint8_t(std::min(1.f, k) * 50)));
  }
  g.ellipse(160, 100, 34 + 6 * std::sin(k * 6), 30, rgb(0xfff2b0, uint8_t(60 + 40 * std::min(1.f, k))));
  // Ancienne et nouvelle forme qui alternent de plus en plus vite ; ensuite la nouvelle seule
  bool after = k >= 1.8f || std::fmod(k * k * 4, 1.f) > .5f;
  drawCreature(g, after ? evoTo_ : evoFrom_, 160, 100, 1.6f, false, time, evoChroma_);
  if (k > 1.6f && k < 2.1f) g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0xffffff, uint8_t(220 * (1 - std::abs(k - 1.85f) / .25f))));
  drawDialogue();  // le message « … a évolué » par-dessus la lumière
}
