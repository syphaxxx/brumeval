// Dessin des personnages, créatures, tuiles et décors : avec les sprites de l'atlas (data/sprites.png, voir gfx.hpp)
// quand ils existent, sinon en code (formes, couleurs).
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
// Hauteur du dessin d'un combattant au-dessus de son point (x, y) de drawFighterSprite (barre de PV, repères)
float fighterTop(const Fighter& f, float s);

void drawTile(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy, float t);
// Objet haut posé sur une case (arbre, réverbère, puits, étal…) : dessiné avec les personnages, triés de haut en
// bas, pour passer devant ou derrière eux. Faux si la case n'en a pas (drawTile dessine alors tout).
bool hasProp(const MapDef& m, int tx, int ty);
void drawProp(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy, float t);
// Le même objet réduit pour tenir dans une case (palette de l'éditeur de cartes)
void drawPropIcon(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy);
void drawBuilding(Gfx& g, const Building& b, int sx, int sy);
// Par-dessus la nuit (2.6) : fenêtres allumées et lueur de la porte (night : 0 jour, 1 nuit) ; fumée de la cheminée
void drawBuildingLights(Gfx& g, const Building& b, int sx, int sy, float night);
void drawChimneySmoke(Gfx& g, const Building& b, int sx, int sy, float t);
void drawChest(Gfx& g, int sx, int sy, bool open);
void drawSign(Gfx& g, int sx, int sy);
