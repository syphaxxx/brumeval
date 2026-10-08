#include "net.hpp"

#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static int lastError() { return WSAGetLastError(); }
static bool wouldBlock(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY; }
static void closeSocket(socket_t s) { closesocket(s); }
static void setNonBlocking(socket_t s) {
  u_long on = 1;
  ioctlsocket(s, FIONBIO, &on);
}
static const int SEND_FLAGS = 0;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static int lastError() { return errno; }
static bool wouldBlock(int e) { return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS || e == EALREADY; }
static void closeSocket(socket_t s) { ::close(s); }
static void setNonBlocking(socket_t s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
#ifdef MSG_NOSIGNAL
static const int SEND_FLAGS = MSG_NOSIGNAL;  // pas de signal SIGPIPE si l'autre a fermé (Linux)
#else
static const int SEND_FLAGS = 0;
#endif
#endif

namespace net {

static const std::uintptr_t NONE = ~std::uintptr_t(0);
static socket_t S(std::uintptr_t s) { return (socket_t)s; }

static void startup() {
#ifdef _WIN32
  static bool done = [] {
    WSADATA d;
    return WSAStartup(MAKEWORD(2, 2), &d) == 0;
  }();
  (void)done;
#endif
}

static void tune(socket_t s) {
  setNonBlocking(s);
  int one = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof one);  // petits messages envoyés tout de suite
#ifdef SO_NOSIGPIPE
  setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&one, sizeof one);  // macOS
#endif
}

// ---------------------------------------------------------------------------
// Connexion
// ---------------------------------------------------------------------------
Conn::Conn(std::uintptr_t s, State st) : sock_(s), state_(st), started_(std::chrono::steady_clock::now()) {}
Conn::~Conn() { close(); }

void Conn::close() {
  if (sock_ != NONE) closeSocket(S(sock_));
  sock_ = NONE;
  state_ = Closed;
}
void Conn::fail(const std::string& why) {
  if (state_ != Closed && error.empty()) error = why;
  close();
}

std::unique_ptr<Conn> Conn::connect(const std::string& address, int port, std::string& err) {
  startup();
  addrinfo hints{}, *res = nullptr;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
    err = "Adresse introuvable : « " + address + " ».";
    return nullptr;
  }
  // IPv4 d'abord (Tailscale, Radmin VPN, réseau local)
  addrinfo* pick = res;
  for (addrinfo* a = res; a; a = a->ai_next)
    if (a->ai_family == AF_INET) {
      pick = a;
      break;
    }
  socket_t s = socket(pick->ai_family, pick->ai_socktype, pick->ai_protocol);
  if (s == (socket_t)NONE) {
    freeaddrinfo(res);
    err = "Impossible de créer la connexion.";
    return nullptr;
  }
  tune(s);
  int r = ::connect(s, pick->ai_addr, (int)pick->ai_addrlen);
  freeaddrinfo(res);
  if (r != 0 && !wouldBlock(lastError())) {
    closeSocket(s);
    err = "Connexion refusée par " + address + ".";
    return nullptr;
  }
  return std::unique_ptr<Conn>(new Conn((std::uintptr_t)s, r == 0 ? Connected : Connecting));
}

void Conn::send(const Json& msg) {
  if (state_ == Closed) return;
  std::string body = msg.dump();
  uint32_t n = (uint32_t)body.size();
  char head[4] = {char(n >> 24), char(n >> 16), char(n >> 8), char(n)};
  out_.append(head, 4);
  out_ += body;
  if (state_ == Connected) flush();
}

void Conn::flush() {
  while (!out_.empty() && state_ == Connected) {
    int n = ::send(S(sock_), out_.data(), (int)std::min<size_t>(out_.size(), 1 << 16), SEND_FLAGS);
    if (n > 0) out_.erase(0, (size_t)n);
    else {
      if (!wouldBlock(lastError())) fail("La connexion avec l'autre joueur est coupée.");
      return;
    }
  }
}

