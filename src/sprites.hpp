// Dessin des personnages, créatures, tuiles et décors (tout est dessiné en code,
// aucune image externe n'est nécessaire).
#pragma once
#include <string>

#include "data.hpp"
#include "gfx.hpp"
#include "world.hpp"

// Créature centrée sur (x, y). flip = tournée vers la droite.
void drawCreature(Gfx& g, const std::string& sp, float x, float y, float s, bool flip, float t);
// Humain : (x, y) = coin haut-gauche d'une case de 16x16, mise à l'échelle par s.
// dir : 0 haut, 1 bas, 2 gauche, 3 droite. step : 0, 1 ou 2 (pas de marche).
void drawHuman(Gfx& g, const Look& lk, float x, float y, float s, int dir, int step, bool weapon);
// Combattant en combat (humain ou créature), centré sur (x, y)
void drawFighterSprite(Gfx& g, const Fighter& f, float x, float y, float s, bool faceRight, float t);

void drawTile(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy, float t);
void drawBuilding(Gfx& g, const Building& b, int sx, int sy);
void drawChest(Gfx& g, int sx, int sy, bool open);
void drawSign(Gfx& g, int sx, int sy);
