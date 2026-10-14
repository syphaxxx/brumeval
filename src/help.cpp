// Aide (Échap > Aide, écran titre > Aide, ou F1 partout) : des pages qui expliquent les
// statistiques, les dégâts, le combat, le damier et les zones, le rythme, les états, le tableau
// des types et les touches. Les nombres viennent des règles (data/regles.json) : l'aide reste
// juste quand on règle le jeu. Méthodes de Game (game.hpp).
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "data.hpp"
#include "game.hpp"
#include "options.hpp"

static const Color WHITE = rgb(0xffffff), GOLD = rgb(0xffd34d), MUTED = rgb(0xaab3d8), GREEN = rgb(0x7dffa8), RED = rgb(0xff8a7a),
                   DIM = rgb(0x5a6290);

// 0.75 → « 0,75 » ; 0.25 → « 25 % »
static std::string num(double x) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", x);
  std::string s = b;
  while (s.back() == '0') s.pop_back();
  if (s.back() == '.') s.pop_back();
  std::replace(s.begin(), s.end(), '.', ',');
  return s;
}
static std::string pct(double x) { return std::to_string(int(std::lround(x * 100))) + " %"; }

namespace {
struct HelpPage {
  std::string title, summary;
  std::vector<std::string> lines;  // vide : page spéciale (tableau des types)
};
}  // namespace

