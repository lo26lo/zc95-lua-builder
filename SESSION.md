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
| Last commit | (uncommitted local work — see "What was accomplished" below) |

---

## House rule

`CLAUDE.md` at the repo root encodes the agreement that **every change
visible to the user must update README.md AND HELP.md in the same
change**. Read it before working.

---

## What was accomplished this session

This session built progressively across multiple feature tiers.

### Tier A — beginner ergonomics (done)

- **A1** Pedagogical tooltips on every form field, written in terms of
  *sensations* not just ranges.
- **A2** 7 safety-lint rules (frequency > 250 Hz, pulse width > 200 µs,
  hard-coded high power without a slider, missing kill-switch, triphase
  info, empty Loop body, runaway SetPower in Loop without `if`).
- **A3** Pre-flight check (`Ctrl+Shift+P`): linter + 1-second simulator
  dry-run with confidence score 0-100 %.
- **A4** Beginner-mode toggle in `View` menu — hides advanced fields,
  callbacks, quick presets. Force-clears triphase + BT passthrough.
- **A5** Autosave every 30 s to
  `<temp>/zc95-lua-builder/autosave-<pid>.lua` + draft recovery dialog
  at startup.

### Tier B — power features (done)

- **B1** Live menu sliders / combos in the simulator that fire
  `MinMaxChange`/`MultiChoiceChange` instantly.
- **B2** "Explain this script" (`Ctrl+Shift+E`) — naive
  pattern-matcher producing plain-English notes per line.
- **B3** New-from-wizard (`Ctrl+Shift+N`) — 6-step beginner generator
  with **contextual side-info panels** per page (sensation /
  recommendation / warnings).
- **B4** Diff dialog after `Ctrl+G` — green/red line-level diff,
  silenceable in `View` menu.
- **B5** ▶ Test button per menu item — drives min/default/max,
  reports whether the script reacted.

### W1 — Power visualised on the timeline (done)

`TimelineWidget` segments now carry a `power` field. `SetPower` events
split the running ON segment so power changes mid-pattern are visible.
Bar height = `2 + power × 12 / 1000` pixels, centered on the lane
mid-line.

### W2 — Variable inspector (done)

`LuaRuntime::inspectGlobals()` walks `_G` and returns underscore-
prefixed globals with their stringified values (handles
number/integer/string/bool/nil/table/function). `SimulatorPanel` shows
them in a 2-column `QTableWidget` next to the log; refreshed on every
`refreshState()` so it updates with each Loop tick + every
callback-induced state change.

### C2 — Timeline editor (done — v1)

A 3rd tab on the right pane.

- `src/codegen/TimelineProject.{h,cpp}` — `TimelineParam` (literal
  vs variable), `TimelineEvent` (Pulse/On/Off + power/freq/width
  params + comment), `TimelineProject` (events + loop + cycleMs) with
  full JSON serialise/deserialise.
- `src/codegen/TimelineCodegen.{h,cpp}` — `generateBody()` emits
  Setup + Loop with `_tl_fired_N` flags and modulo wrap when
  `loop=true`. `extractProject()` and `updateSentinel()` round-trip
  via the embedded `--[[ ZC95_TIMELINE_V1 ... ]]` comment.
- `src/ui/TimelineEditorWidget.{h,cpp}` — interactive 4-lane canvas:
  drag-create / drag-move / drag-resize / right-click menu / wheel
  zoom / Ctrl+wheel pan / Delete key / cross-channel dragging.
- `src/ui/TimelineEditorPanel.{h,cpp}` — toolbar (Loop /
  Cycle / ← Read from Lua / → Push to Lua / Clear all) + canvas +
  property panel for the selected event. Power / freq / width each
  toggle between Literal and Variable.
- `MainWindow` wires it as 3rd right-tab. `formChanged` propagates
  the form's MIN_MAX variable names to the panel for auto-suggestion.
  Push-to-Lua strips existing Setup/Loop, updates the sentinel,
  appends the freshly-generated body, reloads the simulator.

### C3 — Safety profile (done)

Process-wide caps applied at the simulator level + linter level.

- `src/model/SafetyProfile.{h,cpp}` — singleton with `maxPower /
  maxFrequencyHz / maxPulseWidthUs / maxPulseDurationMs / locked /
  pinHash`. SHA-256 hashed PIN with static salt. Persisted in
  QSettings under group `safety`.
- `src/ui/SafetyProfileDialog.{h,cpp}` — 4 spinboxes for caps, lock
  toggle, two PIN fields with confirmation, "Unlock to edit…" button.
- `LuaRuntime::api_SetPower / SetFrequency / SetPulseWidth /
  ChannelPulseMs` clamp via `SafetyProfile::current()` and log a
  `[SAFETY] ... clamped to ...` line.
