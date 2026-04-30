# ZC95 Lua Builder — User Manual

> [!CAUTION]
> ## EXPERIMENTAL — UNTESTED ON REAL HARDWARE — USE AT YOUR OWN RISK
>
> **This project is in active development. Nothing here has been validated on an actual ZC95 device yet.**
>
> The form generator, the parser, the linter and the embedded simulator are exercised by the unit tests, but **no script produced by this tool has been flashed to real hardware.** Generated scripts may behave differently from what the simulator suggests, may exceed safe parameters, or may not load at all.
>
> **Treat every output as draft material. Review it line by line. Run it on a dummy load before any real session. The authors disclaim all liability for damage to equipment or harm to persons.**
>
> If you spot something wrong, open an issue. Until this banner is removed, assume nothing.

---

Complete reference for everything the application does. If something on
screen confuses you, this document is the place to look.

---

## Table of contents

1. [What is this thing?](#1-what-is-this-thing)
2. [Building and running](#2-building-and-running)
3. [The interface at a glance](#3-the-interface-at-a-glance)
4. [Workflows](#4-workflows)
5. [Tab reference (left pane)](#5-tab-reference-left-pane)
   1. [Config](#51-config)
   2. [Menu Items](#52-menu-items)
   3. [Functions](#53-functions)
   4. [Snippets](#54-snippets)
   5. [API Help](#55-api-help)
   6. [LCD Preview](#56-lcd-preview)
6. [Editor reference (right pane)](#6-editor-reference-right-pane)
7. [Simulator reference](#7-simulator-reference)
8. [Issues panel and the linter](#8-issues-panel-and-the-linter)
9. [Smart merge — what `Ctrl+G` actually does](#9-smart-merge--what-ctrlg-actually-does)
10. [Reverse parse — what `Ctrl+Shift+G` actually does](#10-reverse-parse--what-ctrlshiftg-actually-does)
11. [The embedded Lua simulator in detail](#11-the-embedded-lua-simulator-in-detail)
12. [Bundled official scripts](#12-bundled-official-scripts)
13. [Keyboard shortcuts](#13-keyboard-shortcuts)
14. [Settings, recent files, and persistence](#14-settings-recent-files-and-persistence)
15. [Portable Windows build](#15-portable-windows-build)
16. [Troubleshooting / FAQ](#16-troubleshooting--faq)
17. [Known limitations](#17-known-limitations)
18. [Glossary](#18-glossary)

---

## 1. What is this thing?

The [ZC95](https://github.com/CrashOverride85/zc95) is a 4-channel e-stim
controller that runs user-supplied Lua scripts to drive output patterns.
Writing those scripts by hand is doable but error-prone — easy to forget
a `menu_id`, mis-spell `audio_processing_mode`, or set a frequency above
the 300 Hz cap.

**ZC95 Lua Builder** does three things to make that easier:

1. **Form-driven script generation.** Fill in the Config, menu items and
   callbacks in the GUI; the application emits valid Lua.
2. **Static checking.** Range checks on every `zc.*` call, duplicate-ID
   detection, audio-mode consistency, etc. Errors and warnings appear in
   a dockable panel with a click-to-jump cursor.
3. **An embedded simulator.** A Lua 5.4 interpreter runs your script with
   `zc.*` stubbed out, drawing channel state on a live timeline so you
   can see *what* the script does before flashing it to the device.

The application can also open existing `.lua` files (yours or any of the
12 official scripts shipped with the firmware), parse the Config block
back into the form, and edit / re-save them.

---

## 2. Building and running

### Requirements

- **Qt 6.5+** with the `Widgets` and `Test` components.
- **CMake 3.16+**.
- A **C++17** compiler — MSVC 2019/2022, MinGW 11+, recent Clang or GCC.
- An **internet connection** the first time you run CMake (Lua 5.4.7 is
  fetched via FetchContent and built as a static library).

### Building on Windows (MSVC, command line)

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
build\zc95-lua-builder.exe
```

If you don't already have `cl.exe` on PATH, run from a *Developer Command
Prompt for VS 2022* or use the included `_build.bat` which calls
`vcvarsall.bat x64` for you.

### Building with MinGW

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64
cmake --build build
```

### Building on Linux / macOS

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build -j
./build/zc95-lua-builder
```

### Running the test suite

```bash
ctest --test-dir build --output-on-failure
```

Should print `42 passed, 0 failed` in roughly 25ms. Tests cover:

- Generator → parser round-trip on synthetic and real scripts.
- Brace matching and Config block extraction.
- Smart-merge preserving function bodies.
- Linter rule firing on every error class.
- Variable name sanitisation and string escaping.

### Disabling tests

```bash
cmake -S . -B build -DZC95_BUILD_TESTS=OFF ...
```

### Producing a portable Windows build

After a successful Release build, run `_deploy.bat` at the repo root.
It calls `windeployqt`, copies the MSVC runtime DLLs directly, and
produces `dist\zc95-lua-builder\` — a self-contained folder you can zip
and distribute.

---

## 3. The interface at a glance

```
+---------------------------------------------------------------------------+
| File   Edit   Generate   Simulator   Presets   View   Help                |  Menu bar
+---------------------------------------------------------------------------+
| New | Open | Save | Regen (Ctrl+G) | Reparse (Ctrl+Shift+G)               |  Toolbar
+--------------+------------------------------+---------------------------+-+
|              |                              |                           |
| [Config]     |  [Editor] [Simulator]        |                           |
| [Menu Items] |                              |                           |
| [Functions]  |  pattern.lua                 |                           |
| [Snippets]   |                              |                           |
| [API Help]   |  ...                         |                           |
| [LCD Prev.]  |                              |                           |
|              |                              |                           |
| (left tabs)  | (editor + find bar OR full   |                           |
|              |  simulator UI)               |                           |
|              |                              |                           |
+--------------+------------------------------+---------------------------+
| Issues  ⚠ L42  zc.SetPower power 2000 out of range (0-1000)              |  Dock (toggle in View)
+---------------------------------------------------------------------------+
| Ready                                  [● in sync]  | pattern.lua *      |  Status bar
+---------------------------------------------------------------------------+
```

Three resizable areas:

- **Left tabs** — form views (Config, Menu Items, Functions, Snippets,
  API Help, LCD Preview).
- **Right tabs** — the Lua editor, and the simulator.
- **Bottom dock** — Issues panel (linter output). Hide / show via *View*.

A horizontal splitter between the left tabs and the right tabs lets you
balance how much screen space the form vs. the editor get. The split is
remembered between sessions.

The status bar shows:
- Transient messages (left).
- A **sync badge** (right): green `● in sync` when the editor's content
  matches what the form would generate; orange `● form/code differ`
  when they've diverged.

The window title shows the current file name with an asterisk suffix
when there are unsaved changes.

---

## 4. Workflows

### Start fresh

1. `File → New` (or just launch the app).
2. **Config** tab: name your pattern, pick an audio mode if needed.
3. **Menu Items**: click `Add…` for each user-tunable option.
4. **Functions**: tick the callbacks you'll need (`Setup`, `MinMaxChange`,
   `SoftButton`, …).
5. Press `Ctrl+G`. The right pane fills with a skeleton Lua script.
6. Switch to **Editor**, write the body of `Loop(time_ms)` (and any other
   callback). Use **Snippets** or **API Help** to insert boilerplate.
7. Press `Ctrl+L`. Fix any errors / warnings shown in **Issues**.
8. Switch to the **Simulator** tab and press **Run** to watch your
   pattern execute.
9. `File → Save As…` → choose `.lua` filename.

### Start from an official preset

1. `Presets → Official scripts → Climb` (or any of 12).
2. The Lua source loads in the editor; the Config block is parsed back
   into the form; the simulator silently receives the script too.
3. Switch to **Simulator**, press **Run**.
4. Modify a frequency or a delay, press **Reset** then **Run** again.
5. To save the modified version: `File → Save As…`.

### Open an existing file

- Drag-drop the `.lua` onto the window, **or**
- `File → Open…`, **or**
- `File → Recent Files → 1 …`.

The editor is populated, the parser tries to read the Config block back
into the form (a status-bar note tells you if it had partial success),
and the simulator is primed.

### Iterate on Config without losing your code

1. Change menu items / Config in the form.
2. Press `Ctrl+G` (smart merge). Only the `Config = {…}` block is
   rewritten. Your function bodies are untouched.
3. The sync badge flips back to `● in sync`.

If you ever want a clean re-emit (overwriting everything you wrote), use
`Ctrl+Shift+Alt+G`. A confirmation dialog warns you it's destructive.

---

## 5. Tab reference (left pane)

### 5.1 Config

The whole-pattern metadata that ends up at the top of the `Config = {…}`
block.

| Field | Lua key | Notes |
|-------|---------|-------|
| **Name** | `name` | Shown on the device LCD. Mandatory. |
| **Audio mode** | `audio_processing_mode` | `OFF` or `AUDIO_INTENSITY`. Required if you tick `AudioIntensityChange()` or use an `AUDIO_VIEW_INTENSITY_*` menu item. |
| **Soft button label** | `soft_button` | Empty = no soft button. Required if you tick `SoftButton()`. |
| **Loop frequency (Hz)** | `loop_freq_hz` | `0` = device default ("as fast as possible"). Otherwise throttles `Loop()` to N times per second. |
| **Allow triphase** | `allow_triphase` | Required to use `zc.EnableTriphase` / `zc.LinkChannels`. Disables channel isolation — the device will let pulses on different channels overlap. |
| **Bluetooth remote passthrough** | `bluetooth_remote_passthrough` | Required to receive `BluetoothRemoteKeypress`. |

Any change to any field marks the document dirty and emits a status-bar
hint reminding you to press `Ctrl+G`.

### 5.2 Menu Items

The list of user-tunable items shown on the device LCD when the pattern
is running.

Buttons:
- **Add…** — opens the Item dialog with a fresh ID assigned (next free).
- **Edit…** (or double-click) — re-opens an existing item.
- **Duplicate** — copies the selected item with a new ID.
- **Remove** — deletes the selected item.
- **↑** / **↓** — reorder. Order is preserved in the generated `menu_items`.

The dialog has four type-specific pages:

#### MIN_MAX
Continuous integer in `[min, max]` with an `increment_step`, a unit
string (`uom`, free-form: `ms`, `Hz`, `%`, …), and a `default`. The
generator declares a top-level Lua variable named after the item title
(e.g. `_delay_ms`) and pre-fills `MinMaxChange()` with an `if/elseif`
chain that assigns `min_max_val` to that variable.

#### MULTI_CHOICE
A list of `{choice_id, description}` pairs. The generator pre-fills
`MultiChoiceChange()` with an `if/elseif` chain mirroring the IDs.

#### AUDIO_VIEW_INTENSITY_STEREO / _MONO
Decorative LCD widgets that show the current audio level. No additional
parameters.

**ID uniqueness** is enforced by the dialog — saving with a duplicate ID
re-opens the dialog with a warning.

### 5.3 Functions

Checkboxes for every callback the device firmware can invoke. Ticking
one tells the generator to emit a stub for that function. Bodies are
preserved across regenerations (smart merge).

| Checkbox | Lua function | Required for |
|----------|--------------|--------------|
| **Setup()** | `Setup` | Initial channel setup (power, frequency). Optional — defaults are used if absent. |
| **Loop(time_ms)** | `Loop` | **Mandatory** — the periodic callback. Disabled, always on. |
| **MinMaxChange(menu_id, val)** | `MinMaxChange` | Receive `MIN_MAX` slider changes. |
| **MultiChoiceChange(menu_id, choice_id)** | `MultiChoiceChange` | Receive `MULTI_CHOICE` selection changes. |
| **SoftButton(pushed)** | `SoftButton` | Top-left soft button events. Set the **Soft button label** in Config too. |
| **ExternalTrigger(socket, part, active)** | `ExternalTrigger` | TRIGGER1/2 (parts A and B) on the trigger inputs. |
| **BluetoothRemoteKeypress(key)** | `BluetoothRemoteKeypress` | Paired BT remote. Tick **Bluetooth remote passthrough** in Config. |
| **BluetoothHidEvent(usage_page, usage, value)** | `BluetoothHidEvent` | Raw HID events from a paired BT device. |
| **AudioIntensityChange(L, R, virt)** | `AudioIntensityChange` | Real-time audio intensity. Set **Audio mode = AUDIO_INTENSITY**. |

### 5.4 Snippets

Two groups of one-click snippet inserters. The current cursor position
in the editor is the insertion point.

**`zc.*` API** — one snippet per `zc.*` function with a sensible default
example, plus a `print(...)` helper.

**Patterns idiomatiques** — common multi-line patterns:

- **Init 4 channels** — `for chan = 1, 4 do … zc.ChannelOn / SetPower /
  SetFrequency / SetPulseWidth … end`.
- **Toggle every N ms** — gate a block on `time_ms - last >= delay`.
- **244 Hz tick** — re-run a block roughly 244 times per second inside
  `Loop()`.
- **Triangle / Sine wave modulator** — modulate a value between
  `[low, high]` over a configurable period (in ms).
- **Mode / MenuId enum tables** — named constants for menu items and
  their multi-choice options.
- **Triphase fade cycle** — cycle the offset 0 → 100 → 0 to morph from
  monophase to triphase and back.
- **Burst loop** — fire short pulses on all 4 channels at a configurable
  burst frequency.
- **SetFreq helper / SetWidth helper** — apply a frequency or pulse
  width to all 4 channels at once.

Every snippet is committed to the editor as plain text — they're meant
as starting points, not finished code.

### 5.5 API Help

Top half: an alphabetical list of every `zc.*` function and every
firmware callback (`Setup`, `Loop`, `MinMaxChange`, `MultiChoiceChange`,
`SoftButton`, `ExternalTrigger`, `BluetoothRemoteKeypress`,
`BluetoothHidEvent`, `AudioIntensityChange`, `print`).

Bottom half: rendered docs with signature, description, parameter list
and example for the selected entry.

The **Insert example** button drops the example snippet into the editor
at the cursor.

### 5.6 LCD Preview

A live mock-up of how the ZC95 LCD will display your pattern:

- Title bar: `U: <name>` (`U:` like the device shows for user scripts).
- One row per menu item:
  - **MIN_MAX** — bar graph filled to `(default - min) / (max - min)`,
    with the value and unit on the right.
  - **MULTI_CHOICE** — `< first_choice_description >` (only the first
    option is shown in the static preview).
  - **AUDIO_VIEW_INTENSITY_*** — pseudo-waveform fill.
- Footer: the soft-button label in `[brackets]`, if any.
- `+N more` if the menu has more items than fit on screen.

The preview re-paints on every form change, so you can tune the layout
without leaving the form.

---

## 6. Editor reference (right pane)

A `QPlainTextEdit` subclass with the following features:

### Syntax highlighting

Six colour groups (VS-Code Dark+ palette by default):

- **Keywords** (blue, bold): `and`, `break`, `do`, `else`, `elseif`,
  `end`, `false`, `for`, `function`, `goto`, `if`, `in`, `local`, `nil`,
  `not`, `or`, `repeat`, `return`, `then`, `true`, `until`, `while`.
- **`zc.*`** (yellow): `zc.ChannelOn`, `zc.SetPower`, `zc.SetFrequency`,
  etc. — anything matching `\bzc\.[A-Za-z_]+\b`.
- **Callbacks** (cyan, bold): `Setup`, `Loop`, `MinMaxChange`,
  `MultiChoiceChange`, `SoftButton`, `ExternalTrigger`,
  `BluetoothRemoteKeypress`, `BluetoothHidEvent`,
  `AudioIntensityChange`, `Config`, `print`.
- **Numbers** (light green): integer or decimal literals.
- **Strings** (orange): `"…"` and `'…'` with `\` escapes.
- **Comments** (green, italic): `-- line comments` and `--[[ block ]]`.

### Line numbers

Drawn in a gutter on the left. The gutter resizes itself when the line
count grows past 9 / 99 / 999.

### Current-line highlight

The block your cursor is in is painted with a slightly lighter
background so you can find your place at a glance.

### Autocomplete

Triggered:
- Automatically after typing 2+ characters of an identifier.
- Manually with `Ctrl+Space`.

Completion sources (~50 entries):
- Every `zc.*` function.
- Every callback name.
- Every Lua keyword.
- Common ZC95 string constants — `"TRIGGER1"`, `"A"`, `"KEY_UP"`,
  `"AUDIO_INTENSITY"`, `"MIN_MAX"`, `"MULTI_CHOICE"`,
  `"AUDIO_VIEW_INTENSITY_*"`, etc.

`Enter` or `Tab` accepts. `Esc` dismisses.

### Find / Replace bar

`Ctrl+F` opens a one-row Find bar. `Ctrl+H` opens a two-row
Find + Replace bar. Buttons:

- **◀ Prev** / **Next ▶** — find previous / next occurrence. Wraps
  around the document automatically and shows `wrapped` in the bar.
- **Aa** — toggle case sensitivity.
- **W** — match whole words only.
- **Replace** — replace the current selection if it matches the search
  term (under the current case-sensitivity flag), then jump to the next.
- **All** — replace every match in the document, count shown in the bar.
- **✕** — close the bar and return focus to the editor.

`Esc` while the bar has focus closes it.

`F3` / `Shift+F3` find next / previous **without** opening the bar
(works as long as you've used `Ctrl+F` once in this session).

### Drag-drop

Drop one or more `.lua` files anywhere on the application window — the
first `.lua` file is opened (after the unsaved-changes guard).

---

## 7. Simulator reference

Open the **Simulator** tab on the right pane.

```
[ ▶ Run ] [ ⏸ Pause ] [ ⟲ Reset ] [ Step ]    [ Soft Btn ] [ Trigger 1A ]
                            speed ×1.00   20 ms/tick   t = 2.300s

  Channels
  CH1  ON  pwr=850  150Hz  150us
  CH2  OFF pwr=1000
  CH3  OFF pwr=1000
  CH4  OFF pwr=1000

  ┌────────────────────────────────────────────────────────────┐
  │ CH1  ▓▓░░▓▓░░▓▓░░                          (timeline)      │
  │ CH2                                                        │
  │ CH3                                                        │
  │ CH4                                                        │
  │  0.0s    1.0s    2.0s    3.0s    4.0s    5.0s              │
  └────────────────────────────────────────────────────────────┘

  [INFO] Reset.
  [LUA]  Climb mode initialised
  ...
```

### Toolbar

| Button | Action |
|--------|--------|
| **▶ Run** | Calls `Setup()` once (first time after Reset/Load), then ticks `Loop(time_ms)` continuously. If no script is loaded, the current editor is auto-loaded silently. |
| **⏸ Pause** | Stops the timer. State is preserved — press Run again to resume. |
| **⟲ Reset** | Stops the timer, resets simulated time to 0, clears events, re-arms `Setup()` for the next Run. Does **not** re-load the source — use Ctrl+R for that. |
| **Step** | Advance one tick (one `Loop` call) without auto-running. |
| **Soft Btn** | While held, calls `SoftButton(true)`; on release, `SoftButton(false)`. |
| **Trigger 1A** | One-shot: `ExternalTrigger("TRIGGER1", "A", true)` immediately followed by `(false)`. |
| **speed ×N** | Multiplier on simulated time per tick (default ×1.0). At ×10 the script runs 10× faster than wall-clock. |
| **N ms/tick** | How much simulated time advances per tick (default 20ms). Smaller = finer resolution but slower walking. |
| **t = …s** | Current simulated time, monospaced. |

### Channel state row

For each of the 4 channels the panel shows:

- **State**: `ON` (green pill) or `OFF` (grey pill).
- **pwr=N** — last `zc.SetPower(ch, N)` value.
- When ON: also `freqHz` and `pulse_width_us`.

### Timeline

- 4 horizontal lanes (CH1 to CH4).
- Green segments = sustained ON ranges (between `ChannelOn` and
  `ChannelOff`).
- Orange segments = `ChannelPulseMs` events with their declared duration.
- Dashed vertical line = "now". The timeline is a 5s sliding window.
- X-axis: seconds with one decimal.

### Log

Below the timeline:
- `[INFO]` — internal events (Reset, script loaded, …).
- `[OK]` — successful loads.
- `[ERROR]` — Lua errors during `Setup()` or `Loop()`.
- `[LUA]` — output from `print()` calls in the script.
- `[SetMenuOption] id=… value=…` — diagnostic when the script calls
  `zc.SetMenuOption` (the simulator doesn't actually re-fire the
  callbacks, just logs the call).

### How to load a script into the simulator

The simulator is fed the editor's content automatically when:

- A new script is created (`File → New`).
- A `.lua` is opened (`File → Open` or `Recent Files`).
- A `.lua` is dropped onto the window.
- One of the **Presets** is loaded (quick or official).
- You switch to the **Simulator** tab for the first time after launch.
- You press **Run** or **Step** while no script has been loaded yet.

If you've edited the script in the editor, the simulator keeps the
**previous** load — press `Ctrl+R` (Simulator → Load editor into
simulator) to refresh it. Run/Step always operate on the last loaded
copy, never on stale text from the editor.

---

## 8. Issues panel and the linter

`Ctrl+L` runs the linter. Results appear in the **Issues** dock at the
bottom of the window. Double-click an entry to jump to the corresponding
line in the editor (entries with no specific line just give a hint).

### Severities

- **✗ ERROR** (red) — script will misbehave or fail to load on the
  device. Fix before flashing.
- **⚠ WARN** (orange) — likely mistake but not strictly invalid.
- **ℹ INFO** (blue) — informational note, not actionable.

The header summarises counts (`2 error(s), 1 warning(s), 4 info`) and
turns red when errors are present.

### Rules — Config consistency

| Code condition | Severity | Message |
|----|----|----|
| `Config.name` empty or whitespace | Warning | "Pattern name is empty." |
| Two menu items share the same `id` | Error | "Duplicate menu_item id N." |
| Menu item title empty | Warning | "Menu item id N has empty title." |
| `MIN_MAX` `min ≥ max` | Error | "min ≥ max". |
| `MIN_MAX` `default` outside `[min, max]` | Warning | "default outside range". |
| `MIN_MAX` `increment_step ≤ 0` | Error | "increment_step must be > 0". |
| `MULTI_CHOICE` with no choices | Warning | "no choices". |
| `MULTI_CHOICE` duplicate `choice_id` | Error | "duplicate choice_id". |
| `AudioIntensityChange()` ticked but mode is `OFF` | Error | "audio_processing_mode is OFF". |
| `AUDIO_VIEW_INTENSITY_*` item but mode is `OFF` | Warning | "won't display anything useful". |
| `BluetoothRemoteKeypress()` ticked but `bluetooth_remote_passthrough = false` | Warning | "callback never called". |
| `SoftButton()` ticked but `soft_button` label empty | Warning | "button not visible on screen". |

### Rules — source range checks

The linter scans the editor source (with comments and string contents
stripped) and verifies argument ranges:

| Pattern | Check |
|----|----|
| `zc.SetPower(ch, pwr)` | `1 ≤ ch ≤ 4`, `0 ≤ pwr ≤ 1000`. |
| `zc.SetFrequency(ch, hz)` | `1 ≤ ch ≤ 4`, `1 ≤ hz ≤ 300`. |
| `zc.SetPulseWidth(ch, pos, neg)` | `1 ≤ ch ≤ 4`, `0 ≤ pos, neg ≤ 255`. |
| `zc.ChannelOn(ch)` / `ChannelOff(ch)` | `1 ≤ ch ≤ 4`. |
| `zc.ChannelPulseMs(ch, ms)` | `1 ≤ ch ≤ 4`, `ms ≥ 0`. |
| `zc.AccIoWrite(line, …)` | `1 ≤ line ≤ 3`. |
| `zc.DelayMs(ms)` | `0 ≤ ms ≤ 10000`. |
| `zc.LinkChannels(lead, linked, off)` | `1 ≤ lead ≤ 4`, `0 ≤ linked ≤ 4`, `0 ≤ off ≤ 100`. |

### Rules — semantic

| Condition | Severity | Message |
|----|----|----|
| `zc.EnableTriphase` or `zc.LinkChannels` used but `allow_triphase = false` | Error | "Triphase function used but allow_triphase is not set." |
| `if menu_id == N` or `zc.SetMenuOption(N, …)` for an `N` not present in `menu_items` | Warning | "menu_id N referenced but no matching menu_item." |
| `module(…)` — Lua 5.1 idiom, polyfilled | Info | "simulator polyfills it" |
| `package.seeall` — Lua 5.1 idiom, polyfilled | Info | "simulator polyfills it" |
| `require("xxx")` for `xxx ≠ "ettot"` | Info | "provided by the ZC95 firmware but not by the embedded simulator." |
| No top-level `function Loop(` | Warning | "No top-level Loop(time_ms) found." |

`require("ettot")` is silently allowed because the application bundles
`ettot.lua` as a Qt resource; the simulator's custom searcher resolves
it.

---

## 9. Smart merge — what `Ctrl+G` actually does

`Ctrl+G` (Generate → Regenerate code from form) is the everyday flow.

### Steps

1. The form's state is collected into a `ScriptConfig` object.
2. `LuaGenerator::mergeIntoSource(config, editorText)` is called.
3. It looks for the existing `Config = { … }` block in the editor by:
   - blanking out string and comment contents (preserving newlines so
     line numbers don't drift),
   - matching `\bConfig\s*=\s*\{` once, then walking the brace stack
     until the matching `}`.
4. If a Config block is found:
   - The text between (and including) `Config` and the closing `}` is
     replaced with a fresh emit of `generateConfigBlock(config)`.
   - For every callback that's now ticked in **Functions** but not yet
     defined in the source, an empty stub is appended at the end.
   - Everything else — your code, comments, locals, helpers — is
     preserved verbatim.
5. If no Config block is found in the source, the merge falls back to
   a full generation (same as `Ctrl+Shift+Alt+G`).
6. The cursor is moved to the same character offset it had before
   (clamped to the new length).
7. The status bar shows "Regenerated Config block (function bodies
   preserved)".

### What's preserved

- All function bodies and their signatures.
- All comments outside the Config block.
- All `local` and global variable declarations.
- All `require` statements.
- Indentation and blank lines outside the Config block.

### What's replaced / added

- The `Config = { … }` block, in full.
- Stubs for newly-ticked callbacks (only if they don't already exist).

### Smart vs. dumb regenerate

| | `Ctrl+G` (smart) | `Ctrl+Shift+Alt+G` (dumb) |
|---|---|---|
| Replaces Config block | yes | yes |
| Preserves function bodies | **yes** | no — overwritten with stubs |
| Adds new function stubs | yes (for newly ticked callbacks) | yes |
| Confirms before destruction | no — non-destructive | yes (modal warning) |
| Regenerates header / variable decls | only inside Config | yes (everything) |

Use `Ctrl+G` for normal iteration, `Ctrl+Shift+Alt+G` for "I want to
start over but keep my Config".

---

## 10. Reverse parse — what `Ctrl+Shift+G` actually does

`Ctrl+Shift+G` (Generate → Reparse form from editor) does the opposite
of `Ctrl+G`: it reads the editor and updates the form to match.

### Steps

1. The editor source is passed to `LuaParser::parse(source)`.
2. The parser:
   - Strips comments (line `--…` and block `--[[…]]`) by replacing them
     with spaces while preserving newlines.
   - Walks the text looking for `\bConfig\s*=\s*\{`.
   - Brace-matches to the closing `}`.
   - Splits the inner table into top-level `key = value` pairs (a tiny
     state machine that respects nested braces, single and double
     quoted strings, and `\` escapes inside strings).
   - Reads `name`, `audio_processing_mode`, `soft_button`,
     `loop_freq_hz`, `allow_triphase`, `bluetooth_remote_passthrough`,
     `menu_items`.
   - For each `menu_items` entry, recursively parses the inner table
     to fill a `MenuItem` (type, id, title, group, min/max/step/uom/
     default for MIN_MAX, choices for MULTI_CHOICE).
3. Top-level `function Foo(…)` definitions are also detected so the
   **Functions** panel is populated correctly.
4. The form is updated. The sync badge flips back to `● in sync`.

### Caveats

- Values that depend on a Lua expression (e.g. `default = _delay_ms`)
  are read as `0` because the parser is literal — it doesn't run Lua.
- Long-bracket comments (`--[==[ … ]==]`) at level ≥ 1 are not stripped.
  Levels 0 (`--[[ … ]]`) and line comments are.
- The parser is intentionally lenient: if it can't find a Config block,
  it returns `ok = true` with a warning in the status bar rather than
  failing. The function-detection pass still runs.

---

## 11. The embedded Lua simulator in detail

### Architecture

- One `lua_State` per `LuaRuntime` instance. Shared between Setup, Loop
  and all callbacks — globals persist across calls, just like on the
  device.
- `luaL_openlibs()` is called: standard libraries (`math`, `string`,
  `table`, `io`, `os`, `debug`, `package`) are available.
- `print()` is overridden to capture output instead of writing to
  stdout — log lines surface in the simulator log prefixed with `[LUA]`.
- The `zc.*` table is registered with C trampolines — see below.

### `zc.*` stubs

| Function | Effect on simulator state | Event emitted |
|----------|---------------------------|---------------|
| `zc.ChannelOn(ch)` | sets `channels[ch].on = true` | `ChannelOn` |
| `zc.ChannelOff(ch)` | sets `channels[ch].on = false` | `ChannelOff` |
| `zc.ChannelPulseMs(ch, ms)` | sets `on = true`, `pulseEndsAtMs = now + ms` | `ChannelPulseMs` |
| `zc.SetPower(ch, pwr)` | sets `channels[ch].power = pwr` | `SetPower` |
| `zc.SetFrequency(ch, hz)` | sets `channels[ch].frequencyHz` | `SetFrequency` |
| `zc.SetPulseWidth(ch, pos, neg)` | sets pulse width on the channel | `SetPulseWidth` |
| `zc.SetMenuOption(id, val)` | logs (does **not** re-fire callbacks) | — |
| `zc.DelayMs(ms)` | advances simulated time by `ms` | `DelayMs` |
| `zc.EnableTriphase(b)` | sets `triphaseEnabled` | `EnableTriphase` |
| `zc.LinkChannels(lead, linked, off)` | logs the link | `LinkChannels` |
| `zc.AccIoWrite(line, state)` | sets `accIo[line]` | `AccIoWrite` |

After every `Loop` tick the runtime checks `pulseEndsAtMs` and emits an
implicit `ChannelOff` for any pulse that has expired.

### Lua 5.1 compatibility (polyfill)

The official scripts use idioms removed in Lua 5.2. The runtime ships
a small Lua-side polyfill, run once at startup:

- `module(name [, modifier1, modifier2…])`
  1. Creates `M = package.loaded[name] or _G[name] or {}`.
  2. Sets `_G[name] = M`, `package.loaded[name] = M`.
  3. Runs every modifier (e.g. `package.seeall`) on `M`.
  4. Walks the calling chunk's upvalues with `debug.getupvalue`,
     finds the one called `_ENV`, and rebinds it to `M` with
     `debug.setupvalue`. From that point on, bare assignments inside
     the file go into `M`, just like in 5.1.
- `package.seeall(t)` — `setmetatable(t, {__index = _G})`.

### Custom `package.searchers` for Qt resources

The runtime inserts a custom searcher at index 2 (right after
`package.preload`). For `require("foo")` it tries:

1. `:/presets/lib/foo.lua`
2. `:/presets/lib/foo/init.lua`
3. `:/presets/foo.lua`

`foo.bar` becomes `foo/bar`. If none match, it falls through to the
remaining searchers.

The bundle includes `lua_Script/lib/ettot.lua`, embedded as
`:/presets/lib/ettot.lua` via `resources/presets.qrc`. So
`require("ettot")` works in the simulator without any local install.

### What the simulator does NOT do

- **Real waveforms.** `zc.ChannelOn` doesn't generate the actual
  pulses; it just flips a flag.
- **Channel isolation.** Triphase / non-triphase has no electrical
  effect — only a logged event.
- **Audio capture.** `AudioIntensityChange` is never fired automatically
  (you'd need to feed it manually).
- **Front-panel dial scaling.** `SetPower(ch, 1000)` is recorded as
  1000, not "1000 × dial position".

The simulator is a **logic** validator. For the physics, you'll need
the actual hardware.

---

## 12. Bundled official scripts

`Presets → Official scripts` exposes 12 scripts from the upstream ZC95
firmware repository, embedded as Qt resources (no external files
needed):

| Script | Description |
|--------|-------------|
| **climb** | Slowly rises in frequency over the configured period. |
| **combo** | Combination mode. |
| **intense** | Aggressive bursts. |
| **orgasm** | Build-up + sustain pattern. |
| **phasing2** | Frequency phasing across channels. |
| **random2** | Random parameter walks. |
| **rhythm** | Rhythmic gated bursts. |
| **stroke** | Stroking pattern. |
| **tens** | TENS-style steady output. |
| **torment** | Variable-intensity teasing. |
| **trifade** | Triphase fade cycle. |
| **waves** | Smooth wave modulator. |

All 12 are covered by the `officialScriptsParse` /
`officialScriptsRoundtrip` test cases — opening them, parsing the
Config block back into the form, and re-emitting yields a structurally
equivalent script.

Most of them depend on `require("ettot")` and use `module(…,
package.seeall)` — both are handled transparently by the simulator
(see §11).

The four "quick presets" under `Presets → Toggle / Fire / Waves /
Audio` are minimal hand-coded examples meant as starting points for new
patterns.

---

## 13. Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New script |
| `Ctrl+O` | Open `.lua` |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As… |
| `Ctrl+Q` | Quit |
| `Ctrl+G` | Regenerate code from form (smart merge) |
| `Ctrl+Shift+Alt+G` | Regenerate from scratch (overwrite, with confirm) |
| `Ctrl+Shift+G` | Re-parse form from editor |
| `Ctrl+L` | Run linter |
| `Ctrl+R` | Reload editor into simulator |
| `Ctrl+F` | Find… |
| `Ctrl+H` | Replace… |
| `F3` | Find next |
| `Shift+F3` | Find previous |
| `Ctrl+Space` | Trigger autocomplete |
| `Esc` | Close find/replace bar / dismiss completer popup |
| `Tab` / `Enter` | Accept completion (when popup is open) |

---

## 14. Settings, recent files, and persistence

The application uses `QSettings` with organisation `zc95`,
application `lua-builder`. On Windows that maps to the registry under
`HKCU\Software\zc95\lua-builder`; on Linux to `~/.config/zc95/lua-builder.conf`;
on macOS to `~/Library/Preferences/com.zc95.lua-builder.plist`.

Persisted values:

- `mainwindow/geometry` — window size and position.
- `mainwindow/state` — toolbar / dock layout.
- `mainwindow/splitter` — horizontal split between left tabs and
  right pane.
- `recentFiles` — the last 8 file paths that were opened. Oldest are
  dropped automatically. **File → Recent Files → Clear list** wipes
  the list.

To reset to defaults: close the app, delete the registry key (Windows)
or the config file (Linux/macOS), reopen.

---

## 15. Portable Windows build

Run `_build.bat` then `_deploy.bat`. The result is
`dist\zc95-lua-builder\` with:

```
dist/zc95-lua-builder/
├── zc95-lua-builder.exe
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Network.dll
├── Qt6Svg.dll
├── Qt6Widgets.dll
├── vcruntime140.dll      <- MSVC runtime DLLs (no installer needed)
├── vcruntime140_1.dll
├── msvcp140.dll
├── msvcp140_1.dll
├── msvcp140_2.dll
├── dxcompiler.dll        <- DirectX (Qt RHI)
├── dxil.dll
├── generic/              <- Qt plugins
├── iconengines/
├── imageformats/
├── networkinformation/
├── platforms/
├── styles/
└── tls/
```

About 43 MB. Zip the folder and run `zc95-lua-builder.exe` on any
Windows 10/11 x64 — no Qt install, no Visual Studio install, no
dependencies on the host.

If `_deploy.bat` fails with **"Access is denied"**, you have an
instance of the app running from `dist\` — close it first.

---

## 16. Troubleshooting / FAQ

### Build issues

**`'type_traits': No such file or directory`** during compilation
(MSVC). The Visual Studio environment isn't activated. Either run from
a *Developer Command Prompt* or use `_build.bat`.

**`Could not find a configuration file for package "Qt6"`.** Pass
`-DCMAKE_PREFIX_PATH=…/Qt/6.x.y/<kit>` on the cmake command line, or
`export CMAKE_PREFIX_PATH=…` first.

**Lua download fails on first configure.** You're offline. The first
configure needs internet to fetch Lua 5.4.7. After that, `_deps/`
caches it.

### Runtime issues

**Simulator says "no script loaded yet" when I press Run.** Should be
auto-loaded now. If not: press `Ctrl+R`. If still not, check the
**Issues** dock for a Lua syntax error in the editor that prevented
the silent load.

**"module 'ettot' not found"** in the simulator. The polyfill or the
resource searcher isn't running. Make sure you rebuilt after pulling
recent commits (the searcher lives in `LuaRuntime::installResourceSearcher`).

**`function Loop` not defined** error during Run. Tick **Loop** in the
Functions panel and press `Ctrl+G`, or write the function in the
editor by hand.

**"Triphase function used but allow_triphase is not set."** Linter
error. Open the **Config** tab and tick **Allow triphase**, or remove
the `zc.EnableTriphase` / `zc.LinkChannels` calls.

### Workflow

**I edited a script in the editor; the LCD preview shows old data.**
Press `Ctrl+Shift+G` to re-parse the form from the editor. The LCD
preview only reads from the form, not from the editor source.

**I added a callback in the Functions panel; pressing `Ctrl+G` doesn't
add the function body.** Smart merge only adds **stubs** for callbacks
that aren't yet defined. If the callback was already in the source
(maybe you wrote it, then unticked, then re-ticked), the stub won't be
re-emitted. Use `Ctrl+Shift+Alt+G` to force a full regeneration, or
just write the function manually.

**Opening one of the official scripts fills the form with weird
values.** The parser reads literal values; `default = _intensity_value`
becomes `0`. The script will still run correctly because the actual
default is set in `Setup()` or via `MinMaxChange`. The form is only
informational here.

**The sync badge stays orange even right after `Ctrl+G`.** This can
happen if your editor source contains constructs the parser can't read
back identically (e.g. expressions in `default`). The merge **does**
work — the badge is conservative.

### Simulator

**Channels show ON but the timeline lane is empty.** You're seeing the
state at `t = 0` before any events arrive. Press Run for at least a
tick.

**The script crashes on the first call to `zc.SetPower` with
"attempt to index a nil value (global 'zc')".** You're loading code
that overrode the global `zc` (e.g. a malformed `module()` call). Try
again with a clean editor.

---

## 17. Known limitations

- **Lua parser is literal**, not evaluating. Expression values become 0.
- **Long-bracket comments at level ≥ 1** (`--[==[ … ]==]`) aren't
  stripped by the highlighter.
- **Round-trip preserves Config block contents** but reflows whitespace
  and comments inside the block (the rest of the file is byte-preserved
  by smart merge).
- **Simulator UI** has no buttons for `MinMaxChange`,
  `MultiChoiceChange`, `Bluetooth*`, `AudioIntensityChange`, or
  `Trigger 1B / 2A / 2B` yet. They can still be exercised by `Step` if
  the script invokes them itself.
- **No undo across regeneration.** `Ctrl+G` and `Ctrl+Shift+Alt+G`
  invalidate the editor's undo history (Qt limitation when
  `setPlainText` is used). Save before regenerating risky changes.

---

## 18. Glossary

- **Callback** — a Lua function the firmware (or simulator) calls in
  response to an event: `Setup`, `Loop`, `MinMaxChange`,
  `MultiChoiceChange`, `SoftButton`, `ExternalTrigger`,
  `BluetoothRemoteKeypress`, `BluetoothHidEvent`,
  `AudioIntensityChange`.
- **Config block** — the top-level Lua table assigned to `Config`. Read
  by the firmware before launching the pattern. Contains `name`,
  `audio_processing_mode`, `soft_button`, `loop_freq_hz`,
  `allow_triphase`, `bluetooth_remote_passthrough`, `menu_items`.
- **Menu item** — a row in the device's pattern menu. Three kinds:
  `MIN_MAX`, `MULTI_CHOICE`, `AUDIO_VIEW_INTENSITY_*`.
- **`menu_id`** — the integer ID assigned to a menu item, used by
  `MinMaxChange` / `MultiChoiceChange` and `zc.SetMenuOption` to
  identify which item was changed.
- **Triphase** — non-isolated mode where pulses on different channels
  are allowed to overlap. Requires `allow_triphase = true` in Config.
- **Smart merge** — selective regeneration of just the Config block,
  preserving everything else in the source. The default behaviour of
  `Ctrl+G`.
- **Sync indicator** — the green/orange badge in the status bar showing
  whether the editor's Lua matches what the form would generate.
- **`zc.*`** — the namespace exposed to scripts by the firmware (and
  stubbed by the simulator). All hardware control goes through this
  table.
- **ettot** — `lua_Script/lib/ettot.lua`, a helper library used by most
  official scripts. Bundled as a Qt resource and resolved by the
  simulator's custom `package.searchers` entry.
- **`_ENV`** — the implicit upvalue holding a chunk's environment in
  Lua 5.2+. The simulator's `module()` polyfill rebinds it on the
  calling chunk to emulate the 5.1 behaviour.

---

*If something is missing or wrong, open an issue. — End of HELP.*
