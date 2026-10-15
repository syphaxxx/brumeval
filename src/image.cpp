// Lecture des images PNG (stb_image, domaine public : src/ext/stb_image.h, version 2.30).
// Seul ce fichier compile stb_image, avec ses avertissements coupés : le code du jeu reste à 0 avertissement.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "ext/stb_image.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <filesystem>
#include <fstream>
#include <iterator>

#include "gfx.hpp"

bool loadPng(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba) {
  std::ifstream f(std::filesystem::u8path(path), std::ios::binary);  // chemin en UTF-8 (accents)
  if (!f) return false;
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  int n = 0;
  unsigned char* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
  if (!px) return false;
  rgba.assign(px, px + size_t(w) * h * 4);
  stbi_image_free(px);
  return true;
}
