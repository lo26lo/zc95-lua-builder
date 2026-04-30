# ZC95 Lua Builder

> [!CAUTION]
> ## EXPERIMENTAL — UNTESTED ON REAL HARDWARE — USE AT YOUR OWN RISK
>
> **This project is in active development. Nothing here has been validated on an actual ZC95 device yet.**
>
> The form generator, the parser, the linter and the embedded simulator are exercised by the unit tests, but **no script produced by this tool has been flashed to real hardware.** Generated scripts may behave differently from what the simulator suggests, may exceed safe parameters, or may not load at all.
>
> **Treat every output as draft material. Review it line by line. Run it on a dummy load before any real session. The authors disclaim all liability for damage to equipment or harm to persons.**
>
> Found a bug? Open an issue. Until "verified on hardware" can replace this banner, assume nothing.

---

A desktop GUI for visually authoring, simulating and debugging Lua patterns
for the [ZC95](https://github.com/CrashOverride85/zc95) e-stim controller.

Build the script from forms, run it in an embedded Lua 5.4 interpreter, watch
the channels light up on a timeline, and copy the resulting `.lua` to the
device. Or open one of the 12 official scripts shipped with the firmware,
tweak it, and replay it.

> **Status:** functional — round-trip tested on the 12 official scripts
> (`climb`, `combo`, `intense`, `orgasm`, `phasing2`, `random2`, `rhythm`,
> `stroke`, `tens`, `torment`, `trifade`, `waves`).

---

## Highlights

- **Form-driven editor** — fill out the Config, menu items and callbacks in
  tabs; the matching Lua is generated for you.
- **Smart merge** (`Ctrl+G`) — regenerate only the `Config = {…}` block from
  the form. Your function bodies are kept verbatim. Optional **diff dialog**
  shows exactly what was added / removed.
- **Two-way sync** — parse an existing `.lua` back into the form
  (`Ctrl+Shift+G`). A status-bar badge flips between `● in sync` and
  `● form/code differ`.
- **Embedded Lua 5.4 simulator** — really executes your script. The `zc.*`
  API is stubbed and emits events that drive a 4-lane timeline. **Live
  sliders / combos** in the simulator drive `MinMaxChange` and
  `MultiChoiceChange` in real time so you can test the script
  interactively without flashing.
- **Lua 5.1 compat** — `module(…)`, `package.seeall` and `require("ettot")`
  all work in the simulator. Float arguments to `zc.SetFrequency` /
  `SetPower` / etc. are tolerated (truncated, matching device behavior).
- **Beginner-friendly** :
  - **New from wizard…** (`Ctrl+Shift+N`) — 6-step dialog generates a
    safe starting script with French comments on every line.
  - **Beginner mode** (`View` menu) — hides advanced options
    (triphase, Bluetooth HID, audio, …).
  - **Pedagogical tooltips** — every Hz / µs / power field explains
    what the value *feels* like, not just its range.
  - **Explain this script…** (`Ctrl+Shift+E`) — best-effort plain-English
    line-by-line breakdown of the Lua.
- **Safety guardrails** :
  - Linter warns on hard-coded high power, frequencies above 250 Hz,
    pulse widths above 200 µs, missing kill-switch, runaway `Loop()`
    bodies, and unconditional triphase enable.
  - **Pre-flight check** (`Ctrl+Shift+P`) — runs the linter AND a 1-second
    simulator dry-run, returns a confidence score 0-100%.
  - **▶ Test** button per menu item — drives the slider through min /
    default / max and reports whether the script reacted.
  - **Auto-save** every 30s; recover unsaved drafts after a crash.
- **Linter** with 3 severity levels — out-of-range arguments, duplicate
  menu IDs, audio-mode mismatch, triphase without `allow_triphase`,
  missing `Loop()`, and more. Double-click an issue to jump to the line.
- **Code editor** — syntax highlighting, line numbers, current-line
  highlight, autocomplete on `Ctrl+Space` and after 2 chars, find/replace
  with case-sensitivity and whole-word options.
- **LCD preview** — live mock-up of the ZC95 screen as you edit menu items.
- **Bundled official scripts** — load any of the 12 stock patterns straight
  from `Presets → Official scripts`.
- **Quality-of-life** — drag-drop `.lua` files, 8-slot recent files menu,
  geometry / dock / splitter persistence, unsaved-changes guard.
- **Portable Windows build** via `_deploy.bat` — produces a self-contained
  folder you can zip and run on any Windows 10/11 x64 machine.

---

## Screenshots

```
+-------------+--------------------------------------------------+
|  Config     |  Editor                  | Simulator             |
|  Menu Items |  function Loop(time_ms)  | ▶ Run  ⏸ Pause  ⟲    |
|  Functions  |    if (...) then         | CH1: ON  pwr=850      |
|  Snippets   |      zc.SetFreq(1, 50)   | CH2: OFF              |
|  API Help   |    end                   | timeline ▓▓░░▓▓░░     |
|  LCD Prev.  |  end                     | t=2.300s              |
+-------------+--------------------------------------------------+
| Issues   ⚠ L42  zc.SetPower power 2000 out of range (0-1000)   |
+----------------------------------------------------------------+
```

(Real screenshots will be linked here once published.)

---

## Quick start

### Prerequisites

| Tool | Tested with |
|------|-------------|
| Qt   | 6.8.3 (also fine with 6.5+) — components: **Widgets**, **Test** |
| CMake| 3.16+ |
| C++  | C++17 — MSVC 2019/2022, MinGW, Clang or GCC all work |
| Net  | needed once at first configure to fetch Lua 5.4.7 via FetchContent |

### Build (Windows / MSVC)

```bat
git clone https://github.com/<you>/zc95-lua-builder
cd zc95-lua-builder
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
build\zc95-lua-builder.exe
```

If `cl.exe` isn't on PATH, run from a *Developer Command Prompt for VS*
or use the included `_build.bat` helper which calls `vcvarsall.bat` first.

### Build (Linux / macOS)

```bash
git clone https://github.com/<you>/zc95-lua-builder
cd zc95-lua-builder
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build -j
./build/zc95-lua-builder
```

### Run the tests

```bash
ctest --test-dir build --output-on-failure
# 42/42 should pass in ~25ms
```

### Build a portable Windows package

```bat
_build.bat
_deploy.bat
:: => dist\zc95-lua-builder\  (zippable, no Qt install needed on target)
```

`_deploy.bat` runs `windeployqt` and copies the MSVC runtime DLLs
(`vcruntime140.dll`, `msvcp140.dll`, …) directly so the folder runs on a
clean Windows machine without a redistributable installer.

---

## A 30-second tour

### If you've never written Lua before
1. `File → New from wizard…` — answer 6 simple questions.
2. The wizard produces a fully-commented script and fills the form.
3. Switch to **Simulator**, press **Run** — your pattern is alive.
4. Move the **Intensity** slider in the simulator's *Menu controls*
   section — feel the script react in real time.
5. **Generate → Pre-flight check** — get a verdict before saving.

### If you already know what you want
1. **Open** `Presets → Official scripts → Climb`.
2. The form on the left fills in: name, audio mode, menu items.
3. The `LCD Preview` tab shows what the device screen will look like.
4. Switch to the **Simulator** tab and press **Run** — the channels start
   firing on the timeline, and `print()` output appears in the log.
5. Edit something in the editor (e.g. change a frequency in `Loop`),
   press **Reset** then **Run** again to compare.
6. Tweak menu items in the form, press **Ctrl+G** — only the
   `Config = {...}` block is rewritten; your edits in the function bodies
   are kept. A diff dialog shows you exactly what changed.
7. **File → Save As…** to write the `.lua` back out.

---

## Keyboard shortcuts

| Shortcut             | Action |
|----------------------|--------|
| `Ctrl+N`             | New script |
| `Ctrl+Shift+N`       | New from wizard… |
| `Ctrl+O`             | Open `.lua` |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / Save As |
| `Ctrl+G`             | Regenerate code from form (smart merge — keeps function bodies) |
| `Ctrl+Shift+Alt+G`   | Regenerate from scratch (overwrites the editor — confirms first) |
| `Ctrl+Shift+G`       | Re-parse the form from the editor |
| `Ctrl+L`             | Run the linter |
| `Ctrl+Shift+P`       | Pre-flight check (lint + 1-second simulator dry-run) |
| `Ctrl+Shift+E`       | Explain this script… |
| `Ctrl+R`             | Reload the editor into the simulator |
| `Ctrl+F` / `Ctrl+H`  | Find / Replace |
| `F3` / `Shift+F3`    | Find next / previous |
| `Ctrl+Space`         | Trigger autocomplete |

For a complete walkthrough of every panel, every linter rule, every
keyboard shortcut and every troubleshooting step, see [**HELP.md**](HELP.md).

---

## Project layout

```
src/
├── main.cpp
├── MainWindow.{h,cpp}            main window, menus, I/O, settings, drag-drop
├── model/
│   ├── ScriptConfig.{h,cpp}      Config block data
│   └── MenuItem.{h,cpp}          MIN_MAX / MULTI_CHOICE / AUDIO_VIEW_*
├── codegen/
│   ├── LuaGenerator.{h,cpp}      model → Lua, plus the smart merge
│   ├── LuaParser.{h,cpp}         Lua → model (tolerant)
│   ├── Linter.{h,cpp}            consistency checks
│   └── ApiDoc.{h,cpp}            built-in zc.* API docs
├── sim/
│   ├── ChannelEvent.h            simulator event types
│   └── LuaRuntime.{h,cpp}        Lua 5.4 + stubs + 5.1 polyfill + qrc loader
├── highlight/
│   └── LuaHighlighter.{h,cpp}    QSyntaxHighlighter for Lua
└── ui/
    ├── ConfigPanel, MenuItemsPanel, MenuItemDialog
    ├── FunctionsPanel, SnippetsPanel
    ├── LuaEditor                 editor (line numbers, completer, highlighter)
    ├── FindReplaceBar            Ctrl+F / Ctrl+H bar
    ├── IssuesPanel               linter dock
    ├── ApiDocPanel               API help dock
    ├── LcdPreviewPanel           ZC95 LCD mock-up
    ├── TimelineWidget            4-lane channel timeline
    └── SimulatorPanel            full simulator UI

resources/
├── presets.qrc                   embeds the 12 official scripts + ettot.lua
└── ../lua_Script/                source of the embedded scripts

tests/
└── test_roundtrip.cpp            Qt Test: 42 cases (round-trip, linter, merge)
```

---

## How smart merge works

`Ctrl+G` is **not** "regenerate from scratch". The flow is:

1. `LuaGenerator::mergeIntoSource(config, existingSource)` finds the
   `Config = { … }` block in the editor by brace-matching on a sanitized
   copy of the source (strings and comments blanked out).
2. The Config block is replaced with a freshly emitted one from the form.
3. For every callback that's now ticked in the **Functions** panel and that
   isn't yet defined in the source, an empty stub is appended at the end.
4. Everything else — your `Setup()`, `Loop()`, locals, helpers, comments —
   is preserved byte-for-byte.

If the source has no `Config = {…}` block at all, the merge falls back to
a full generation (same as `Ctrl+Shift+Alt+G`).

---

## How the simulator works

- A single `lua_State` is created with all standard libraries.
- The `zc.*` table is registered with C functions that don't talk to any
  hardware — they update an in-memory `SimState` (4 channels, triphase
  flag, accessory I/O lines) and push events into a queue.
- A timer ticks at the configured rate (default 20ms simulated, drawn at
  ~50ms wallclock) and calls `Loop(time_ms)` after the chunk's `Setup()`.
- Buttons in the simulator toolbar call `SoftButton(true/false)` and
  `ExternalTrigger("TRIGGER1", "A", true/false)` directly into Lua.
- Events drive the **TimelineWidget** which paints 4 lanes with green
  segments for sustained `ChannelOn`/`ChannelOff` ranges and orange for
  `ChannelPulseMs`.

### Lua 5.1 compatibility

The official scripts use idioms removed in Lua 5.2:

- `module("ettot", package.seeall)` — polyfilled by walking the calling
  chunk's upvalues, finding `_ENV`, and rebinding it to the module table.
- `require("ettot")` — resolved against `:/presets/lib/ettot.lua` via a
  custom `package.searchers` entry installed at index 2.

Result: every official script runs out of the box.

---

## Limitations

- The Lua parser is **tolerant but literal** — values that depend on a
  Lua variable (e.g. `default = _delay_ms`) read back as `0`. Comments
  inside the `Config` block are kept by smart-merge but not by full
  generation.
- The simulator stubs `zc.*` for logic verification, not physics — it
  doesn't model real waveforms, electrode-level current, or audio
  buffering. It tells you *what* the script tries to do, not *how it
  feels* on the device.
- The simulator UI exposes Run/Pause/Reset/Step/SoftButton/Trigger 1A.
  Other callbacks (`MinMaxChange`, `MultiChoiceChange`, `Bluetooth*`,
  `AudioIntensityChange`) can be invoked from the menu but have no
  dedicated buttons yet.
- Long-bracket comments at level ≥ 1 (`--[==[ … ]==]`) are not stripped
  by the highlighter (they are stripped by the parser).

---

## Contributing

PRs welcome. The codebase is laid out so that new features tend to fit in
exactly one place:

- **A new `zc.*` function** — one entry in `ApiDoc.cpp` (docs), one entry
  in `Linter.cpp` (range check), one C function in `LuaRuntime.cpp` (sim
  stub), and optionally a snippet in `SnippetsPanel.cpp`.
- **A new menu-item type** — one enum in `MenuItem.h`, one
  `typeToString`, one branch in `LuaGenerator::generateMenuItem`, one
  branch in `LuaParser::parseMenuItem`, one branch in
  `LcdPreviewPanel::paintEvent`, one combo entry in `MenuItemDialog`.
- **A new linter rule** — one block in `Linter::lintSource` or
  `Linter::lintConfig`.
- **A new built-in preset** — drop a `.lua` in `lua_Script/`, add a
  `<file>` entry in `resources/presets.qrc`, add the action in
  `MainWindow::buildMenus`.

Run `ctest` from `build/` before opening a PR. New features should come
with a test in `tests/test_roundtrip.cpp`.

---

## License

The application code in `src/` is released under the terms of the parent
ZC95 project (see upstream). Third-party code:

- **Lua 5.4.7** — MIT license, fetched at configure time.
- **Qt 6** — LGPLv3 / GPLv3 / commercial — link as a dynamic library to
  comply with LGPL.
- The bundled official scripts (`lua_Script/*.lua`) are reproduced from
  the upstream ZC95 firmware repository under their original license.

---

## Further reading

- [HELP.md](HELP.md) — complete user manual (every panel, every menu,
  every linter rule, every shortcut, every troubleshooting step).
- [Upstream ZC95 firmware](https://github.com/CrashOverride85/zc95) —
  device source, Lua API reference, hardware schematics.