- `Linter::lintSource` rule **S0** scans literals; severity is
  `Warning` when unlocked, `Error` when locked.
- `MainWindow` shows an orange status-bar badge
  `🔒 Safety locked (max pwr=N · M Hz · K µs)` and force-checks +
  disables `View → Beginner mode` while locked.

### Bug fixes & polish (this session)

- **Float-to-int tolerance** — `LuaRuntime` uses a `checkIntLike`
  helper (`luaL_checknumber` + cast) so floats from `math.*` are
  accepted, not rejected by Lua 5.4's `luaL_checkinteger`.
- **Reset reload** — `SimulatorPanel::reset()` re-loads the cached
  source so a 2nd Run starts from a fresh `lua_State` (TENS no longer
  freezes after Reset).
- **`trifade.lua` Setup crash** — `LuaRuntime::callSetup` now passes
  `lua_pushnumber(L, m_currentTimeMs)` (= 0). Lua silently ignores
  extra args for scripts without a parameter, so other scripts are
  unaffected.
- **Identifier-based menu IDs** — `LuaRuntime::extractScriptConfig`
  walks the resolved `Config` table from Lua. `MainWindow::
  fixupFormFromSimulator` patches the form's regex-parsed IDs (which
  were all 0 because expressions like `id = MenuId.FREQ` are
  literal-only) with real integer IDs. Called after every load.
- **LCD live mirror** — `LcdPreviewPanel` got `m_liveValues : QMap<int,int>`
  and `setLiveValue/clearLiveValues`. SimulatorPanel emits
  `liveMenuValueChanged` per slider/combo move; MainWindow mirrors to
  the LCD preview.
- **Smart-merge — function removal** — `Ctrl+G` now detects callbacks
  that are unticked in the form but still defined in the source, and
  asks `Yes/No/Cancel` before deleting them. `LuaGenerator::
  findFunctionRange` walks the body counting
  `function|if|for|while|repeat` (+1) and `end|until` (-1).
- **Smart-merge — canonical insertion** — new stubs are inserted at
  their canonical position relative to existing firmware callbacks
  (Setup, Loop, MinMaxChange, MultiChoiceChange, SoftButton,
  ExternalTrigger, BluetoothRemoteKeypress, BluetoothHidEvent,
  AudioIntensityChange) instead of all being appended at the end.
- **Yellow timeline cursor** — replaced the 1 px dashed red cursor
  with a 12 px translucent halo + 3 px solid yellow line + 1 px white
  core + double yellow triangles top & bottom. Per-segment outlines
  removed (they looked like fake cursors).
- **User-input markers on the timeline** — moving a slider / combo,
  pressing Soft Btn, or firing Trigger 1A appends a `UserInput` event
  to `m_allEvents`. `TimelineWidget` renders these as a labelled,
  colour-coded vertical dashed line so you can correlate input with
  reaction. Hovering any segment also shows an exact tooltip
  (channel, kind, start/end, duration, power). The previous
  auto-wipe-and-reseed behaviour was dropped — markers are far more
  informative than throwing the history away.
- **Linter** dead `LineFinder` removed. `replaceCurrent` honors the
  case-sensitivity flag. `updatePulses` no longer emits ChannelOff
  with `timeMs = -1`.
- **Smoke tests** — 60 new test cases × 12 official scripts covering
  loadInSimulator, setupRuns, loopRuns (100 ticks), resolveConfig
  (unique non-zero IDs), exerciseCallbacks. Total: 102 / 102 passing.

### Docs

- `README.md` rewritten in English with `[!CAUTION]` banner,
  highlights grouped (beginner-friendly / safety / simulator /
  timeline editor / safety profile), 3 image placeholders pointing
  at `pictures/`.
- `HELP.md` extended to **25 sections** (added: Beginner mode +
  wizard, Safety profile, Pre-flight, Explain, Timeline editor, ▶
  Test buttons, Auto-save). FAQ expanded with the recent bug
  symptoms. Glossary covers every term users see in the UI.
