#include "audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "store.hpp"

namespace audio {

int playedCount = 0;
std::string lastEffect;

namespace {

constexpr int RATE = 44100;
enum Wave { SQUARE, TRIANGLE, SAW, SINE, NOISE };

// Un son simple d'un effet : onde, fréquence qui glisse de f0 à f1, à partir de « start »
struct Seg {
  Wave wave = SQUARE;
  float f0 = 440, f1 = 440, start = 0, dur = .1f, vol = .3f, duty = .5f;
};
struct Effect {
  std::vector<Seg> segs;
  float length = 0;
};
// Une case de la partition : note, prolongation de la précédente (tie), silence ou coup de bruit
struct Note {
  float freq = 0;
  bool tie = false, rest = true;
};
struct Voice {
  Wave wave = SQUARE;
  float vol = .2f, duty = .5f, decay = 0;  // decay > 0 : son pincé qui s'éteint (secondes)
  std::vector<Note> notes;
};
struct Track {
  float step = .25f;  // durée d'une case (secondes)
  std::vector<Voice> voices;
};

std::map<std::string, Effect> effects_;
std::map<std::string, Track> tracks_;
std::map<std::string, std::string> places_;
std::vector<std::string> problems_;

// ---------------------------------------------------------------------------
// Lecture de data/sons.json
// ---------------------------------------------------------------------------
Wave waveOf(const std::string& s, const std::string& where) {
  if (s == "carree") return SQUARE;
  if (s == "triangle") return TRIANGLE;
  if (s == "scie") return SAW;
  if (s == "sinus") return SINE;
  if (s == "bruit") return NOISE;
  throw std::runtime_error("sons.json : " + where + " : onde inconnue « " + s + " » (carree, triangle, scie, sinus, bruit)");
}

// « A4 », « C#5 », « Bb3 » -> fréquence en Hz ; 0 si ce n'est pas une note
float noteFreq(const std::string& s) {
  static const int SEMI[] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G
  if (s.size() < 2 || s[0] < 'A' || s[0] > 'G') return 0;
  int semi = SEMI[s[0] - 'A'];
  size_t i = 1;
  if (s[i] == '#') semi++, i++;
  else if (s[i] == 'b') semi--, i++;
  if (i >= s.size() || !std::isdigit((unsigned char)s[i])) return 0;
  int oct = std::atoi(s.c_str() + i);
  int midi = 12 * (oct + 1) + semi;
  return 440.f * std::pow(2.f, (midi - 69) / 12.f);
}

float freqOf(const Json& j, const std::string& where) {
  if (j.is_number()) return std::max(1.f, j.get<float>());
  if (j.is_string()) {
    float f = noteFreq(j.get<std::string>());
    if (f > 0) return f;
  }
  throw std::runtime_error("sons.json : " + where + " : fréquence ou note invalide (" + j.dump() + ")");
}

Effect readEffect(const std::string& id, const Json& list) {
  Effect e;
  float t = 0;
  for (auto& s : list) {
    Seg g;
    std::string where = "effet « " + id + " »";
    g.wave = waveOf(jget<std::string>(s, "onde", "carree"), where);
    g.f0 = freqOf(s.value("de", Json(440)), where);
    g.f1 = s.contains("a") ? freqOf(s["a"], where) : g.f0;
    g.dur = std::max(.005f, jget(s, "duree", .1f));
    g.vol = std::clamp(jget(s, "volume", .3f), 0.f, 1.f);
    g.duty = std::clamp(jget(s, "rapport", .5f), .05f, .95f);
    g.start = s.contains("depart") ? std::max(0.f, s["depart"].get<float>()) : t;  // sans « depart » : après le précédent
    t = g.start + g.dur;
    e.length = std::max(e.length, t);
    e.segs.push_back(g);
  }
  return e;
}

Track readTrack(const std::string& id, const Json& j) {
  Track tr;
  float tempo = std::clamp(jget(j, "tempo", 120.f), 20.f, 400.f);
  int perBeat = std::clamp(jget(j, "pas_par_temps", 2), 1, 8);
  tr.step = 60.f / tempo / perBeat;
  Json voices = j.value("voix", Json::array());
  for (auto& v : voices) {
    Voice vo;
    std::string where = "musique « " + id + " »";
    vo.wave = waveOf(jget<std::string>(v, "onde", "carree"), where);
    vo.vol = std::clamp(jget(v, "volume", .2f), 0.f, 1.f);
    vo.duty = std::clamp(jget(v, "rapport", .5f), .05f, .95f);
    vo.decay = std::max(0.f, jget(v, "extinction", vo.wave == NOISE ? .06f : 0.f));
    float hitFreq = v.contains("frequence") ? freqOf(v["frequence"], where) : 4000.f;
    std::istringstream in(jget<std::string>(v, "notes", ""));
    std::string tok;
    while (in >> tok) {
      if (tok == "|") continue;  // barre de mesure : seulement pour s'y retrouver
      Note n;
      if (tok == ".") n.tie = true, n.rest = false;
      else if (tok == "-") n.rest = true;
      else if (tok == "x") n.freq = hitFreq, n.rest = false;
      else if ((n.freq = noteFreq(tok)) > 0) n.rest = false;
      else throw std::runtime_error("sons.json : " + where + " : note inconnue « " + tok + " » (ex. A4, C#5, Bb3, « . », « - », « x »)");
      vo.notes.push_back(n);
    }
    if (!vo.notes.empty()) tr.voices.push_back(vo);
  }
  return tr;
}

// ---------------------------------------------------------------------------
// Synthèse
// ---------------------------------------------------------------------------
struct Osc {
  float phase = 0, noise = 1;
  uint32_t lfsr = 0xACE1u;
  float next(Wave w, float f, float duty) {
    phase += f / RATE;
    bool wrap = phase >= 1;
    if (wrap) phase -= std::floor(phase);
    switch (w) {
      case SQUARE: return phase < duty ? 1.f : -1.f;
      case TRIANGLE: return 4 * std::fabs(phase - .5f) - 1;
      case SAW: return 2 * phase - 1;
      case SINE: return std::sin(6.2831853f * phase);
      case NOISE:
        if (wrap) {  // bruit : une nouvelle valeur au hasard à chaque période
          lfsr = (lfsr >> 1) ^ (-(int32_t)(lfsr & 1u) & 0xB400u);
          noise = (lfsr & 1u) ? 1.f : -1.f;
        }
        return noise;
    }
    return 0;
  }
};

struct EffectPlay {
  const Effect* e;
  float t = 0;
  std::vector<Osc> osc;
};

struct VoicePlay {
  Osc osc;
  float freq = 0, t = 0;  // t : temps depuis le début de la note
  bool on = false;
};

class Mixer {
 public:
  float musicGain = .5f, fxGain = .6f;
  std::string track, pending;  // morceau en cours ; celui qui le remplacera après le fondu
  bool switching = false;

