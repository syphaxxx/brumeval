#include "ui.hpp"

#include <algorithm>

std::string fmtInt(int v) { return std::to_string(v); }
std::string utf8Prefix(const std::string& s, int n) {
  size_t i = 0;
  int c = 0;
  while (i < s.size()) {
    if ((s[i] & 0xC0) != 0x80) {
      if (c == n) break;
      c++;
    }
    i++;
  }
  return s.substr(0, i);
}

// ---------------------------------------------------------------------------
// Script
// ---------------------------------------------------------------------------
void Script::push(Step s) {
  if (collecting_) pending_.push_back(std::move(s));
  else q_.push_back(std::move(s));
}
void Script::say(const std::string& text, float autoSec) { push({Msg, text, nullptr, autoSec}); }
void Script::call(std::function<void()> fn) { push({Call, "", std::move(fn), 0}); }
void Script::wait(float sec) { push({Wait, "", nullptr, sec}); }
void Script::runNow(const std::function<void()>& fn) {
  if (collecting_) {  // déjà dans un appel : les étapes seront insérées juste après lui
    if (fn) fn();
    return;
  }
  collecting_ = true;
  pending_.clear();
  if (fn) fn();
  collecting_ = false;
  for (auto it = pending_.rbegin(); it != pending_.rend(); ++it) q_.push_front(std::move(*it));
  pending_.clear();
}

static int countChars(const std::string& s) {
  int n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) n++;
  return n;
}
bool Script::messageDone() const { return typed_ >= countChars(text_); }