static std::vector<HelpPage> helpPages() {
  const Rules& R = rules();
  std::vector<HelpPage> p;
  p.push_back({"Statistiques", "PV, PM, Attaque, Défense, Magie, Résistance, Vitesse…",
               {"PV (points de vie) : à 0, le combattant est K.O. Un soigneur ou une auberge rend tout.",
                "PM (points de magie) : les sorts en coûtent. Les techniques sont gratuites.",
                "Attaque : force des coups physiques. Défense : protège des coups physiques.",
                "Magie : force des sorts et des soins. Résistance : protège des sorts.",
                "Vitesse : remplit plus vite la jauge ATB, donc on agit plus souvent.",
                "Précision : chance de toucher. Esquive : chance d'éviter un coup physique (jamais un sort).",
                "Critique : chance d'un coup critique (dégâts ×" + num(R.critMult) + ").",
                "Niveau : toutes les statistiques montent avec lui. L'expérience va surtout aux combattants ; les autres membres de l'équipe en reçoivent une part.",
                "Équipement (Échap > Équipement) : l'arme et l'armure (héros seulement) et l'accessoire ajoutent leurs bonus."}});
  p.push_back({"Dégâts", "Comment se calculent les dégâts.",
               {"Coup physique : Attaque du lanceur contre Défense de la cible. Sort : Magie contre Résistance. La puissance de la technique et le niveau du lanceur comptent aussi.",
                "Technique du même type que le lanceur : ×" + num(R.stab) + ".",
                "Type efficace : ×2 (×4 si les deux types de la cible sont faibles). Peu efficace : ×0,5. Sans effet : 0. Voir la page Types.",
                "Critique : ×" + num(R.critMult) + ". Un peu de hasard : de " + pct(R.spreadMin) + " à 100 % des dégâts.",
                "Bonus et malus : de −3 à +3 par statistique, " + pct(R.stageStep) + " de plus par niveau (un malus divise d'autant). Tout s'efface à la fin du combat.",
                "Certaines créatures résistent en plus aux coups physiques, à la magie ou à un type : leur fiche du Bestiaire donne leurs faiblesses."}});
  p.push_back({"Combat", "Jauge ATB, Limite, mode auto, objets, fuite.",
               {"Jauge ATB (trait blanc) : pleine, le combattant agit. Le temps s'arrête pendant que vous choisissez.",
                "Compagnons : chaque héros combat avec sa créature. Leur jauge avance à " + pct(R.companionAtb) + " de la vitesse normale.",
                "Limite (trait rose) : se remplit quand on reçoit des coups. Pleine, la technique ultime du combattant est prête.",
                "Mode auto (Tab) : chacun joue sa première tactique possible (Échap > Tactiques).",
                "Objets : soins, remèdes. Une lanterne capture une créature sauvage : plus elle est affaiblie, mieux c'est.",
                "Fuir : " + pct(R.flee) + " de chance de réussir, pas contre tous les adversaires.",
                "Changer : un autre membre de l'équipe prend la place d'un combattant."}});
  p.push_back({"Brisure", "Le bouclier des ennemis, leurs faiblesses, l'ennemi brisé.",
               {"Chaque ennemi a un bouclier (le nombre à gauche de sa barre de vie) : " + std::to_string(R.breakShield) + " points en général, " +
                    std::to_string(R.breakShieldBoss) + " pour un boss.",
                "Le frapper sur une faiblesse (un type efficace contre lui) retire un point, deux s'il est doublement faible. La faiblesse est alors découverte : une pastille de sa couleur s'affiche sous sa barre, et le Bestiaire la retient.",
                "Bouclier à zéro : l'ennemi est BRISÉ. Sa jauge retombe à zéro, son attaque annoncée est annulée, il subit ×" + num(R.breakDamage) +
                    " de dégâts et perd son prochain tour. Puis il se ressaisit, bouclier rechargé.",
                "Le bon moment pour les plus gros coups et les Limites : brisez d'abord, frappez ensuite.",
                "Dans le menu de la cible, « faiblesse ! » signale un type déjà découvert ; « ? », un type pas encore essayé contre lui."}});
  p.push_back({"Gemmes", "Sertir des gemmes dans l'équipement.",
               {"Chaque équipement porté a des chatons : " + std::to_string(R.socketWeapon) + " pour une arme, " + std::to_string(R.socketArmor) +
                    " pour une armure, " + std::to_string(R.socketAccessory) + " pour un accessoire, un de plus forgé à +" + std::to_string(R.socketForge) + ".",
                "Échap > Équipement > un membre > Gemmes : choisir un chaton, puis la gemme du sac (ou la retirer).",
                "Gemmes de technique : donnent un sort ou une technique (Feu, Soin, Rempart…), même à une créature.",
                "Gemmes de bonus : un pourcentage de PV, d'Attaque ou de Magie en plus.",
                "Gemmes de soutien : éveil (jauge avancée au début du combat), vampire (rend une part des dégâts infligés), riposte (rend les coups au corps à corps), brise-garde (use plus les boucliers).",
                "On les achète dans les boutiques ; chaque grand boss en laisse une. Changer d'équipement rend au sac les gemmes qui n'ont plus de place."}});
  p.push_back({"Rôles", "Les rôles des ennemis, les phases des boss.",
               {"Certaines créatures ont un rôle, montré par une icône à droite de leur barre de vie (et dans le menu de la cible) :",
                "Protecteur (écu gris) : ses voisins de damier (en croix) reçoivent " + pct(R.roleGuard) + " de dégâts en moins. Abattez-le ou poussez sa cible loin de lui.",
                "Soigneur (croix verte) : soigne ses alliés blessés au lieu d'attaquer. À éliminer en premier.",
                "Chef (couronne) : tant qu'il est debout, la jauge ATB de ses alliés va " + pct(R.roleChief) + " plus vite.",
                "Vos créatures protectrices et chefs aident aussi votre équipe.",
                "Les boss ont des phases : sous un seuil de PV, ils changent (bouclier rechargé, bonus, nouvelles techniques, autre météo, renforts). Gardez de quoi encaisser leur seconde forme."}});
  p.push_back({"Pousser", "Pousser et tirer les combattants sur le damier.",
               {"Certaines techniques poussent la cible d'une case vers l'arrière (Charge, Ruée, Rafale, Vague, Fracas…) ou la tirent vers l'avant (Flèche-grappin, Tornade, Étreinte…). Leur description le précise.",
                "Si la case d'arrivée est déjà prise, les deux combattants se heurtent : chacun subit un choc de " + pct(R.shoveClash) +
                    " des dégâts du coup. Poussé contre le bord arrière : choc aussi.",
                "Tirer un mage vers l'avant le met à portée du corps à corps ; pousser un ennemi sur des braises le brûle aussitôt.",
                "Les boss sont inébranlables. Les ennemis aussi poussent et tirent : gardez vos soigneurs loin des tireurs de grappin."}});
  {
    std::vector<std::string> dl = {"Deux alliés qui vont ensemble lancent une technique à deux, plus forte (menu « Duo »). Le partenaire doit avoir sa jauge ATB à " +
                                       std::to_string(R.duoGauge) + " % au moins : il la dépense, et chacun paie les PM.",
                                   "En mode auto, la tactique « Meilleure attaque » choisit un duo quand il vaut le coup."};
    for (auto& d : R.duos) {
      if (!hasMove(d.move) || d.members.size() != 2) continue;
      auto who = [](const std::string& m) {
        if (m == "humain") return std::string("un héros");
        if (m.rfind("type:", 0) == 0) {
          for (auto& t : types())
            if (t.id == m.substr(5)) return "une créature " + t.name;
          return m;
        }
        return hasSpecies(m) ? species(m).name : m;
      };
      dl.push_back(moveInfo(d.move).name + " : " + who(d.members[0]) + " + " + who(d.members[1]) + ".");
    }
    p.push_back({"Duos", "Les techniques à deux.", dl});
  }
  {
    std::vector<std::string> ml = {"Chaque combat a un temps, selon le lieu (affiché sous l'ordre des tours) :"};
    for (auto& w : R.weathers) ml.push_back(w.name + " : " + w.desc);
    ml.push_back("Des cases du damier ont un terrain, de leur couleur. Dans « Se déplacer », chaque case dit le sien :");
    for (auto& t : R.terrains) ml.push_back(t.name + " : " + t.desc);
    ml.push_back("En mode auto, la tactique « Se mettre à l'abri » quitte aussi une case qui blesse ou ralentit.");
    p.push_back({"Météo", "Le temps et les cases de terrain.", ml});
  }
  {
    std::vector<std::string> tl = {"Chaque espèce a un talent, un effet permanent (fiche d'un membre : Échap > Équipe ; Bestiaire). Une évolution le garde."};
    for (auto& t : R.talents) tl.push_back(t.name + " : " + t.desc);
    p.push_back({"Talents", "Le pouvoir passif de chaque espèce.", tl});
  }
  p.push_back({"Damier", "Les " + std::to_string(N_CELLS) + " cases de chaque camp, corps à corps et distance.",
               {"Chaque camp a " + std::to_string(N_CELLS) + " cases : " + std::to_string(GRID) + " colonnes (de l'avant à l'arrière) et " +
                    std::to_string(GRID) + " lignes. L'avant est du côté de l'adversaire.",
                "Corps à corps : ne vise que la colonne ennemie la plus avancée encore debout. Tant que l'avant tient, l'arrière est à l'abri.",
                "À distance (sorts, flèches, jets…) : n'importe quelle case.",
                "Au corps à corps, frapper depuis l'arrière ou viser l'arrière ne fait que " + pct(R.backRow) + " des dégâts.",
                "Se déplacer : une action rapide (la jauge repart à " + std::to_string(R.quickGauge) + " %), pour s'abriter ou s'approcher.",
                "Formation (Échap > Formation) : la case de chacun au début du combat. Les costauds devant, les lanceurs de sorts derrière."}});
  p.push_back({"Zones", "Ligne, colonne, croix, tout le camp ; attaques annoncées.",
               {"Une cible : seulement la case visée.",
                "Ligne : la case visée et toute sa ligne, de l'avant à l'arrière.",
                "Colonne : la case visée et toute sa colonne, de haut en bas.",
                "Croix : la case visée et ses voisines (dessus, dessous, devant, derrière).",
                "Tout le camp : tous les ennemis (ou tous les alliés), sans viser.",
                "Autour de la case visée, une zone fait " + pct(R.zoneSide) + " des dégâts. En visant, les cases touchées s'allument en or : des ennemis serrés sont une belle cible, vos combattants aussi !",
                "Attaques annoncées : un boss marque parfois une zone en rouge. À son tour suivant, il frappe ×" + num(R.bossBurst) + " ceux qui y sont encore : écartez-vous."}});
  p.push_back({"Rythme", "Techniques rapides, normales et lourdes.",
               {"Normale : après l'action, la jauge ATB repart de zéro.",
                "Rapide : la jauge repart à " + std::to_string(R.quickGauge) + " % : on rejoue plus tôt. Souvent plus faible, parfois avec un effet en plus.",
                "Lourde : la jauge repart à −" + std::to_string(R.heavyDelay) + " % : on rejoue plus tard. Ce sont les plus fortes.",
                "Se déplacer est toujours rapide.",
                "Chaque technique dit son rythme dans sa description (en bas de l'écran quand on la choisit)."}});
  // États : effets chiffrés, puis les types insensibles
  std::vector<std::string> st = {
      "Poison : perd " + pct(R.poisonDmg) + " de ses PV max à la fin de chacun de ses tours.",
      "Brûlure : perd " + pct(R.burnDmg) + " de ses PV max par tour, et son Attaque ×" + num(R.burnAtk) + ".",
      "Paralysie : Vitesse ×" + num(R.paraSpeed) + " et " + pct(R.paraSkip) + " de chance de perdre son tour.",
      "Sommeil : ne fait rien pendant " + std::to_string(R.sleepMin) + " à " + std::to_string(R.sleepMax) + " tours.",
      "Un seul état à la fois. Un Remède ou un sort de guérison l'enlève, et tout s'efface à la fin du combat."};
  std::string imm;
  for (auto& t : types())
    if (!t.immune.empty()) {
      std::string list;
      for (auto& s : t.immune) list += (list.empty() ? "" : ", ") + std::string(statusName(statusOf(s)));
      imm += (imm.empty() ? "" : " ; ") + t.name + " : " + list;
    }
  if (!imm.empty()) st.push_back("Insensibles : " + imm + ".");
  p.push_back({"États", "Poison, brûlure, paralysie, sommeil.", st});
  p.push_back({"Types", "Le tableau des forces et faiblesses entre les types.", {}});
  {
    int n = 0, lo = 99, hi = 0;
    for (auto& s : allSpecies())
      if (!s.evolveTo.empty()) n++, lo = std::min(lo, s.evolveLvl), hi = std::max(hi, s.evolveLvl);
    if (n)
      p.push_back({"Évolution", "Les créatures qui deviennent plus fortes.",
                   {std::to_string(n) + " créatures peuvent évoluer, du niveau " + std::to_string(lo) + " au niveau " + std::to_string(hi) +
                        " selon l'espèce (fiche du Bestiaire : « Évolue en … »).",
                    "Après le combat où elle atteint ce niveau, le jeu demande « Évoluer ? ». Elle devient une espèce plus forte, un peu plus grande, et apprend de nouvelles techniques.",
                    "Elle garde son niveau, son expérience, son équipement, ses tactiques, sa place sur le damier et le héros qu'elle accompagne.",
                    "Vous pouvez répondre Non (une petite créature est plus rapide à faire progresser) : Échap > Évolution la fait évoluer plus tard.",
                    "Les formes évoluées vivent aussi à l'état sauvage dans les régions les plus lointaines."}});
  }
  {
    std::vector<std::string> fl = {"À Forgeroc, une fois Brann dans l'équipe, la forge renforce une arme ou une armure (portée ou dans le sac)."};
    for (size_t L = 0; L < R.forge.size(); L++) {
      std::string need;
      for (auto& [id, n] : R.forge[L].items) need += ", " + std::to_string(n) + " " + (id == "trophee" ? std::string("trophée de boss") : item(id).name);
      fl.push_back("+" + std::to_string(L + 1) + " : bonus de l'objet +" + pct(R.forge[L].bonus) + " (arrondi au-dessus). Coût : " +
                   std::to_string(R.forge[L].gold) + " or" + need + ".");
    }
    std::string drops;
    for (auto& td : R.typeDrops) {
      std::string tn = td.type;
      for (auto& t : types())
        if (t.id == td.type) tn = t.name;
      drops += (drops.empty() ? "" : ", ") + item(td.item).name + " (créatures " + tn + ")";
    }
    if (!drops.empty()) fl.push_back("Matériaux : butin des combats, " + drops + ", et quelques coffres.");
    fl.push_back("Chaque boss vaincu laisse un trophée. Un objet forgé se revend plus cher.");
    p.push_back({"Forge", "Renforcer armes et armures à Forgeroc.", fl});
  }
  const auto& K = options().keys;
  p.push_back({"Touches", "Clavier, raccourcis F1, F2, F11.",
               {"Se déplacer, choisir : flèches ou " + keyName(K[K_UP]) + keyName(K[K_LEFT]) + keyName(K[K_DOWN]) + keyName(K[K_RIGHT]) + ".",
                "Valider, parler : Entrée ou " + keyName(K[K_OK]) + ". Retour, menu : Échap ou " + keyName(K[K_BACK]) + ".",
                "Mode auto en combat : " + keyName(K[K_AUTO]) + ".",
                "F1 : cette aide. F2 : vitesse du jeu ×1, ×2 ou ×3. F11 : plein écran.",
                "Les touches se changent dans Options > Touches. Une manette branchée marche toute seule."}});
  return p;
}