std::vector<Json> Conn::poll() {
  std::vector<Json> got;
  if (state_ == Connecting) {
    fd_set w, e;
    FD_ZERO(&w);
    FD_ZERO(&e);
    FD_SET(S(sock_), &w);
    FD_SET(S(sock_), &e);
    timeval tv{0, 0};
    int r = select((int)S(sock_) + 1, nullptr, &w, &e, &tv);
    if (r > 0) {
      int so = 0;
      socklen_t len = sizeof so;
      getsockopt(S(sock_), SOL_SOCKET, SO_ERROR, (char*)&so, &len);
      if (so != 0 || FD_ISSET(S(sock_), &e)) fail("Personne ne répond à cette adresse (l'hôte attend-il ? le pare-feu l'autorise-t-il ?).");
      else state_ = Connected;
    } else if (std::chrono::steady_clock::now() - started_ > std::chrono::seconds(8))
      fail("Délai dépassé : personne ne répond à cette adresse.");
    if (state_ != Connected) return got;
  }
  if (state_ != Connected) return got;
  flush();
  char buf[8192];
  for (;;) {
    int n = ::recv(S(sock_), buf, (int)sizeof buf, 0);
    if (n > 0) in_.append(buf, (size_t)n);
    else if (n == 0) {
      fail("L'autre joueur s'est déconnecté.");
      break;
    } else {
      if (!wouldBlock(lastError())) fail("La connexion avec l'autre joueur est coupée.");
      break;
    }
  }
  // Découpe en messages : 4 octets de longueur, puis le JSON
  while (in_.size() >= 4) {
    uint32_t n = uint32_t((unsigned char)in_[0]) << 24 | uint32_t((unsigned char)in_[1]) << 16 | uint32_t((unsigned char)in_[2]) << 8 |
                 uint32_t((unsigned char)in_[3]);
    if (n > (1u << 22)) {
      fail("Message reçu invalide.");
      break;
    }
    if (in_.size() < 4 + (size_t)n) break;
    try {
      got.push_back(Json::parse(in_.substr(4, n)));
    } catch (std::exception&) {
    }
    in_.erase(0, 4 + (size_t)n);
  }
  return got;
}

// ---------------------------------------------------------------------------
// Hôte
// ---------------------------------------------------------------------------
Server::~Server() { close(); }
void Server::close() {
  if (sock_ != NONE) closeSocket(S(sock_));
  sock_ = NONE;
}
bool Server::open() const { return sock_ != NONE; }

bool Server::listen(int port, std::string& err, bool loopback) {
  startup();
  close();
  int one = 1, zero = 0;
  // IPv6 et IPv4 à la fois si possible, sinon IPv4 seul
  socket_t s = loopback ? (socket_t)NONE : socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  if (s != (socket_t)NONE) {
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&zero, sizeof zero);
#ifndef _WIN32
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof one);
#endif
    sockaddr_in6 a{};
    a.sin6_family = AF_INET6;
    a.sin6_addr = in6addr_any;
    a.sin6_port = htons((unsigned short)port);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) {
      closeSocket(s);
      s = (socket_t)NONE;
    }
  }
  if (s == (socket_t)NONE) {
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == (socket_t)NONE) {
      err = "Impossible d'ouvrir le réseau.";
      return false;
    }
#ifndef _WIN32
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof one);
#endif
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(loopback ? INADDR_LOOPBACK : INADDR_ANY);
    a.sin_port = htons((unsigned short)port);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) {
      closeSocket(s);
      err = "Le port " + std::to_string(port) + " est déjà utilisé (une autre partie est-elle ouverte ?).";
      return false;
    }
  }
  (void)one;
  if (::listen(s, 4) != 0) {
    closeSocket(s);
    err = "Impossible d'attendre des joueurs sur le port " + std::to_string(port) + ".";
    return false;
  }
  setNonBlocking(s);
  sock_ = (std::uintptr_t)s;
  return true;
}

std::unique_ptr<Conn> Server::accept() {
  if (sock_ == NONE) return nullptr;
  socket_t c = ::accept(S(sock_), nullptr, nullptr);
  if (c == (socket_t)NONE) return nullptr;
  tune(c);
  return std::unique_ptr<Conn>(new Conn((std::uintptr_t)c, Conn::Connected));
}

// ---------------------------------------------------------------------------
// Adresses de cet ordinateur
// ---------------------------------------------------------------------------
std::vector<std::string> localAddresses() {
  startup();
  std::vector<std::string> out;
  auto add = [&](const sockaddr* sa) {
    char s[64] = {0};
    if (sa->sa_family != AF_INET) return;
    inet_ntop(AF_INET, &((const sockaddr_in*)sa)->sin_addr, s, sizeof s);
    std::string a = s;
    if (a.empty() || a.rfind("127.", 0) == 0 || a.rfind("169.254.", 0) == 0) return;
    for (auto& o : out)
      if (o == a) return;
    out.push_back(a);
  };
#ifdef _WIN32
  char name[256] = {0};
  if (gethostname(name, sizeof name) == 0) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    if (getaddrinfo(name, nullptr, &hints, &res) == 0) {
      for (addrinfo* a = res; a; a = a->ai_next) add(a->ai_addr);
      freeaddrinfo(res);
    }
  }
#else
  ifaddrs* ifs = nullptr;
  if (getifaddrs(&ifs) == 0) {
    for (ifaddrs* i = ifs; i; i = i->ifa_next)
      if (i->ifa_addr) add(i->ifa_addr);
    freeifaddrs(ifs);
  }
#endif
  return out;
}

}  // namespace net
