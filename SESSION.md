# Session resumption notes

> Internal handoff document — not part of the user-facing docs.
> If you're picking this project up from scratch, read **README.md** and
> **HELP.md** first. This file only captures session context for
> continuing work.

---

## Snapshot

| Item | State |
|---|---|
| Build | ✅ green (MSVC 2022, Qt 6.8.3) |
| Tests | ✅ 102/102 passing in ~70 ms |
| Hardware validation | ❌ never run on a real ZC95 — see EXPERIMENTAL banner |
| Scripts validated | All 12 officials load, run Setup, 100 Loop ticks, exercise every callback without crashing |
| Portable build | ✅ `_deploy.bat` produces `dist/zc95-lua-builder/` (~43 MB, self-contained) |

---

## What was accomplished this session

### Tier A — beginner ergonomics (done)

- **A1 — Pedagogical tooltips** on every Config / MenuItemDialog / Functions
  field, written in terms of *sensations* not just ranges
  ("50 Hz = slow buzz, 250 Hz = sharp", DANGER ZONE on triphase, etc.)
- **A2 — 7 safety-lint rules** in `Linter.cpp` (`SetFrequency > 250 Hz`,
  `SetPulseWidth > 200 µs`, hard-coded high power without a slider, no
  kill-switch, EnableTriphase info, empty Loop body, runaway SetPower
  inside Loop without `if`)
- **A3 — Pre-flight check** (Ctrl+Shift+P): runs the linter + a 1-second
  simulator dry-run (Setup + 50 Loop ticks) and shows a verdict dialog
  with confidence score 0-100%
- **A4 — Beginner mode toggle** (View menu): hides advanced Config
  fields, advanced callbacks, and the "quick presets". Force-clears
  triphase + BT passthrough flags. Persisted in QSettings. Blue badge
  in status bar.
- **A5 — Autosave / draft recovery**: 30 s timer dumps editor to
  `<temp>/zc95-lua-builder/autosave-<pid>.lua`. On startup, scans for
  leftovers from other PIDs and offers recovery. Cleaned up on graceful
  close.

### Tier B — power features (done)

- **B1 — Live menu sliders in the simulator**: section "Menu controls
  (live)" with sliders/combos that fire `MinMaxChange` /
  `MultiChoiceChange` immediately, no rerun needed
- **B2 — Explain this script** (Ctrl+Shift+E): naive line-by-line
  pattern-matcher producing plain English notes (covers `zc.*`, `for`,
  `if menu_id`, `local`, `module`, `require`, …) in a 3-column HTML
  table
- **B3 — New from wizard…** (Ctrl+Shift+N): 6-step pattern generator
  (Type / Intensity / Cycle / Channels / Kill-switch / Review) producing
  a French-commented Lua script. Wizard later upgraded with
  contextual side-info panels per page (sensation + recommendation +
  warnings per choice)
- **B4 — Diff dialog after Ctrl+G**: shows green/red line-level diff of
  what smart-merge changed. "Don't show again" persists; toggle in
  View → Show diff after regenerate
- **B5 — ▶ Test button** in MenuItemsPanel: drives the selected item
  through min/default/max (or every choice_id), reports whether the
  script reacted

### Bug fixes

- **Float-to-int tolerance**: `LuaRuntime` now uses `checkIntLike()`
  helper (truncate via `lua_Number`) for every numeric `zc.*` argument.
  Lua 5.4's `luaL_checkinteger` rejected floats like `22.0`; ettot.lua
  generates floats from `math.*` calls.
- **Reset doesn't reset Lua state** (TENS Run 2 broken): `reset()` now
  re-loads the cached `m_currentSource` so every global goes back to
  initialisation, channels reset, schedulers reset.
- **trifade `_step_start_time_ms` nil**: `callSetup` now passes
  `lua_pushnumber(L, m_currentTimeMs)` (= 0). Lua silently ignores extra
  args, so other scripts are unaffected.
- **menu_id collision in form** (`MenuId.FREQ` etc.): added
  `LuaRuntime::extractScriptConfig()` that walks the resolved Lua
  `Config` table after pcall, with real integer IDs. `MainWindow::
  fixupFormFromSimulator()` overwrites the form's regex-parsed IDs with
  the Lua-resolved ones after every load. Called from `loadFile`,
  `loadOfficialScript`, `loadPreset`.