void Game::helpMenu(int sel) {
  auto pages = helpPages();
  int panel = panelMode_;
  panelMode_ = 0;  // le menu prend la place du résumé de l'équipe
  Menu m;
  m.title = "Aide";
  m.x = 96, m.y = 30, m.w = 128, m.rows = std::min(9, (int)pages.size() + 1), m.sel = sel;  // au-delà, la liste défile
  for (size_t i = 0; i < pages.size(); i++)
    m.items.push_back({pages[i].title, "", pages[i].summary + " (aussi avec F1)", true, [this, i] { openHelp((int)i); }});
  m.items.push_back({"Retour", "", "", true, [this, panel] {
                       panelMode_ = panel;
                       menus.pop();
                     }});
  m.onCancel = m.items.back().act;
  menus.push(m);
}

bool Game::tip(const std::string& id, Script& s) {
  if (tipsOff_ || !options().tips || pilot_ || tipsSeen.count(id)) return false;
  for (auto& t : rules().tips) {
    if (t.id != id) continue;
    tipsSeen.insert(id);
    auto pages = helpPages();
    for (size_t i = 0; i < pages.size(); i++)
      if (pages[i].title == t.help) lastHelp_ = (int)i;
    s.say("Astuce : " + t.text + " (F1 : en savoir plus)", 0);
    return true;
  }
  return false;
}

