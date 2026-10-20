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
void drawHuman(Gfx& g, const Look& lk, float x, float y, float s, int dir, int step, bool weapon, bool run = false);
// Combattant en combat (humain ou créature), centré sur (x, y)
void drawFighterSprite(Gfx& g, const Fighter& f, float x, float y, float s, bool faceRight, float t);
// Hauteur du dessin d'un combattant au-dessus de son point (x, y) de drawFighterSprite (barre de PV, repères)
float fighterTop(const Fighter& f, float s);
// Échelle pour qu'une créature tienne dans maxH pixels de haut : 2 si son sprite y tient deux fois plus grand, sinon 1
// ou moins (fiches du Bestiaire et de l'équipe)
float creatureFit(const std::string& id, float maxH, float maxW = 0);
// Petit portrait (3.3, ordre des tours) : la tête d'un humain, la créature entière réduite, centrée dans w x h.
// Faux s'il n'a pas de sprite.
bool drawFighterIcon(Gfx& g, const Fighter& f, float cx, float cy, float w, float h, bool faceRight);
// Pose d'un héros qui en a les sprites (3.3, Lior) : <sprite>_<pose> à la place de son dessin de combat (« lancer_2 »,
// « victoire », « ko »), au même endroit que drawFighterSprite. Faux s'il n'a pas cette pose.
bool drawFighterPose(Gfx& g, const Fighter& f, const std::string& pose, float x, float y, bool faceRight);

void drawTile(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy, float t);
// Objet haut posé sur une case (arbre, réverbère, puits, étal…) : dessiné avec les personnages, triés de haut en
// bas, pour passer devant ou derrière eux. Faux si la case n'en a pas (drawTile dessine alors tout).
bool hasProp(const MapDef& m, int tx, int ty);
// Hautes herbes : dessinées (faux s'il n'y en a pas) ; front : le bas des touffes, par-dessus un personnage
bool tallGrass(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy, float t, bool front);
void drawProp(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy, float t);
// Petite vie de la carte (2026-10-20) : papillons au-dessus des fleurs le jour, poissons qui sautent dans l'eau
// (cases de tx0..tx1, ty0..ty1 ; dessin seul, rien d'enregistré)
void drawMapCritters(Gfx& g, const MapDef& m, int tx0, int tx1, int ty0, int ty1, int camX, int camY, float t, float night);
// Le même objet réduit pour tenir dans une case (palette de l'éditeur de cartes)
void drawPropIcon(Gfx& g, const MapDef& m, int tx, int ty, int sx, int sy);
void drawBuilding(Gfx& g, const Building& b, int sx, int sy);
// Par-dessus la nuit (2.6) : fenêtres allumées et lueur de la porte (night : 0 jour, 1 nuit) ; fumée de la cheminée
void drawBuildingLights(Gfx& g, const Building& b, int sx, int sy, float night);
void drawChimneySmoke(Gfx& g, const Building& b, int sx, int sy, float t);
void drawChest(Gfx& g, int sx, int sy, bool open);
void drawSign(Gfx& g, int sx, int sy);
