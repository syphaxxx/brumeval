// Langue du jeu (3.1) : le jeu est écrit en français ; data/langues/<langue>.json donne la traduction de
// chaque phrase française (« Équipe » → « Team »). T("…") renvoie la traduction de la phrase si la langue
// choisie n'est pas le français et que la phrase est traduite, sinon la phrase française elle-même : une
// phrase oubliée reste lisible, en français.
//
// Les textes des données (noms, descriptions, dialogues) passent par tr() au moment où le jeu les utilise :
// les fichiers de data/ restent en français (les éditeurs les réécrivent tels quels).
#pragma once
#include <string>
#include <vector>

void setLanguage(const std::string& id);  // "fr" ou "en" ; charge data/langues/<id>.json
const std::string& language();
bool translating();                       // une autre langue que le français est choisie
// Traduction d'une phrase (le pointeur reste valable tant qu'on ne change pas de langue)
const char* T(const char* fr);
std::string tr(const std::string& fr);
// Phrases demandées mais absentes du dictionnaire (pour compléter la traduction, mode test)
std::vector<std::string> missingTranslations();