std::string Game::adviceText() const {
  for (auto& a : rules().advice) {
    if (has(a.flag) || !hasSpecies(a.boss)) continue;
    const Species& s = species(a.boss);
    std::string t = "Prochain gardien : " + s.name + (a.place.empty() ? "" : ", dans " + a.place) + ". ";
    std::string known;
    for (int ty = 0; ty < (int)types().size(); ty++)
      if (weakKnown(a.boss, ty)) known += (known.empty() ? "" : ", ") + types()[ty].name;
    t += known.empty() ? "Ses faiblesses sont à découvrir : essayez chaque type, le Bestiaire s'en souviendra. "
                       : "Faiblesses connues : " + known + ". ";
    t += a.text;
    if (!a.gem.empty() && hasItem(a.gem)) t += " Gemme conseillée : " + item(a.gem).name + ".";
    return t;
  }
  return "";
}

void Game::openHelp(int page) {
  helpPage_ = std::clamp(page, 0, (int)helpPages().size() - 1);
  helpScroll_ = 0;
  in.endFrame();
}

// Pendant l'aide, le reste du jeu attend. Gauche / droite : page précédente / suivante ;
// haut / bas : faire défiler une page longue ; Entrée, Échap ou un clic : fermer.
void Game::updateHelp() {
  int n = (int)helpPages().size();
  if (in.press[LEFT]) helpPage_ = (helpPage_ + n - 1) % n, helpScroll_ = 0;
  if (in.press[RIGHT]) helpPage_ = (helpPage_ + 1) % n, helpScroll_ = 0;
  if (in.press[UP] || in.wheel > 0) helpScroll_ = std::max(0, helpScroll_ - 1);
  if (in.press[DOWN] || in.wheel < 0) helpScroll_++;  // borné au dessin
  if (in.confirm || in.cancel || in.mclick[0]) {
    if (menus.active() && menus.top().title == "Aide") menus.top().sel = helpPage_;
    helpPage_ = -1;
  }
}

