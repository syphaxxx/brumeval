#include "i18n.hpp"
#include "upnp.hpp"

#include "net.hpp"

#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>

#include <cctype>
#include <mutex>
#include <thread>

namespace upnp {

static std::mutex mu;
static Result result;
static std::thread worker;
static int mappedPort = 0;  // port ouvert sur la box (0 : aucun)
static std::string controlURL, serviceType;

static void set(const Result& r) {
  std::lock_guard<std::mutex> l(mu);
  result = r;
}

static void search(int port) {
  Result r;
  int err = 0;
  UPNPDev* devs = upnpDiscover(2000, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &err);
  if (!devs) {
    r.done = true;
    r.error = T("Aucune box trouvée qui accepte l'ouverture automatique (UPnP désactivé ?).");
    return set(r);
  }
  UPNPUrls urls;
  IGDdatas data;
  char lan[64] = "", wan[64] = "";
  int igd = UPNP_GetValidIGD(devs, &urls, &data, lan, sizeof lan, wan, sizeof wan);
  freeUPNPDevlist(devs);
  r.local = lan;
  if (igd == UPNP_NO_IGD || igd == UPNP_UNKNOWN_DEVICE) {
    if (igd != UPNP_NO_IGD) FreeUPNPUrls(&urls);
    r.done = true;
    r.error = T("La box ne propose pas l'ouverture automatique des ports (UPnP).");
    return set(r);
  }
  char ext[64] = "";
  UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, ext);
  r.external = ext[0] ? ext : wan;
  std::string p = std::to_string(port);
  int add = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, p.c_str(), p.c_str(), lan, "Brumeval", "TCP", nullptr, "0");
  if (add != UPNPCOMMAND_SUCCESS)  // certaines box refusent une ouverture sans durée : 2 heures
    add = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, p.c_str(), p.c_str(), lan, "Brumeval", "TCP", nullptr, "7200");
  r.done = true;
  if (add != UPNPCOMMAND_SUCCESS) {
    r.error = std::string(T("La box a refusé d'ouvrir le port ")) + p + " (" + strupnperror(add) + ").";
  } else {
    {
      std::lock_guard<std::mutex> l(mu);
      mappedPort = port;
      controlURL = urls.controlURL;
      serviceType = data.first.servicetype;
    }
    if (igd == UPNP_PRIVATEIP_IGD || !net::isPublicIPv4(r.external))
      r.error = T("Port ouvert, mais la box n'a pas d'adresse Internet à elle (") + (r.external.empty() ? std::string("inconnue") : r.external) +
                T(") : votre fournisseur la partage entre plusieurs abonnés.");
    else
      r.ok = true;
  }
  FreeUPNPUrls(&urls);
  set(r);
}

void open(int port) {
  close();
  net::startup();
  set(Result{});
  worker = std::thread(search, port);
}

Result state() {
  std::lock_guard<std::mutex> l(mu);
  return result;
}

void close() {
  if (worker.joinable()) worker.join();  // au plus ~2 s si la recherche est en cours
  std::string url, svc;
  int port;
  {
    std::lock_guard<std::mutex> l(mu);
    port = mappedPort, url = controlURL, svc = serviceType;
    mappedPort = 0;
    result = Result{};
  }
  if (port) UPNP_DeletePortMapping(url.c_str(), svc.c_str(), std::to_string(port).c_str(), "TCP", nullptr);
}

}  // namespace upnp

namespace net {

static const char ALPHABET[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";  // Crockford : sans I, L, O, U

static bool parseIPv4(const std::string& s, unsigned char out[4]) {
  int part = 0, digits = 0;
  unsigned v = 0;
  for (size_t i = 0; i <= s.size(); i++) {
    char c = i < s.size() ? s[i] : '.';
    if (c >= '0' && c <= '9' && digits < 3) {
      v = v * 10 + unsigned(c - '0'), digits++;
    } else if (c == '.' && digits > 0 && v <= 255 && part < 4) {
      out[part++] = (unsigned char)v, v = 0, digits = 0;
    } else
      return false;
  }
  return part == 4;
}
static unsigned char check(const unsigned char b[4]) { return (unsigned char)((b[0] * 7 + b[1] * 11 + b[2] * 13 + b[3] * 17 + 0x5A) & 0xFF); }

std::string gameCode(const std::string& ipv4) {
  unsigned char b[4];
  if (!parseIPv4(ipv4, b)) return "";
  unsigned long long v = 0;  // 40 bits : l'adresse puis le contrôle
  for (unsigned char x : b) v = v << 8 | x;
  v = v << 8 | check(b);
  std::string s;
  for (int i = 7; i >= 0; i--) s += ALPHABET[(v >> (i * 5)) & 31];
  return s.substr(0, 4) + "-" + s.substr(4);
}

bool decodeGameCode(const std::string& text, std::string& ipv4) {
  std::string s;
  for (char c : text) {
    c = (char)std::toupper((unsigned char)c);
    if (c == '-' || c == ' ') continue;
    if (c == 'O') c = '0';  // lettres faciles à confondre
    if (c == 'I' || c == 'L') c = '1';
    s += c;
  }
  if (s.size() != 8) return false;
  unsigned long long v = 0;
  for (char c : s) {
    const char* p = nullptr;
    for (const char* a = ALPHABET; *a; a++)
      if (*a == c) p = a;
    if (!p) return false;
    v = v << 5 | (unsigned long long)(p - ALPHABET);
  }
  unsigned char b[4] = {(unsigned char)(v >> 32), (unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8)};
  if (check(b) != (unsigned char)(v & 0xFF)) return false;  // faute de frappe
  ipv4 = std::to_string(b[0]) + "." + std::to_string(b[1]) + "." + std::to_string(b[2]) + "." + std::to_string(b[3]);
  return true;
}

bool isPublicIPv4(const std::string& ipv4) {
  unsigned char b[4];
  if (!parseIPv4(ipv4, b)) return false;
  if (b[0] == 0 || b[0] == 10 || b[0] == 127 || b[0] >= 224) return false;
  if (b[0] == 100 && b[1] >= 64 && b[1] < 128) return false;  // adresse partagée par le fournisseur (CGNAT)
  if (b[0] == 169 && b[1] == 254) return false;
  if (b[0] == 172 && b[1] >= 16 && b[1] < 32) return false;
  if (b[0] == 192 && b[1] == 168) return false;
  return true;
}

}  // namespace net