  void playEffect(const std::string& id) {
    auto it = effects_.find(id);
    if (it == effects_.end()) return;
    if (fx_.size() >= 12) fx_.erase(fx_.begin());  // trop d'effets à la fois : le plus ancien s'arrête
    EffectPlay p{&it->second, 0, std::vector<Osc>(it->second.segs.size())};
    fx_.push_back(p);
  }
  void setMusic(const std::string& id) {
    if (switching ? id == pending : id == track) return;
    if (track.empty() || !tr_) return start(id);
    pending = id;  // fondu de l'ancien morceau, puis le nouveau commence
    switching = true;
  }
  void mix(float* out, int n) {
    const float dt = 1.f / RATE;
    for (int i = 0; i < n; i++) {
      float m = 0, fx = 0;
      if (switching) {
        fade_ -= dt / .35f;
        if (fade_ <= 0) start(pending);
      }
      if (tr_) m = musicSample(dt) * fade_;
      for (auto& p : fx_) {
        p.t += dt;
        for (size_t k = 0; k < p.e->segs.size(); k++) {
          const Seg& s = p.e->segs[k];
          float u = (p.t - s.start) / s.dur;
          if (u < 0 || u >= 1) continue;
          float f = s.f0 * std::pow(s.f1 / s.f0, u);  // glissement de fréquence
          float env = std::min(1.f, (p.t - s.start) / .003f) * (u < .7f ? 1.f : (1 - u) / .3f);
          fx += p.osc[k].next(s.wave, f, s.duty) * s.vol * env;
        }
      }
      out[i] = std::clamp((m * musicGain + fx * fxGain) * .7f, -1.f, 1.f);
    }
    fx_.erase(std::remove_if(fx_.begin(), fx_.end(), [](const EffectPlay& p) { return p.t > p.e->length; }), fx_.end());
  }