- **Cursor invisible**: TimelineWidget now draws a yellow halo (12 px
  α=50) + bright 3 px yellow line + 1 px white core + double yellow
  triangles top & bottom. Per-segment outlines removed (they looked like
  cursors that didn't move).
- **LCD preview disconnected from simulator**: `LcdPreviewPanel` got a
  `m_liveValues` map. SimulatorPanel emits
  `liveMenuValueChanged(menuId, value)` on slider/combo move; MainWindow
  wires it through. Cleared on script swap.
- **`replaceCurrent` ignored case-sensitivity flag**: fixed to honor it.
- **`updatePulses` emitted ChannelOff with `timeMs = -1`**: capture
  `pulseEndsAtMs` before reset.
- **Linter dead code**: removed unused `LineFinder` struct.

### Tests added

- `officialScriptsLoadInSimulator` × 12
- `officialScriptsSetupRuns` × 12
- `officialScriptsLoopRuns` × 12 (100 Loop ticks @ 20 ms each)
- `officialScriptsResolveConfig` × 12 (Lua-extracted Config; checks
  unique non-zero IDs)
- `officialScriptsExerciseCallbacks` × 12 (drives every MIN_MAX through
  min/default/max, every MULTI_CHOICE through all choices, +SoftButton
  +ExternalTrigger)

`tests/test_roundtrip.cpp` now includes `LuaRuntime` so the test target
links against `lua_static`.

### Docs

- **README.md**: rewritten in English with EXPERIMENTAL caution banner
  (`[!CAUTION]` GitHub callout). Highlights section now grouped:
  beginner-friendly, safety guardrails, …
- **HELP.md**: 23-section English manual with the same banner. New
  sections covering beginner mode + wizard, pre-flight check, explain
  script, ▶ test buttons, autosave. Linter table updated with safety
  rules.
- **MANUEL.md**: French manual (older, not updated this session — the
  English HELP.md is canonical now).

---

## Architecture cheat-sheet

```
src/
├── main.cpp + MainWindow.{h,cpp}    GUI shell, menus, autosave, beginner mode,
│                                    fixupFormFromSimulator, runPreflight,
│                                    explainScript, newFromWizard, …
├── model/                           ScriptConfig, MenuItem
├── codegen/
│   ├── LuaGenerator                 model→Lua + smart-merge
│   ├── LuaParser                    Lua→model (regex, literal-only)
│   ├── Linter                       static checks (now incl. 7 safety rules)
│   ├── ApiDoc                       built-in zc.* docs
│   ├── Explainer    [NEW]           plain-English line-by-line
│   └── LineDiff     [NEW]           LCS for "what changed" dialog
├── sim/
│   ├── ChannelEvent.h
│   └── LuaRuntime.{h,cpp}           Lua 5.4, zc.* stubs, 5.1 polyfill,
│                                    Qt-resource searcher,
│                                    extractScriptConfig() [NEW]
├── highlight/LuaHighlighter
└── ui/
    ├── ConfigPanel + setBeginnerMode
    ├── MenuItemsPanel + ▶ Test button
    ├── MenuItemDialog
    ├── FunctionsPanel + setBeginnerMode
    ├── SnippetsPanel
    ├── ApiDocPanel
    ├── LcdPreviewPanel + setLiveValue / clearLiveValues   [NEW]
    ├── LuaEditor
    ├── FindReplaceBar
    ├── IssuesPanel
    ├── TimelineWidget (improved cursor + segment merging)
    ├── SimulatorPanel + setMenuItems / live sliders / signals  [NEW]
    └── WizardDialog (with 6 pages × side info panels)     [NEW]
```

---

## Known limitations / open ideas

### Tier C (not started)

The original plan listed three big features in Tier C. None implemented.

- **C1 — Block-based pattern editor (Scratch-style)**: drag-drop blocks
  that compile to Lua. Estimated 2-3 days, ~800-1500 LOC.
- **C2 — Timeline editor**: draw pulses on a 4-channel timeline at the
  mouse, generate Lua that reproduces it. JSON intermediate format
  needed for round-trip.
- **C3 — Compare-mode**: overlay two scripts' timelines side by side.

### Tier D (deferred)

- Direct upload to ZC95 over the `RemoteAccess.md` protocol
- ettot polyfill for the simulator (currently `ettot` is bundled via
  Qt resources but `module()` polyfill handles loading)
- Variable inspector live in simulator (debug.getlocal walking)
- Profiles of safety (hardware-locked caps): "Beginner profile = max
  power 500"
- Glossary popups (right-click any term → definition)

### Smaller TODOs

- Long-bracket comments at level ≥ 1 (`--[==[ … ]==]`) not stripped by
  highlighter (parser handles them)
- The 4 quick presets (Toggle/Fire/Waves/Audio) are skeletons; could be
  replaced or hidden permanently in beginner mode (currently hidden
  there but visible in expert mode)
- Wizard-generated scripts always start `_intensity = 0`. The user has
  to move the slider before any output. Document this more prominently?
- `MenuItemDialog::item()` reads choices even for MIN_MAX type (no
  effect on generated code, but the table data persists in memory)

---

## Build / test commands

```bat
:: Build (Windows / MSVC)
_build.bat

:: Run smoke + roundtrip tests
build\test_roundtrip.exe

:: Build portable
_deploy.bat
```

Manual GUI verification still TODO for:
- The Reset/Run-2 fix (channels should flicker on Run 2 same as Run 1)
- The yellow cursor visibility on dense Burst patterns
- The wizard side-info panels rendering correctly at 980×620 default
- The LCD preview live updates when moving simulator sliders

---

## File-touched-most index

- `src/MainWindow.{h,cpp}` — most of the new wiring lives here
- `src/sim/LuaRuntime.{h,cpp}` — float-tolerance, extractScriptConfig,
  Setup time_ms, 5.1 polyfill, qrc searcher
- `src/ui/SimulatorPanel.{h,cpp}` — live sliders, reset reload, signals
  for LCD mirror, testMenuItemDrive, resolveScriptConfig
- `src/ui/WizardDialog.{h,cpp}` — entirely new + side info panels
- `src/codegen/Linter.cpp` — 7 safety rules added
- `src/codegen/Explainer.{h,cpp}` — new
- `src/codegen/LineDiff.{h,cpp}` — new
- `src/ui/LcdPreviewPanel.{h,cpp}` — live-value override
- `src/ui/TimelineWidget.cpp` — cursor + segment overhaul
- `tests/test_roundtrip.cpp` — 60 new test cases (5 × 12 scripts)
- `CMakeLists.txt` — Explainer + LineDiff + WizardDialog + lua_static
  link for tests
- `README.md`, `HELP.md` — extensive rewrite + EXPERIMENTAL banner