void Game::drawHelp() {
  auto pages = helpPages();
  if (helpPage_ < 0 || helpPage_ >= (int)pages.size()) return;
  const HelpPage& p = pages[(size_t)helpPage_];
  g.rect(g.left(), 0, g.fullW, SCREEN_H, rgb(0x000010, 150));
  g.newLayer();
  g.window(4, 4, 312, 232);
  g.text(160, 9, "Aide · " + p.title + "  (" + std::to_string(helpPage_ + 1) + "/" + std::to_string(pages.size()) + ")", GOLD, 1);
  g.text(160, 222, "Gauche / droite : page · Échap : fermer", MUTED, 1);
  if (!p.lines.empty()) {
    std::vector<std::pair<std::string, bool>> rows;  // ligne, début de paragraphe
    for (auto& l : p.lines) {
      auto w = Gfx::wrap(l, 288);
      for (size_t i = 0; i < w.size(); i++) rows.push_back({w[i], i == 0});
    }
    const int visible = 17;
    helpScroll_ = std::clamp(helpScroll_, 0, std::max(0, (int)rows.size() - visible));
    for (int i = 0; i < visible && helpScroll_ + i < (int)rows.size(); i++) {
      auto& [s, first] = rows[(size_t)(helpScroll_ + i)];
      float y = 24 + i * 11;
      if (first) g.text(12, y, "·", GOLD);
      g.text(20, y, s, WHITE);
    }
    if ((int)rows.size() > visible) g.text(306, 210, helpScroll_ + visible < (int)rows.size() ? "▼" : " ", GOLD, 2);
    return;
  }
  // Tableau des types : ligne = type de l'attaque, colonne = type de la cible
  const auto& T = types();
  int n = (int)T.size();
  const float cw = std::min(21.f, 272.f / std::max(1, n)), x0 = 38, y0 = 24;
  for (int d = 0; d < n; d++) g.text(x0 + d * cw + cw / 2, y0, utf8Prefix(T[d].name, 3), rgb(T[d].color), 1);
  for (int a = 0; a < n; a++) {
    float y = y0 + 12 + a * 12;
    if (a % 2 == 0) g.rect(10, y - 2, 300, 12, rgb(0xffffff, 14));
    g.text(12, y, utf8Prefix(T[a].name, 3), rgb(T[a].color));
    for (int d = 0; d < n; d++) {
      float e = typeEff(a, d), x = x0 + d * cw + cw / 2;
      if (e > 1) g.text(x, y, "2", GREEN, 1);
      else if (e == 0) g.text(x, y, "0", MUTED, 1);
      else if (e < 1) g.text(x, y, "-", RED, 1);
      else g.text(x, y, "·", DIM, 1);
    }
  }
  float ly = y0 + 16 + n * 12;
  g.text(160, ly, "Ligne : l'attaque · colonne : la cible", MUTED, 1);
  g.text(20, ly + 12, "2", GREEN);
  g.text(30, ly + 12, "efficace ×2", WHITE);
  g.text(112, ly + 12, "-", RED);
  g.text(122, ly + 12, "peu efficace ×0,5", WHITE);
  g.text(232, ly + 12, "0", MUTED);
  g.text(242, ly + 12, "sans effet", WHITE);
}
