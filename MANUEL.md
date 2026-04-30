# ZC95 Lua Builder — Manuel d'utilisation

Application de bureau Qt 6 / C++ pour créer, simuler et déboguer des scripts Lua
pour le ZC95.

---

## Table des matières

1. [Vue d'ensemble](#1-vue-densemble)
2. [Premier lancement](#2-premier-lancement)
3. [Tour de l'interface](#3-tour-de-linterface)
4. [Workflow type](#4-workflow-type)
5. [Référence : onglets de gauche](#5-référence--onglets-de-gauche)
6. [Référence : zone éditeur / simulateur](#6-référence--zone-éditeur--simulateur)
7. [Référence : panneau Issues (linter)](#7-référence--panneau-issues-linter)
8. [Smart merge : régénération sans perte](#8-smart-merge--régénération-sans-perte)
9. [Simulateur Lua embarqué](#9-simulateur-lua-embarqué)
10. [Raccourcis clavier](#10-raccourcis-clavier)
11. [Dépannage / FAQ](#11-dépannage--faq)
12. [Limitations connues](#12-limitations-connues)

---

## 1. Vue d'ensemble

L'app permet trois choses :

1. **Construire un script Lua ZC95** via une interface visuelle (formulaires +
   éditeur). Aller-retour bidirectionnel : la form génère le code, le code
   regénère la form.
2. **Lancer le linter** pour détecter les incohérences entre la config et le
   code (channels hors plages, mode audio incohérent, idiomes Lua 5.1 cassés
   en 5.4, etc.).
3. **Simuler le script** via un interpréteur Lua 5.4 embarqué qui enregistre
   tous les appels `zc.*` et les affiche sur une timeline 4 canaux.

L'app gère 12 scripts officiels (presets pré-chargés), 9 patterns idiomatiques
en snippets, et un panneau d'aide API complet.

---

## 2. Premier lancement

### Pré-requis

- Qt 6.2+ (`Widgets`, `Test` si vous voulez compiler les tests)
- CMake 3.16+
- Compilateur C/C++17 (MSVC 2019+ ou MinGW)
- Connexion internet au premier `cmake -S . -B build` (Lua 5.4 est téléchargé
  via FetchContent)

### Build (Windows / MSVC)

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
build\zc95-lua-builder.exe
```

### Build (Windows / MinGW)

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64
cmake --build build
```

### Lancer les tests

```
ctest --test-dir build --output-on-failure
```

ou directement :

```
build\test_roundtrip.exe
```

Au démarrage, l'app charge un script vide (preset par défaut). Si vous aviez
ouvert un fichier la dernière fois, sa position est restaurée via QSettings.

---

## 3. Tour de l'interface

```
┌────────────────────────────────────────────────────────────────────┐
│ File   Edit   Generate   Simulator   Presets   View   Help         │ Menu
├────────────────────────────────────────────────────────────────────┤
│ [New] [Open] [Save] | [Regenerate] [Reparse]                       │ Toolbar
├──────────────────────┬─────────────────────────────────────────────┤
│ Tabs gauche :        │ Tabs droite :  [Editor] [Simulator]         │
│  • Config            │ ┌─────────────────────────────────────────┐ │
│  • Menu Items        │ │ Find/Replace bar (cachée par défaut)    │ │
│  • Functions         │ ├─────────────────────────────────────────┤ │
│  • Snippets          │ │                                         │ │
│  • API Help          │ │  Éditeur Lua (lignes, highlight,        │ │
│  • LCD Preview       │ │  auto-complétion)                       │ │
│                      │ │                                         │ │
│                      │ └─────────────────────────────────────────┘ │
├──────────────────────┴─────────────────────────────────────────────┤
│ Issues panel (dock — toggle via View → Issues)                     │
├────────────────────────────────────────────────────────────────────┤
│ Status bar : messages temporaires        ● in sync  /  ● differ    │
└────────────────────────────────────────────────────────────────────┘
```

---

## 4. Workflow type

### Démarrer d'un script vierge

1. **File → New** (Ctrl+N).
2. Onglet **Config** : nommer le pattern, choisir audio mode, fréquence de
   loop, etc.
3. Onglet **Menu Items** : ajouter les paramètres réglables (vitesse, durée,
   mode...). Donner un ID unique à chacun.
4. Onglet **Functions** : cocher les callbacks utiles (au minimum `Loop`).
5. **Ctrl+G** (smart merge) → l'éditeur reçoit le squelette.
6. Compléter la logique métier dans l'éditeur (les snippets et l'API Help à
   gauche aident).
7. **Ctrl+L** → linter. Corriger les erreurs/warnings.
8. **Ctrl+R** → charger dans le simulateur. **Run**, observer la timeline.
9. **File → Save As…** → fichier `.lua`.
10. Upload sur le ZC95 (cf. RemoteAccess.md du firmware).

### Partir d'un script officiel

1. **Presets → Official scripts → Intense** (par exemple).
2. Le code est chargé, la form se remplit automatiquement.
3. Modifier ce qu'on veut → **Ctrl+G** pour ré-injecter les changements de
   form dans le code (sans perdre la logique).
4. Save As pour ne pas écraser le preset original.

### Repartir d'un fichier existant

- **File → Open…** ou drag & drop d'un `.lua` sur la fenêtre.
- Ou **File → Recent Files** pour les 8 derniers.

---

## 5. Référence : onglets de gauche

### Config

Tous les champs du bloc `Config = {...}` sauf `menu_items` :

| Champ                       | Lua équivalent                       |
|-----------------------------|--------------------------------------|
| Name                        | `name = "..."`                       |
| Audio mode                  | `audio_processing_mode = "..."`      |
| Soft button label           | `soft_button = "..."`                |
| Loop freq Hz                | `loop_freq_hz = N`                   |
| Allow triphase              | `allow_triphase = true`              |
| BT remote passthrough       | `bluetooth_remote_passthrough = true`|

Modifier un champ marque le badge de status comme **"form/code differ"** —
appuyer sur Ctrl+G pour synchroniser.

### Menu Items

Liste des paramètres exposés sur l'écran LCD du ZC95. Trois types :

- **MIN_MAX** : valeur numérique ajustable (min, max, step, unité, défaut).
- **MULTI_CHOICE** : liste de choix (chacun avec un ID et une description).
- **AUDIO_VIEW_INTENSITY_STEREO / MONO** : vue temps réel du niveau audio.

Boutons : `Add` / `Edit` / `Duplicate` / `Delete`. L'ID est validé en direct
(refusé si en doublon).

### Functions

Cases à cocher pour générer (ou ajouter) les stubs Lua :

| Checkbox                  | Fonction Lua                         |
|---------------------------|--------------------------------------|
| Setup                     | `function Setup()`                   |
| Loop ⚠ obligatoire        | `function Loop(time_ms)`             |
| MinMaxChange              | `function MinMaxChange(menu_id, val)`|
| MultiChoiceChange         | `function MultiChoiceChange(menu_id, choice_id)` |
| SoftButton                | `function SoftButton(pushed)`        |
| ExternalTrigger           | `function ExternalTrigger(socket, part, active)` |
| BluetoothRemoteKeypress   | `function BluetoothRemoteKeypress(key)` |
| BluetoothHidEvent         | `function BluetoothHidEvent(usage_page, usage, value)` |
| AudioIntensityChange      | `function AudioIntensityChange(left, right, virt)` |

**Cocher** une fonction → `Ctrl+G` ajoute le stub.
**Décocher** → la fonction reste dans l'éditeur (l'utilisateur la supprime
manuellement s'il le souhaite). Le smart merge ne supprime jamais.

### Snippets

Deux groupes :

- **zc.\* API** : un bouton par fonction (`ChannelOn`, `SetPower`, etc.).
- **Patterns idiomatiques** : init des 4 channels, toggle every N ms, tick
  244 Hz, modulateurs triangle/sinus, enums Mode/MenuId, triphase fade cycle,
  burst loop, helpers SetFreq/SetWidth.

Clic = insertion à la position du curseur dans l'éditeur. Le tooltip montre
le code complet.

### API Help

Documentation intégrée de toute l'API `zc.*` :

- Liste à gauche, doc + exemple à droite.
- Bouton **Insert example** → injecte l'exemple à la position du curseur.

### LCD Preview

Aperçu fidèle de l'affichage LCD ZC95 :

- Titre du pattern.
- Soft button label (en bas).
- Items du menu : barre graduée pour MIN_MAX, `< choice >` pour MULTI_CHOICE,
  pseudo-waveform pour les vues audio.

Se met à jour en direct quand vous modifiez la form.

---

## 6. Référence : zone éditeur / simulateur

### Éditeur Lua

- **Numéros de ligne** dans la marge gauche, ligne courante surlignée.
- **Coloration syntaxique** : mots-clés Lua, `zc.*`, callbacks, chaînes,
  nombres, commentaires `--` et `--[[...]]`.
- **Auto-complétion** :
  - `Ctrl+Space` ouvre la liste manuellement.
  - Auto-déclenchée après 2 caractères.
  - Suggestions : `zc.*`, callbacks, mots-clés Lua, constantes (`true`, `nil`).

### Find / Replace

- **Ctrl+F** ouvre la barre en mode Find seulement.
- **Ctrl+H** ouvre la barre en mode Find + Replace.
- Options : sensible à la casse, mots entiers, wrap-around (boucle).
- F3 / Shift+F3 : next / previous.
- Echap ferme la barre.

### Onglet Simulator

Voir [section 9](#9-simulateur-lua-embarqué).

---

## 7. Référence : panneau Issues (linter)

Lancement : **Ctrl+L** ou **Generate → Lint**.

Trois sévérités, code couleur :

| Sévérité  | Couleur | Quand                                        |
|-----------|---------|----------------------------------------------|
| **Info**  | bleu    | Dépendances externes (`require("ettot")`)    |
| **Warn**  | orange  | Idiomes Lua 5.1 (`module()`), warnings de form |
| **Error** | rouge   | Plages dépassées, IDs en double, triphase sans flag |

**Vérifications appliquées :**

- IDs de menu en doublon ou références inconnues à `menu_id`.
- Mode audio incohérent : `AudioIntensityChange` activé mais audio off, etc.
- Usage de `zc.LinkChannels` / `zc.EnableTriphase` sans `allow_triphase = true`.
- Plages : `SetPower` (0-1000), `SetFrequency` (1-300 Hz), `SetPulseWidth`
  (0-255 us), `ChannelPulseMs` (≥ 0), `LinkChannels` offset (0-100%),
  `AccIoWrite` line (1-3), `DelayMs` (0-10000).
- Numéros de canal hors `[1..4]`.
- `module()` / `package.seeall` : Lua 5.1, supprimés en 5.4 (le simulateur
  embarqué refusera de charger ces scripts).
- `require("xxx")` : info que la lib est fournie par le firmware ZC95 mais
  pas par le simulateur.
- Absence de `function Loop(time_ms)` (warning).

**Double-clic sur une issue** = saut à la ligne dans l'éditeur (et bascule sur
l'onglet Editor automatiquement).

---

## 8. Smart merge : régénération sans perte

C'est le mécanisme par défaut de `Ctrl+G`.

### Comportement

`Ctrl+G` ne touche que :

1. **Le bloc `Config = {...}`** — entièrement remplacé par celui généré
   depuis la form actuelle.
2. **Les fonctions nouvellement activées** dans l'onglet Functions — leurs
   stubs sont ajoutés à la fin du fichier.

`Ctrl+G` **ne touche jamais** :

- Le corps des fonctions déjà présentes dans l'éditeur.
- Les variables globales (`_freq_hz = 100`, etc.).
- Les commentaires en dehors du bloc Config.
- L'ordre des fonctions ou la structure générale.

Le curseur reste à sa position approximative après le merge.

### Quand utiliser "Regenerate from scratch"

Menu **Generate → Regenerate code from scratch** (`Ctrl+Shift+Alt+G`).

Utile dans deux cas :

- Vous voulez repartir des stubs propres et abandonner la logique actuelle.
- Le smart merge a fait quelque chose d'inattendu (rare) — l'overwrite donne
  un état canonique dont on peut repartir.

Une boîte de dialogue confirme avant l'écrasement.

### Indicateur de sync

En bas à droite de la status bar :

- 🟢 **● in sync** : si on faisait Ctrl+G maintenant, rien ne changerait.
- 🟠 **● form/code differ** : la form ou les fonctions cochées ont changé
  par rapport à ce qui est dans l'éditeur. `Ctrl+G` synchronisera.

---

## 9. Simulateur Lua embarqué

Onglet **Simulator** (à droite, à côté d'Editor).

### Charger un script

- **Ctrl+R** ou **Simulator → Load editor into simulator**.
- L'app reset l'interpréteur, recompile le script, et bascule sur l'onglet
  Simulator.

### Toolbar

| Bouton            | Action                                               |
|-------------------|------------------------------------------------------|
| ▶ Run             | Démarre la boucle (timer Qt à ~50 ms)                |
| ⏸ Pause           | Stoppe la boucle (état conservé)                     |
| ⟲ Reset           | Reset le temps simulé à 0, vide la timeline          |
| Step              | Avance d'un tick (= un appel à `Loop`)               |
| Soft Btn          | Pressé tant que maintenu (envoie `SoftButton(true/false)`) |
| Trigger 1A        | Envoie un cycle `ExternalTrigger("TRIGGER1", "A", true/false)` |
| `speed × N`       | Multiplicateur de temps simulé (0.1× à 100×)         |
| `N ms/tick`       | Pas de simulation entre deux appels à Loop (1-1000)  |

### État affiché

- **CH1..4** : ON/OFF, puissance, fréquence Hz, pulse width us.
- **Timeline** : 4 lanes. Vert = canal ON continu, orange = `ChannelPulseMs`.
  Axe temporel en bas, ligne verticale = "now". Fenêtre glissante de 5 s.
- **Log** en bas : sortie de `print()` Lua + erreurs.

### Limites du simulateur

- L'API `zc.*` est stubbée — les appels enregistrent des **événements**
  mais ne reproduisent pas la physique du ZC95 (formes d'onde fines, audio
  temps réel, contraintes électriques).
- Lua 5.4 strict : `module()`, `package.seeall`, `loadstring` sont absents.
  Les scripts qui en dépendent (Waves, Rhythm — ils utilisent `ettot`) ne
  tournent pas tels quels dans le simulateur.
- Le linter signale ces incompatibilités via les Issues bleues.

---

## 10. Raccourcis clavier

| Raccourci             | Action                                          |
|-----------------------|-------------------------------------------------|
| **Fichier**           |                                                 |
| Ctrl+N                | Nouveau                                         |
| Ctrl+O                | Ouvrir                                          |
| Ctrl+S                | Save                                            |
| Ctrl+Shift+S          | Save As                                         |
| Ctrl+Q                | Quit                                            |
| **Édition**           |                                                 |
| Ctrl+F                | Find                                            |
| Ctrl+H                | Replace                                         |
| F3 / Shift+F3         | Find next / previous                            |
| Echap                 | Fermer la barre Find                            |
| **Génération**        |                                                 |
| Ctrl+G                | Smart merge (form → code, préserve logique)     |
| Ctrl+Shift+Alt+G      | Regenerate from scratch (overwrite)             |
| Ctrl+Shift+G          | Reparser : code → form                          |
| Ctrl+L                | Lint                                            |
| **Simulateur**        |                                                 |
| Ctrl+R                | Charger l'éditeur dans le simulateur            |
| **Éditeur Lua**       |                                                 |
| Ctrl+Space            | Auto-complétion manuelle                        |
| Ctrl+Z / Ctrl+Y       | Undo / Redo                                     |

---

## 11. Dépannage / FAQ

### "L'app ne se lance pas — DLL Qt manquante"

Sur Windows, ajouter `C:\Qt\6.8.3\msvc2022_64\bin` au PATH avant de lancer
l'exe, ou utiliser **windeployqt** :

```
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe build\zc95-lua-builder.exe
```

### "Le build échoue : 'type_traits' introuvable"

L'environnement MSVC n'est pas activé. Lancer d'abord :

```
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
```

### "Mon script référence une lib mais ne tourne pas dans le simulateur"

Le simulateur n'a pas accès aux libs du firmware ZC95 (`ettot`, `time`, etc.).
Le linter vous le rappelle via une Issue bleue. Le script tournera correctement
sur l'appareil — le simulateur ne sert qu'à valider la logique pure.

### "J'ai perdu le corps de ma fonction Loop après Ctrl+G"

Cela n'est plus possible depuis le smart merge. Si vous avez encore un fichier
qui aurait perdu son corps, vérifiez :

- Que vous êtes bien sur la version courante (`Ctrl+G` mentionne "function
  bodies preserved" dans la status bar).
- Que vous n'avez pas pressé `Ctrl+Shift+Alt+G` (= overwrite avec dialog).

### "Le parser ne lit pas une valeur calculée"

Le parser est tolérant mais ne fait pas d'évaluation. Une expression du type
`default = _delay_ms` est lue comme `0`. Solutions :

- Mettre une valeur littérale dans le bloc Config (`default = 500`) puis
  initialiser la variable séparément.
- Régénérer la form depuis la valeur littérale.

### "Comment uploader le script sur le ZC95 ?"

Voir le `RemoteAccess.md` du firmware ZC95. L'app ne fait pas l'upload
elle-même (pour l'instant) — copier le contenu de l'éditeur ou le fichier
`.lua` via la méthode décrite par le firmware.

---

## 12. Limitations connues

- **Pas d'upload direct** vers le ZC95 (passer par l'interface du firmware).
- **Parser Lua tolérant mais limité** :
  - N'évalue pas les expressions (`default = _x` → 0).
  - Ne gère pas les `--[==[ ]==]` (long brackets niveau ≥ 1, rare).
- **Comments dans le bloc Config** : non préservés à travers le round-trip
  (parser → générateur).
- **Simulateur Lua 5.4** : ne charge pas les scripts qui utilisent `module()`
  ou `package.seeall` (Waves, Rhythm). Le linter le signale.
- **Pas d'undo cross-form** : changer un champ de la form puis Ctrl+Z dans
  l'éditeur ne reverte pas le champ de la form.