- `CLAUDE.md` codifies house rules (docs in mirror, banner stays,
  run tests, don't bypass `m_suppressDirty`, commit message format).
- `pictures/README.md` describes what each screenshot should capture.

---

## Architecture cheat-sheet

```
src/
├── main.cpp + MainWindow.{h,cpp}    GUI shell, menus, autosave, beginner
│                                    mode, fixupFormFromSimulator,
│                                    runPreflight, explainScript,
│                                    newFromWizard, openSafetyProfileDialog,
│                                    onTimelinePushToLua / PullFromLua, …
├── model/
│   ├── ScriptConfig                  Config block data
│   ├── MenuItem                      MIN_MAX / MULTI_CHOICE / AUDIO_VIEW_*
│   └── SafetyProfile  [NEW C3]       caps + locked + pinHash
├── codegen/
│   ├── LuaGenerator                  model → Lua, smart merge w/ canonical
│   │                                 insertion + confirmed removals
│   ├── LuaParser                     Lua → model (regex, literal-only)
│   ├── Linter                        inkl. 7 safety rules + S0 cap-checks
│   ├── ApiDoc                        built-in zc.* docs
│   ├── Explainer                     plain-English line-by-line
│   ├── LineDiff                      LCS for the "what changed" dialog
│   ├── TimelineProject  [NEW C2]     events + JSON serialise/deserialise
│   └── TimelineCodegen  [NEW C2]     generateBody / extractProject /
│                                     updateSentinel
├── sim/
│   ├── ChannelEvent.h
│   └── LuaRuntime                    Lua 5.4 + zc.* stubs (with safety
│                                     clamps) + 5.1 polyfill + qrc loader
│                                     + extractScriptConfig + inspectGlobals
├── highlight/LuaHighlighter
└── ui/
    ├── ConfigPanel + setBeginnerMode
    ├── MenuItemsPanel + ▶ Test
    ├── MenuItemDialog
    ├── FunctionsPanel + setBeginnerMode
    ├── SnippetsPanel
    ├── ApiDocPanel
    ├── LcdPreviewPanel + setLiveValue / clearLiveValues
    ├── LuaEditor
    ├── FindReplaceBar
    ├── IssuesPanel
    ├── TimelineWidget                4-lane simulator timeline + power
    │                                 viz + yellow cursor
    ├── SimulatorPanel                live sliders + variables panel +
    │                                 testMenuItemDrive + reset reload
    ├── WizardDialog                  6 pages × side info panels
    ├── TimelineEditorWidget [NEW C2] interactive 4-lane editor canvas
    ├── TimelineEditorPanel  [NEW C2] toolbar + canvas + props panel
    └── SafetyProfileDialog  [NEW C3] caps editor + PIN lock
```

---

## Known limitations / open ideas

### What I'd reach for next, by tier

**Quick wins (~1-3 h each)**
- W3 — adjustable timeline window in simulator (1 s ↔ 60 s slider /
  zoom buttons).
- W4 — pulse height proportional to pulse-width (mirror of W1 but for
  pulses). Easier comparison of pulse charges across scripts.
- W5 — first-launch onboarding dialog "Welcome — start with the
  wizard / open a preset".

**Medium (~1 day each)**
- M1 — cross-platform portable builds (`_deploy.sh` for Linux,
  `_deploy_mac.sh` for macOS).
- M2 — GitHub Actions CI (build + tests on push, attach binaries to
  releases).
- M3 — step-back / scrub the simulator (snapshot every N ticks, slider
  under the timeline to rewind).
- M4 — audio input simulation (load a WAV, feed
  `AudioIntensityChange`).
- M5 — code outline / function jumper in the editor (`Ctrl+P`).
- M6 — user snippets manager.

**Big (~2-5 days)**
- C1 — block-based pattern editor (Scratch-style) — the no-code
  jump for total beginners. Compiles to Lua.
- C4 — direct upload to ZC95 over `RemoteAccess.md` (TCP/Wi-Fi).
  Requires real hardware to validate.

### Smaller TODOs

- Long-bracket comments at level ≥ 1 (`--[==[ … ]==]`) not stripped
  by highlighter (parser handles them).
- Wizard-generated scripts always start `_intensity = 0`. The user has
  to move the slider before any output. Document this more
  prominently?
- `MenuItemDialog::item()` reads choices even for MIN_MAX type (no
  effect on generated code, but the table data persists in memory).
- Timeline editor: bare `do…end` blocks could confuse the
  function-removal scanner (rare in practice).
- Timeline editor: no Save/Load of timeline-only files (always
  embeds in the .lua via sentinel). Probably fine.

---

## Build / test commands

```bat
:: Build (Windows / MSVC)
_build.bat

:: Run smoke + roundtrip tests
build\test_roundtrip.exe

:: Build portable (Windows only for now)
_deploy.bat
```

Manual GUI verification still TODO for:
- W1: timeline bar height visibly varying with `SetPower` mid-pattern.
- W2: Variables panel updating live as `_intensity` etc. change.
- C2: round-trip the sentinel — push a timeline, save the .lua, reopen,
  click ← Read from Lua, confirm the events come back identically.
- C3: lock with a PIN, restart the app, confirm the badge persists and
  the simulator clamps. Verify the linter promotes warnings to errors.

---

## File-touched-most index

- `src/MainWindow.{h,cpp}` — most of the new wiring lives here.
- `src/sim/LuaRuntime.{h,cpp}` — float tolerance, extractScriptConfig,
  Setup time_ms, 5.1 polyfill, qrc searcher, safety clamps,
  inspectGlobals.
- `src/ui/SimulatorPanel.{h,cpp}` — live sliders, reset reload, signals
  for LCD mirror, testMenuItemDrive, resolveScriptConfig, variables
  table, pushUserInput.
- `src/ui/TimelineWidget.cpp` — yellow cursor, no-outline segments,
  power-as-bar-height, cyan/magenta/orange user-input markers, hover
  tooltips on segments and markers.
- `src/codegen/LuaGenerator.{h,cpp}` — function removal,
  canonical-position insertion, findFunctionRange.
- `src/codegen/Linter.cpp` — 7 safety rules + S0 cap-checks.
- `src/codegen/TimelineProject.{h,cpp}` + `TimelineCodegen.{h,cpp}` —
  data model + Lua codegen + sentinel round-trip.
- `src/ui/TimelineEditorWidget.{h,cpp}` + `TimelineEditorPanel.{h,cpp}`
  — interactive editor + property panel.
- `src/ui/WizardDialog.{h,cpp}` — 6 pages × side info panels with
  contextual sensation / recommendation / warning.
- `src/model/SafetyProfile.{h,cpp}` + `src/ui/SafetyProfileDialog.{h,cpp}`
  — caps + PIN lock.
- `src/ui/LcdPreviewPanel.{h,cpp}` — live-value override map.
- `src/codegen/Explainer.{h,cpp}` — plain-English breakdown.
- `src/codegen/LineDiff.{h,cpp}` — LCS for the diff dialog.
- `tests/test_roundtrip.cpp` — 102 cases including 60 simulator smoke
  tests across the 12 official scripts.
- `CMakeLists.txt` — every new file declared in MODEL_SOURCES /
  CODEGEN_SOURCES / SIM_SOURCES / UI_SOURCES; `lua_static` linked into
  test target.
- `README.md`, `HELP.md`, `CLAUDE.md`, `pictures/README.md` — docs.

---

## Suggested commit message

```
feat: timeline editor, safety profile, power viz, variable inspector

W1 — Power on timeline
  Bar height ∝ power (0..1000), centered on lane mid-line. SetPower
  events split running segments so changes mid-pattern are visible.

W2 — Variable inspector
  New 2-column panel next to the log shows every underscore-prefixed
  Lua global with its current value. LuaRuntime::inspectGlobals walks
  _G and stringifies number / int / bool / string / nil / table /
  function. Refreshes every Loop tick.

C2 — Timeline editor
  Third tab on the right pane: drag-create / drag-move / drag-resize
  pulses on a 4-lane canvas. Power / Freq / Width each toggle between
  literal value and reference to a form variable (e.g. _intensity)
  for fully dynamic patterns. Loop checkbox + Cycle (ms) for modulo
  wrap. Round-trips with the editor through an embedded
  --[[ ZC95_TIMELINE_V1 ... ]] sentinel block.
  + TimelineProject / TimelineCodegen modules
  + TimelineEditorWidget / TimelineEditorPanel UI

C3 — Safety profile (locked caps)
  Process-wide caps on power / frequency / pulse width / pulse
  duration applied at the simulator level (clamped + [SAFETY] log)
  AND the linter (warning → error when locked). Lockable with a
  4-16 char PIN, hashed SHA-256 + salt. Locked profile forces
  Beginner mode ON and disables its toggle. Status-bar badge.
  + SafetyProfile model + SafetyProfileDialog
  + Linter rule S0 (cap-check)
  + LuaRuntime clamps in api_SetPower / SetFrequency / SetPulseWidth
    / ChannelPulseMs

Timeline UX — user-input markers + hover tooltips
  Added a UserInput event type carrying a category + label. Sliders /
  combos / Soft Btn / Trigger 1A push one at currentTimeMs; the
  TimelineWidget renders them as cyan / magenta / orange dashed
  vertical lines with a label in a band above the lanes. Segments
  and markers are now cached on setEvents() so mouseMoveEvent can
  resolve hover targets cheaply and feed QToolTip live values
  (channel, kind, start/end, duration, power for bars; category +
  label + timestamp for markers). The previous auto-wipe-on-slider
  behaviour was dropped — keeping history annotated is much more
  useful than throwing it away.
  - ChannelEvent: new UserInput type, param1 = category code
  - SimulatorPanel: pushUserInput helper, removed clearTimelineKeepingState
  - TimelineWidget: cached segments/markers, paintEvent renders the
    new band, mouseMoveEvent + leaveEvent drive QToolTip

Docs
  - HELP.md: 25 sections; new §14 Safety profile, §17 Timeline editor.
  - README.md: highlights updated, picture placeholders documented.
  - CLAUDE.md: house rule that docs must mirror code changes.
  - SESSION.md: full handoff notes.

Tests
  102 / 102 passing (~70 ms).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

(Drop the Claude attribution line if you prefer.)
