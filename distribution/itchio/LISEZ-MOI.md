# Page itch.io de Brumeval

Tout ce qu'il faut pour mettre le jeu sur [itch.io](https://itch.io) (gratuit).
Le compte et l'envoi des fichiers se font à la main : itch.io demande de se
connecter avec ton propre compte.

## 1. Créer la page

1. Crée un compte sur itch.io, puis **Dashboard > Create new project**.
2. **Title** : `Brumeval`. **Project URL** : `brumeval`.
3. **Short description** (une ligne) : reprendre « Accroche » ci-dessous.
4. **Classification** : *Games*. **Kind of project** : *Downloadable*.
5. **Pricing** : *No payments* (ou *Donations* si tu veux accepter des dons).
6. **Uploads** : envoyer les trois paquets de la dernière version GitHub
   (`Brumeval-X.Y-windows.zip`, `-mac.zip`, `-linux.tar.gz`) et cocher le
   système de chacun (Windows, macOS, Linux).
7. **Description** : copier le texte français (ou anglais) ci-dessous.
8. **Genre** : *Role Playing*. **Tags** : `jrpg`, `turn-based`, `pixel-art`,
   `creature-collector`, `atb`, `singleplayer`, `multiplayer`, `roguelite`.
9. **Screenshots** : les images du dossier `captures/` (dans l'ordre des
   numéros). **Cover image** : `1_titre.png` (itch.io la recadre en 630×500).
10. **Visibility** : *Draft* pour vérifier, puis *Public*.

À chaque nouvelle version : envoyer les nouveaux paquets dans *Uploads* (ou
utiliser l'outil `butler` d'itch.io), et ajouter un *Devlog* avec le texte de
`distribution/notes.md`.

## 2. Accroche

> Un petit RPG au tour par tour actif : capturez des créatures, brisez les boucliers des boss et sauvez la vallée de Brumeval.

*(anglais : A small active-time-battle RPG: catch creatures, break boss shields and save the valley of Brumeval.)*

## 3. Description (français)

**Brumeval — Les gardiens de la vallée**

Une brume étrange rend les créatures sauvages. Lior et ses compagnons partent
apaiser les gardiens de la vallée… puis traversent le lac jusqu'à l'archipel
d'Écume, où un orage millénaire se réveille.

- **Combats au tour par tour actif** (jauges ATB, comme Final Fantasy) sur un
  **échiquier commun** : on avance, on passe dans le camp adverse, on frappe
  dans le dos, on bloque le passage ; zones, obstacles, attaques annoncées des boss.
- **Brisure** : frappez les faiblesses pour briser le bouclier des ennemis.
- **Créatures à capturer**, avec talents, rôles et évolutions ; chaque héros
  combat avec sa créature, et ils lancent des **techniques en duo**.
- **Pousser, tirer, bondir, échanger**, **météo** et **cases de terrain** sur le plateau ;
  **défis de l'échiquier** pour s'entraîner.
- **Gemmes** à sertir dans l'équipement, **forge**, **tactiques** pour le mode
  automatique.
- **Boss en plusieurs phases**, **énigmes** de rochers, quêtes annexes, deux actes.
- **Mode Expédition** (roguelite) : un monde généré à partir d'une graine.
- **Multijoueur** : expédition à plusieurs (jusqu'à 4), duels et échanges.
- Éditeurs intégrés (cartes, histoire, règles) : tout le contenu est modifiable.

Fait avec C++ et SDL2. Windows, Mac et Linux. Gratuit.

## 4. Description (anglais)

**Brumeval — The Valley's Guardians**

A strange mist is turning creatures wild. Lior and friends set out to calm the
valley's guardians… then sail across the lake to the Foam Archipelago, where a
thousand-year-old storm is waking up.

- **Active-time battles** (ATB gauges, Final Fantasy style) on a **shared
  chessboard**: advance, cross into the enemy side, strike from behind, block
  the way; area attacks, obstacles, telegraphed boss attacks.
- **Break**: hit weaknesses to shatter enemy shields.
- **Catch creatures** with talents, roles and evolutions; every hero fights
  alongside a creature, and they unleash **dual techniques**.
- **Push, pull, leap, swap**, **weather** and **terrain tiles** on the board;
  **chessboard challenges** to practise.
- **Gems** to socket in your gear, a **forge**, and **tactics** for auto-battle.
- **Multi-phase bosses**, boulder **puzzles**, side quests, two acts.
- **Expedition mode** (roguelite): a whole world generated from a seed.
- **Multiplayer**: co-op expedition (up to 4), duels and trades.
- Built-in editors (maps, story, rules): all content is moddable.

*Play in English or French (Options > Language).*

Made with C++ and SDL2. Windows, Mac and Linux. Free.
