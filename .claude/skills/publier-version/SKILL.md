---
name: publier-version
description: "Brumeval : publier une nouvelle version pour les joueurs (numéro, notes, étiquette, page GitHub, annonce Discord). À utiliser quand l'utilisateur demande de publier."
---

# Publier une version de Brumeval

`.github/workflows/compilation.yml` compile le jeu sous Windows (MSVC), Mac
(application universelle `Brumeval.app`, macOS 11+) et Linux (Ubuntu 22.04) à
chaque envoi sur GitHub, lance le mode test sur les trois, et fabrique les
paquets (exécutable + `data/` + `distribution/LISEZ-MOI.txt`). Pour publier :
augmenter `BRUMEVAL_VERSION` dans `src/version.hpp`, fusionner dans `main`,
puis `git tag v1.2` et `git push origin v1.2` : la page Releases est créée avec
le texte de `distribution/notes.md`, puis la version est annoncée sur le canal Discord de l'utilisateur (webhook rangé dans le secret GitHub `DISCORD_WEBHOOK`, jamais dans le code : le dépôt est public ; texte = première ligne du message de l'étiquette, donc écrire `git tag -a v1.6 -m "Version 1.6 : …"` ; la section « Nouveautés » de `notes.md` est à réécrire à chaque version). Sous Windows, la bibliothèque C++ est
intégrée (`CMAKE_MSVC_RUNTIME_LIBRARY`, `SDL_FORCE_STATIC_VCRT`) ; sous Linux,
`-static-libstdc++`. Sur Mac, `data/` va dans `Contents/Resources` (trouvé par
`SDL_GetBasePath`). Pour essayer une version Release ici : `cmake -S . -B
build-release` puis `cmake --build build-release --config Release`.


Vérifier ensuite dans GitHub Actions que les étapes « Publier les paquets » et « Annoncer la version sur Discord » ont réussi.