void Script::update(float dt, Input& in) {
  while (!q_.empty() && !halted_) {
    Step& s = q_.front();
    if (s.kind == Call) {
      auto fn = std::move(s.fn);
      q_.pop_front();
      collecting_ = true;
      pending_.clear();
      if (fn) fn();
      collecting_ = false;
      for (auto it = pending_.rbegin(); it != pending_.rend(); ++it) q_.push_front(std::move(*it));
      pending_.clear();
      continue;
    }
    if (s.kind == Wait) {
      t_ += dt;
      if (t_ >= s.dur) {
        t_ = 0;
        q_.pop_front();
        continue;
      }
      return;
    }
    // Message
    if (!started_) {
      started_ = true;
      text_ = s.text;
      typed_ = 0;
      t_ = 0;
    }
    int len = countChars(text_);
    typed_ = std::min<float>(len, typed_ + dt * 60);
    t_ += dt;
    bool next = false;
    if (in.confirm) {
      in.confirm = false;
      if (typed_ < len) typed_ = (float)len;
      else next = true;
    }
    if (s.dur > 0 && t_ >= s.dur + (len > 40 ? .4f : 0)) next = true;
    if (next) {
      q_.pop_front();
      started_ = false;
      t_ = 0;
      continue;
    }
    return;
  }
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------
// Saute les titres de section dans le sens d (+1 ou -1)
static void skipHeaders(Menu& m, int d) {
  int n = (int)m.items.size();
  for (int k = 0; k < n && m.items[m.sel].header; k++) m.sel = (m.sel + d + n) % n;
}

void MenuStack::push(Menu m) {
  m.sel = std::clamp(m.sel, 0, std::max(0, (int)m.items.size() - 1));
  if (!m.items.empty()) skipHeaders(m, 1);
  if (m.sel >= m.top + m.rows) m.top = m.sel - m.rows + 1;
  st_.push_back(std::move(m));
  hover();
}
void MenuStack::pop() {
  if (!st_.empty()) st_.pop_back();
  hover();
}
void MenuStack::hover() {
  if (st_.empty()) return;
  Menu& m = st_.back();
  if (m.sel < (int)m.items.size() && m.items[m.sel].hover) m.items[m.sel].hover();
}
std::string MenuStack::help() const {
  if (st_.empty()) return "";
  const Menu& m = st_.back();
  if (m.sel < (int)m.items.size()) return m.items[m.sel].help;
  return "";
}

// Élément du menu sous la souris (-1 si aucun)
static int itemAt(const Menu& m, int mx, int my) {
  int th = m.title.empty() ? 0 : 13;
  int shown = std::min(m.rows, (int)m.items.size());
  if (mx < m.x || mx >= m.x + m.w) return -1;
  int i = (my - (m.y + 4 + th)) / 12;
  if (my < m.y + 4 + th || i < 0 || i >= shown) return -1;
  return m.top + i;
}

void MenuStack::update(Input& in) {
  if (st_.empty()) return;
  Menu& m = st_.back();
  int n = (int)m.items.size();
  // Souris : survol, clic gauche = Entrée, clic droit = Échap, molette = défiler ou régler
  if (in.mouseOn && n > 0) {
    int at = itemAt(m, in.mx, in.my);
    if (at >= 0 && !m.items[at].header && in.moved && at != m.sel) {
      m.sel = at;
      hover();
    }
    if (in.wheel && at >= 0 && m.items[m.sel].adjust && m.items[m.sel].enabled) {
      auto f = m.items[m.sel].adjust;
      int d = in.wheel > 0 ? 1 : -1;
      in.wheel = 0;
      f(d);
      return;
    }
    if (in.wheel && n > m.rows) {
      m.top = std::clamp(m.top - in.wheel, 0, n - m.rows);
      in.wheel = 0;
    }
    if (in.mclick[0] && at >= 0 && !m.items[at].header) {
      in.mclick[0] = false;
      m.sel = at;
      if (m.items[at].adjust && m.items[at].enabled && !m.items[at].act) {
        auto f = m.items[at].adjust;
        f(+1);
        return;
      }
      in.confirm = true;
    } else if (in.mclick[2]) {
      in.mclick[2] = false;
      if (at >= 0 && m.items[at].adjust && m.items[at].enabled) {
        auto f = m.items[at].adjust;
        f(-1);
        return;
      }
      in.cancel = true;
    }
  }
  if (n > 0) {
    int old = m.sel;
    if (in.press[UP]) {
      m.sel = (m.sel + n - 1) % n;
      skipHeaders(m, -1);
    }
    if (in.press[DOWN]) {
      m.sel = (m.sel + 1) % n;
      skipHeaders(m, 1);
    }
    if ((in.press[LEFT] || in.press[RIGHT]) && m.items[m.sel].adjust && m.items[m.sel].enabled) {
      auto f = m.items[m.sel].adjust;  // copie : le menu peut être reconstruit pendant le réglage
      int d = in.press[RIGHT] ? 1 : -1;
      in.press[LEFT] = in.press[RIGHT] = false;
      f(d);
      return;
    }
    if (in.press[LEFT] && n > m.rows) m.sel = std::max(0, m.sel - m.rows);
    if (in.press[RIGHT] && n > m.rows) m.sel = std::min(n - 1, m.sel + m.rows);
    if (m.sel < m.top) m.top = m.sel;
    if (m.sel > 0 && m.items[m.sel - 1].header && m.sel - 1 < m.top) m.top = m.sel - 1;  // garde le titre visible
    if (m.sel >= m.top + m.rows) m.top = m.sel - m.rows + 1;
    if (old != m.sel) hover();
  }
  if (in.confirm) {
    in.confirm = false;
    if (n > 0 && m.items[m.sel].enabled && m.items[m.sel].act) {
      auto act = m.items[m.sel].act;  // copie : le menu peut être détruit pendant l'action
      act();
    }
    return;
  }
  if (in.cancel) {
    in.cancel = false;
    if (!m.cancelable) return;
    if (m.onCancel) {
      auto f = m.onCancel;
      f();
    } else pop();
  }
}

void MenuStack::draw(Gfx& g, float t) const {
  for (size_t k = 0; k < st_.size(); k++) {
    const Menu& m = st_[k];
    bool top = k + 1 == st_.size();
    int shown = std::min(m.rows, (int)m.items.size());
    int th = m.title.empty() ? 0 : 13;
    int h = shown * 12 + 8 + th;
    g.window(m.x, m.y, m.w, h);
    if (!m.title.empty()) {
      g.text(m.x + 6, m.y + 3, m.title, rgb(0xffd34d));
      g.rect(m.x + 4, m.y + 14, m.w - 8, 1, rgb(0x8090c8, 140));
    }
    for (int i = 0; i < shown; i++) {
      int idx = m.top + i;
      const MenuItem& it = m.items[idx];
      float y = m.y + 4 + th + i * 12;
      if (it.header) {
        g.text(m.x + 6, y, it.label, rgb(0xffd34d));
        g.rect(m.x + 8 + Gfx::textW(it.label), y + 5, m.w - 16 - Gfx::textW(it.label), 1, rgb(0x8090c8, 140));
        continue;
      }
      Color c = it.enabled ? rgb(0xffffff) : rgb(0x8a92b8);
      g.text(m.x + 13, y, it.label, c);
      std::string right = it.rightFn ? it.rightFn() : it.right;
      if (!right.empty()) g.text(m.x + m.w - 6, y, right, it.enabled ? (it.adjust ? rgb(0xffe066) : rgb(0xd8def2)) : rgb(0x8a92b8), 2);
      if (idx == m.sel) {
        if (top) g.cursor(m.x + 4 + (int(t * 4) % 2), y + 1);
        else g.rect(m.x + 4, y + 3, 4, 4, rgb(0xffd34d, 150));
      }
    }
    if (m.top > 0) g.tri(m.x + m.w - 10, m.y + th + 6, m.x + m.w - 6, m.y + th + 6, m.x + m.w - 8, m.y + th + 3, rgb(0xffd34d));
    if (m.top + shown < (int)m.items.size())
      g.tri(m.x + m.w - 10, m.y + h - 6, m.x + m.w - 6, m.y + h - 6, m.x + m.w - 8, m.y + h - 3, rgb(0xffd34d));
    if (!top) {
      Gfx& gg = g;
      gg.rect(m.x, m.y, m.w, h, rgb(0x000010, 70));
    }
  }
}