 private:
  const Track* tr_ = nullptr;
  std::vector<VoicePlay> voices_;
  std::vector<EffectPlay> fx_;
  int step_ = -1;
  float stepT_ = 0, fade_ = 1;

  void start(const std::string& id) {
    track = id;
    switching = false;
    pending.clear();
    fade_ = 1;
    auto it = tracks_.find(id);
    tr_ = it == tracks_.end() ? nullptr : &it->second;
    voices_.assign(tr_ ? tr_->voices.size() : 0, VoicePlay{});
    step_ = -1;
    stepT_ = tr_ ? tr_->step : 0;  // la première case commence tout de suite
  }
  float musicSample(float dt) {
    const Track& T = *tr_;
    stepT_ += dt;
    if (stepT_ >= T.step) {  // case suivante de la partition
      stepT_ -= T.step;
      step_++;
      for (size_t v = 0; v < T.voices.size(); v++) {
        const Note& n = T.voices[v].notes[(size_t)step_ % T.voices[v].notes.size()];
        VoicePlay& p = voices_[v];
        if (n.tie) continue;
        p.on = !n.rest;
        if (p.on) p.freq = n.freq, p.t = 0;
      }
    }
    float s = 0;
    for (size_t v = 0; v < T.voices.size(); v++) {
      const Voice& V = T.voices[v];
      VoicePlay& p = voices_[v];
      if (!p.on) continue;
      p.t += dt;
      float env = std::min(1.f, p.t / .005f);
      env *= V.decay > 0 ? std::exp(-p.t / V.decay) : .7f + .3f * std::exp(-p.t / .08f);
      // Fin de la note (la case suivante n'est pas une prolongation) : extinction rapide
      const Note& next = V.notes[(size_t)(step_ + 1) % V.notes.size()];
      if (!next.tie) env *= std::min(1.f, (T.step - stepT_) / .02f);
      s += p.osc.next(V.wave, p.freq, V.duty) * V.vol * env;
    }
    return s;
  }
};

Mixer mixer_;
SDL_AudioDeviceID dev_ = 0;

void callback(void*, Uint8* stream, int len) { mixer_.mix(reinterpret_cast<float*>(stream), len / (int)sizeof(float)); }

// Les changements passent par ce verrou : le son est fabriqué dans un autre fil
struct Lock {
  Lock() {
    if (dev_) SDL_LockAudioDevice(dev_);
  }
  ~Lock() {
    if (dev_) SDL_UnlockAudioDevice(dev_);
  }
};

}  // namespace

// Effets que le jeu demande (le mode test vérifie qu'ils existent tous)
static const char* REQUIRED_EFFECTS[] = {"curseur", "valider", "retour", "refus", "coup", "critique", "sort", "soin", "rate", "ko",
                                         "statut", "bonus", "malus", "lancer", "capture", "capture_rate", "niveau", "victoire",
                                         "defaite", "rencontre", "fuite", "coffre", "achat", "porte", "limite"};
static const char* REQUIRED_TRACKS[] = {"titre", "combat", "boss"};

void loadSounds() {
  Json j = readJson("sons.json");
  std::map<std::string, Effect> fx;
  std::map<std::string, Track> tr;
  std::map<std::string, std::string> pl;
  // (listes rangées dans des variables : jamais de boucle sur un j.value(...) temporaire)
  Json effects = j.value("effets", Json::object()), tracks = j.value("musiques", Json::object()), places = j.value("lieux", Json::object());
  for (auto& [id, e] : effects.items()) fx[id] = readEffect(id, e);
  for (auto& [id, t] : tracks.items()) tr[id] = readTrack(id, t);
  for (auto& [theme, t] : places.items()) pl[theme] = t.get<std::string>();
  Lock lock;
  std::string playing = mixer_.track;
  float mg = mixer_.musicGain, fg = mixer_.fxGain;
  effects_ = std::move(fx);
  tracks_ = std::move(tr);
  places_ = std::move(pl);
  mixer_ = Mixer{};  // les anciens morceaux n'existent plus : on repart du début
  mixer_.musicGain = mg, mixer_.fxGain = fg;
  mixer_.setMusic(playing);
}

std::vector<std::string> checkSounds() {
  std::vector<std::string> p;
  for (auto* e : REQUIRED_EFFECTS)
    if (!effects_.count(e)) p.push_back(std::string("sons.json : effet « ") + e + " » manquant");
  for (auto* t : REQUIRED_TRACKS)
    if (!tracks_.count(t)) p.push_back(std::string("sons.json : musique « ") + t + " » manquante");
  for (auto& [theme, t] : places_)
    if (!tracks_.count(t)) p.push_back("sons.json : lieu « " + theme + " » : musique « " + t + " » inconnue");
  for (auto& [id, t] : tracks_)
    if (t.voices.empty()) p.push_back("sons.json : musique « " + id + " » sans notes");
  return p;
}

void init() {
  if (dev_ || SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
  SDL_AudioSpec want{}, have{};
  want.freq = RATE;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = callback;
  dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (dev_) SDL_PauseAudioDevice(dev_, 0);
}

void shutdown() {
  if (dev_) SDL_CloseAudioDevice(dev_);
  dev_ = 0;
}

void play(const std::string& effect) {
  playedCount++;
  lastEffect = effect;
  Lock lock;
  mixer_.playEffect(effect);
}

void music(const std::string& track) {
  Lock lock;
  mixer_.setMusic(track);
}

std::string currentMusic() {
  Lock lock;
  return mixer_.switching ? mixer_.pending : mixer_.track;
}

std::string musicForPlace(const std::string& theme) {
  auto it = places_.find(theme);
  return it == places_.end() ? "" : it->second;
}

void setVolumes(int music, int effects) {
  auto gain = [](int v) {
    float x = std::clamp(v, 0, 10) / 10.f;
    return x * x;  // l'oreille entend mieux les petits volumes ainsi
  };
  Lock lock;
  mixer_.musicGain = gain(music);
  mixer_.fxGain = gain(effects);
}

std::vector<float> render(const std::string& name, bool isMusic, float seconds) {
  Mixer m;
  m.musicGain = m.fxGain = 1;
  if (isMusic) m.setMusic(name);
  else m.playEffect(name);
  std::vector<float> out((size_t)std::max(0.f, seconds * RATE));
  m.mix(out.data(), (int)out.size());
  return out;
}

std::vector<std::string> effectNames() {
  std::vector<std::string> v;
  for (auto& [id, e] : effects_) v.push_back(id);
  return v;
}
std::vector<std::string> trackNames() {
  std::vector<std::string> v;
  for (auto& [id, t] : tracks_) v.push_back(id);
  return v;
}

}  // namespace audio
