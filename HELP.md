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
13. [Beginner mode and the new-pattern wizard](#13-beginner-mode-and-the-new-pattern-wizard)
14. [Safety profile (locked caps)](#14-safety-profile-locked-caps)
15. [Pre-flight check](#15-pre-flight-check)
16. [Explain this script](#16-explain-this-script)
17. [Timeline editor](#17-timeline-editor)
18. [▶ Test buttons on menu items](#18--test-buttons-on-menu-items)
19. [Auto-save and draft recovery](#19-auto-save-and-draft-recovery)
20. [Keyboard shortcuts](#20-keyboard-shortcuts)
21. [Settings, recent files, and persistence](#21-settings-recent-files-and-persistence)
22. [Portable Windows build](#22-portable-windows-build)
23. [Troubleshooting / FAQ](#23-troubleshooting--faq)
24. [Known limitations](#24-known-limitations)
25. [Glossary](#25-glossary)

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
| [Config]     |  [Editor] [Simulator] [TL Beta] |                        |
| [Menu Items] |                              |                           |
| [Functions]  |  pattern.lua                 |                           |
| [Snippets]   |                              |                           |
| [API Help]   |  ...                         |                           |
| [LCD Prev.]  |                              |                           |
|              |                              |                           |
| (left tabs)  | (editor + find bar OR full   |                           |
|              |  simulator OR timeline beta) |                           |
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
  - **MIN_MAX** — bar graph filled to `(value - min) / (max - min)`,
    with the value and unit on the right.
  - **MULTI_CHOICE** — `< description >` of the currently-selected
    choice.
  - **AUDIO_VIEW_INTENSITY_*** — pseudo-waveform fill.
- Footer: the soft-button label in `[brackets]`, if any.
- `+N more` if the menu has more items than fit on screen.

The preview re-paints on every form change, so you can tune the layout
without leaving the form.

#### Live mirror of the simulator

When you move a slider in the **Simulator** tab's *Menu controls (live)*
section, the matching MIN_MAX bar in the LCD Preview updates **in real
time**. Same for MULTI_CHOICE combos. This lets you see exactly what
the user will see on the device LCD as the script runs.

The mechanism is simple: each `MenuItem` has an internal *override map*
keyed by `menu_id`. Moving the simulator slider sets the override; the
LCD preview's `paintEvent` reads the override if present, otherwise
falls back to the form's `default` value. Loading a different script
clears all overrides.

#### User-input markers on parameter change

Whenever you move a slider, pick a multi-choice option, push **Soft
Btn**, or fire **Trigger 1A**, a labelled vertical dashed line is
stamped on the timeline at the current simulated time:

- **Cyan** — menu input. Label is `Title=value` (e.g. `Frequency=200`)
  or `Title=Choice text` for a multi-choice combo.
- **Magenta** — Soft Btn press / release (`Soft↓` / `Soft↑`).
- **Orange** — external trigger (`Trig 1A`).

These markers let you read the timeline as a story: *"I bumped
intensity here → the green segments thickened immediately, the orange
pulses got denser"*. The marker is appended **before** the script
reacts so the dashed line always sits just to the left of the events
its callback produced.

Earlier versions wiped the event history on every slider change. That
behavior is gone — markers preserve full history, which makes the
before/after comparison much easier to see.

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
| **⟲ Reset** | Stops the timer, resets simulated time to 0, clears events, **reloads the cached source into a fresh `lua_State`**, re-arms `Setup()` for the next Run. After Reset every global goes back to its initial value (no stale schedulers from the previous Run). |
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

### Menu controls (live)

Below the channel state row, a section *Menu controls (live)* appears
whenever the form has at least one MIN_MAX or MULTI_CHOICE item:

- **MIN_MAX** items get a horizontal slider (range = `[min, max]`,
  step = `increment_step`, starting at `default`) plus a numeric readout.
- **MULTI_CHOICE** items get a combobox listing every choice.

Moving a slider or picking a choice fires the matching
`MinMaxChange(menu_id, val)` / `MultiChoiceChange(menu_id, choice_id)`
callback **immediately**, before the next Loop tick. The simulator
captures any `print()` output and channel state change. This lets you
test how your script reacts to the user turning the device knob without
flashing.

If the script doesn't react when you move the slider, you've probably:
- forgotten to tick the corresponding callback in **Functions**, or
- forgotten the `if (menu_id == N) then ... end` branch for that ID
  inside the callback's body.

### Timeline

- 4 horizontal lanes (CH1 to CH4).
- **Green segments** = sustained ON ranges (between `ChannelOn` and
  `ChannelOff`). Adjacent same-color segments merge into a single
  visual blob — no per-segment outlines, so you don't mistake pulse
  edges for cursors.
- **Orange segments** = `ChannelPulseMs` events with their declared
  duration.
- **Bar height = power**: each segment's vertical thickness is
  proportional to the channel's `power` at that moment (range 2 px at
  `power=0` … 14 px at `power=1000`). A low-intensity sustained ON
  looks like a thin stripe centered on the lane mid-line; full power
  is a fat bar. SetPower events split segments at runtime so
  changes mid-pattern are visible immediately.
- **Yellow now-cursor**: a high-contrast vertical marker pinned to the
  rightmost (current) timestamp. Three layers — a 12 px translucent
  yellow halo, a 3 px solid yellow line, a 1 px white core — plus a
  triangle at the top and bottom. Designed to remain visible even on
  fully-saturated channels.
- **Dashed user-input markers**: cyan (menu slider / combo), magenta
  (Soft Btn), orange (Trigger 1A). Each carries a small label in the
  band above the lanes — see §5 *User-input markers*.
- **Hover tooltip**: pass the mouse over a green/orange bar and you get
  the exact `CH<n>`, segment kind, start/end times in seconds, duration
  in ms and power level. Hovering an empty stretch of a lane shows the
  channel name + the time at the cursor; hovering a marker shows its
  category, label and timestamp.
- X-axis: seconds with one decimal.

The timeline is a 5 s sliding window — once simulated time exceeds 5 s
the window starts to scroll, and the cursor stays at the right edge.

### Variables panel

Right of the log, a 2-column table titled *Variables* shows every
underscore-prefixed Lua global (the convention used by all official
scripts to store user state). The table refreshes every Loop tick and
also right after each MinMaxChange / MultiChoiceChange / SoftButton /
ExternalTrigger.

What you'll see:

| Variable | Value |
|---|---|
| `_intensity` | `37` |
| `_speed` | `5` |
| `_burst_next_burst_ms` | `2400` |

Numeric values are right-aligned. Strings are quoted. `nil`, `true`,
`false`, tables (`table (3 entries)`) and functions are also
recognised. The view is read-only — to change a variable, move the
matching slider in *Menu controls* or write code in the editor and
press Reset to reload.

This is the easiest way to *see* how the script's internal state
evolves without sprinkling `print()` calls. Particularly useful for
beginners learning what `time_ms - _last_event` means.

### Log

Below the timeline (left half of the bottom panel):
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

### Rules — safety guardrails

These rules don't catch Lua bugs — they catch *comfort* and *electrical
safety* problems. They are deliberately conservative and silenceable by
tightening the offending value.

| Condition | Severity | Message |
|----|----|----|
| `zc.SetFrequency(*, hz)` with `hz > 250` | Warning | "frequencies above 250 Hz can feel harsh" |
| `zc.SetPulseWidth(*, p, n)` with `max(p, n) > 200` | Warning | "pulse widths above 200 µs deliver a lot of charge" |
| `zc.SetPower(*, X)` with `X ≥ 800` and **no** MIN_MAX menu item exists | Warning | "hard-coded high power and no slider to dial it down" |
| Script defines no `SoftButton`, no `ExternalTrigger`, and no MULTI_CHOICE menu | Warning | "no software kill-switch in this pattern" |
| `zc.EnableTriphase(true)` present and `allow_triphase` ticked | Info | "channel isolation OFF — pay attention to electrode placement" |
| `function Loop(` exists but **no** `zc.*` call anywhere | Info | "the pattern won't drive any channel — empty body?" |
| `zc.SetPower(*, X≥800)` inside `Loop()` with no `if` around it | Warning | "unconditional SetPower runs every tick — wrap it in `if` or move to Setup()" |

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

### Where new stubs are inserted

Smart-merge inserts new function stubs at their **canonical position**,
not at the end of the file. The canonical order matches the firmware's
expected layout:

> `Setup`, `Loop`, `MinMaxChange`, `MultiChoiceChange`, `SoftButton`,
> `ExternalTrigger`, `BluetoothRemoteKeypress`, `BluetoothHidEvent`,
> `AudioIntensityChange`

For each callback you've just ticked, smart-merge looks for the
**earliest existing canonical successor** in your source and inserts
the new stub just before it. If no later canonical callback is present,
the stub is appended at the end.

Example — your source has `Setup`, `Loop`, `SoftButton`. You tick
`MinMaxChange` and `MultiChoiceChange`:

- `MinMaxChange`: first existing successor = `SoftButton`. Inserted
  just before it.
- `MultiChoiceChange`: first existing successor = `SoftButton`.
  Inserted just before it (and just after the new `MinMaxChange`,
  because we process insertions in canonical-descending order).

Result:

```
Config = { … }

function Setup() … end
function Loop(time_ms) … end
function MinMaxChange(menu_id, val) … end       -- inserted
function MultiChoiceChange(menu_id, choice_id) … end  -- inserted
function SoftButton(pushed) … end
```

User-written helpers (e.g. `function MyHelper()`) are never moved.
Stubs land in their canonical slot relative to the **firmware
callbacks**; anything else stays exactly where you left it.

### What can be removed (with confirmation)

If you **uncheck** a callback in the Functions panel and that callback
is still defined in the editor, `Ctrl+G` opens a confirmation dialog
listing the orphaned function(s):

> `Setup()` is present in the editor but you just unchecked it in the
> Functions panel. Its body will be permanently deleted if you say Yes.
> Choose No to keep it in place.
> &nbsp;&nbsp;&nbsp;&nbsp;[Yes] [No] [Cancel]

- **Yes** — the function (signature + body + trailing blank line) is
  removed from the source.
- **No** — the function stays as written; the form simply stops listing
  it as ticked. You can re-tick it later without losing the body.
- **Cancel** — the whole regenerate is aborted; the editor is unchanged.

This preserves the smart-merge guarantee that **no code disappears
without explicit user consent**.

The function-block locator counts `function|if|for|while|repeat`
openers and `end|until` closers — sufficient for every real-world
script we've tested. Bare `do…end` blocks could theoretically confuse
it, but they're rare and the diff dialog will show any discrepancy.

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

### Diff dialog

After every `Ctrl+G` that changes anything, a dialog opens showing a
side-by-side diff with **green** for added lines and **red** for removed
ones. This is the easiest way to confirm "smart merge actually preserved
my function bodies" the first time you try it.

A *Don't show this dialog again* checkbox silences it; toggle it back on
via `View → Show diff after regenerate`.

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
- **Reset reloads the script** into a fresh `lua_State` from a cached
  copy of the source. Without this, a 2nd Run after Reset would inherit
  every global from the end of Run 1 (e.g. `_burst_next_burst_ms` in
  TENS pointing 5 s into the future, making the pattern silent until
  simulated time catches up).

### Numeric argument tolerance

Lua 5.4's `luaL_checkinteger` rejects floating-point numbers even when
they have an exact integer representation (e.g. `22.0`). Many official
scripts produce floats from `math.*` calls and pass them straight to
`zc.SetFrequency` etc. — which would crash with
`bad argument #2 to 'SetFrequency' (number has no integer representation)`.

To match what the real device firmware does, the simulator uses a
`checkIntLike()` helper that calls `luaL_checknumber` and truncates
toward zero. Floats are accepted; their fractional parts are discarded.

### Setup() argument

`LuaRuntime::callSetup` pushes `m_currentTimeMs` (= 0 at startup) as a
single argument before calling `Setup`. Lua silently ignores extra
arguments for scripts that declare `function Setup()` without any
parameter, so this is invisible to most scripts. But `trifade.lua`
declares `function Setup(time_ms)` and assigns
`_step_start_time_ms = time_ms` — without the argument, that global
becomes `nil` and the next Loop tick crashes on arithmetic.

### Resolving identifier-based menu IDs

Most official scripts express menu IDs as Lua identifiers:

```lua
MenuId = { MODE = 1, FREQ = 2, PULSE_WIDTH = 3, ... }
Config = {
    menu_items = {
        { id = MenuId.FREQ, ... },
        ...
    }
}
```

The regex parser (`LuaParser`) is purely textual and reads `MenuId.FREQ`
as the literal text — `toInt()` of that returns 0. Result: every menu
item ends up with `id = 0`, they collide in lookups, and the simulator
calls `MinMaxChange(0, val)` regardless of which slider you moved.

To fix this, after a script is successfully loaded into the simulator,
`LuaRuntime::extractScriptConfig()` walks the resolved `Config` table
*from Lua* — yielding real integer IDs. `MainWindow::
fixupFormFromSimulator()` then merges these IDs back into the form's
menu items. The form's display IDs are now authoritative, the
simulator's `MinMaxChange` calls hit the right branches, and the LCD
preview's live overrides match the right rows.

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

## 13. Beginner mode and the new-pattern wizard

### Beginner mode

`View → Beginner mode` toggles a simplified UI:

- **Config** tab: hides `Loop frequency (Hz)`, `Allow triphase`,
  `Bluetooth remote passthrough`.
- **Functions** tab: hides `ExternalTrigger`, `BluetoothRemoteKeypress`,
  `BluetoothHidEvent`, `AudioIntensityChange`.
- **Presets** menu: hides the four "quick presets" (Toggle, Fire, Waves,
  Audio — they're skeletons, less polished than the official scripts).
- A blue `● beginner mode` badge appears in the status bar.

When you enable beginner mode, two safety flags get force-cleared:
`allow_triphase = false` and `bluetooth_remote_passthrough = false`.
This is intentional — they're hidden, so they shouldn't silently persist
in the generated script.

The setting is remembered in `QSettings` between sessions.

### New from wizard…

`File → New from wizard…` (`Ctrl+Shift+N`) opens a 6-step dialog with a
**two-column layout**: controls on the left, contextual help on the
right. The right column updates live as you change selections.

#### Pages

1. **Pattern type** — Pulse (recommended), Constant, Fade in/out, Burst,
   TENS-like.
2. **Intensity ceiling** — Gentle (max power 400), Medium (700),
   Strong (1000).
3. **Cycle duration** — 1-60 seconds.
4. **Channels** — pick which of CH1..CH4 the pattern drives.
5. **Kill-switch** — checkbox to add a `STOP` soft button (recommended).
6. **Review** — read-only summary before generating.

#### The contextual side panel

Each page's right column has up to four sections that update as you
move sliders or toggle radios:

- 🎯 **What it does** — the technical effect of the selected option,
  in one or two sentences.
- 🎨 **Feeling** — the typical *sensation* in plain language. "Fine
  tingling" for TENS-like, "rapid-fire double-tap-tap-tap" for Burst,
  "very rapid" for 1 s cycles, etc.
- 💡 **Recommendation** — for who / when this option is appropriate.
  Always points beginners toward the safest choice.
- ⚠️ **Warning** — appears only when the current selection has a
  meaningful safety implication: Strong intensity + Burst, 4 channels
  at once, kill-switch disabled, etc.

The recommendations are conservative by default. The wizard pushes
beginners toward Pulse / Gentle / 5 s / 1 channel / kill-switch ON
because that's the safest configuration to learn the device.

#### What gets generated

Pressing **Generate** produces:

- A populated form (Config name, audio mode, menu items, callbacks).
- A complete Lua script in the editor with **French line-by-line
  comments** explaining every choice.
- The script is silently loaded into the simulator and the LCD preview
  is updated.

The generated script is intentionally **safe by default**:
- The `_intensity` global starts at 0 — the user has to move the slider
  to get any output.
- Power is gated by an `Intensity` MIN_MAX item bounded at the chosen
  ceiling.
- If kill-switch was ticked, `SoftButton(true)` instantly turns every
  channel off and resets `_intensity` to 0.

Read the comments before running on real hardware. The generator is a
starting point, not a finished pattern.

---

## 14. Safety profile (locked caps)

A process-wide profile (configured via **View → Safety profile…**) sets
hard caps on every numeric `zc.*` argument. The simulator clamps,
the linter complains, and — if the profile is locked — the user can't
change anything without the PIN.

### What gets capped

| Cap | Default (max) | Affects |
|-----|---------------|---------|
| Power            | 1000 | `zc.SetPower(*, value)` |
| Frequency        | 300 Hz | `zc.SetFrequency(*, hz)` |
| Pulse width      | 255 µs | `zc.SetPulseWidth(*, pos, neg)` (each phase) |
| Pulse duration   | 10000 ms | `zc.ChannelPulseMs(*, ms)` |

When the script tries to set a value above the cap, the simulator:
1. Silently clamps the argument before applying it.
2. Logs a `[SAFETY] SetPower(1, 1000) clamped to 500` line in the log.

The linter scans the editor source for numeric literals exceeding the
caps and:
- emits a **Warning** if the profile is unlocked,
- emits an **Error** if the profile is locked (the pre-flight check
  refuses scripts that wouldn't run cleanly under the active caps).

### Locking with a PIN

Locking serves one purpose: stop someone borrowing your device from
casually raising the caps. The PIN is hashed with SHA-256 + a static
salt and stored in QSettings. The binary is publicly readable so this
is **not** a security measure against a determined attacker — only
against accidental tampering.

To lock:
1. Set caps to your preferred values.
2. Tick **Lock with a PIN**.
3. Enter the same PIN twice (4-16 characters).
4. **OK**.

While locked:
- The View menu's Safety profile dialog requires the PIN before any
  field can be edited.
- The View → Beginner mode toggle is **forced ON** and read-only, so
  advanced controls (triphase, BT HID, audio, …) stay hidden.
- The status bar shows `🔒 Safety locked (max pwr=N · M Hz · K µs)`.
- Linter warnings about exceeding caps become Errors → pre-flight
  refuses the script.

To unlock:
1. **View → Safety profile…**
2. Click **Unlock to edit…** → enter PIN.
3. Untick **Lock with a PIN** → **OK**.

Or just leave it locked and lower the offending values in the script.

### Suggested starting profiles

| Profile | Power | Freq | Width | Duration | Use case |
|---|---|---|---|---|---|
| First session | 400 | 200 | 180 | 500 | New user, learning their tolerance |
| Casual | 700 | 250 | 200 | 1000 | Familiar user, daily patterns |
| Unrestricted | 1000 | 300 | 255 | 10000 | Experienced, no caps |

The status bar badge shows up whenever **any** cap is below the
hardware maximum **or** the profile is locked, so you always know
when caps are in play.

### Limitations

- Caps apply to **the simulator only**. Real-device behavior is
  governed by the script you flash. Always lower values inside the
  script before flashing — the safety profile won't follow it.
- Caps don't apply to literal values in `Config = {…}` (e.g. a MIN_MAX
  item with `max = 1000`). The user can still pick high values via
  the slider; the simulator clamps the resulting `SetPower(*, val)`
  call.
- The PIN protects against tampering with the safety profile UI — it
  doesn't prevent the user from editing the source script directly.

---

## 15. Pre-flight check

`Generate → Pre-flight check` (`Ctrl+Shift+P`) is the recommended
"is it ready?" step before you save / flash a script.

It runs:

1. The full linter (Config + source + safety rules).
2. A throwaway `LuaRuntime`:
   - Loads the script (`luaL_loadbuffer` + `pcall`).
   - Calls `Setup()`.
   - Runs 50 ticks of `Loop(time_ms)` at 20ms each (1.0 s simulated time).

Then opens a dialog with:

- A **verdict** banner — `✓ Looks good` (green), `⚠ Mostly OK` (orange),
  or `❌ Not ready` (red).
- A **confidence score** 0-100% (-30 per error, -8 per warning, -50/-20/-30
  for failed load / Setup / Loop).
- A breakdown table: lint errors, warnings, info, loads-in-Lua, Setup
  ran, Loop ran 50 times.
- The simulator stack trace if anything crashed.
- A reminder that the simulator only validates **logic**, not electrical
  safety.

A green verdict doesn't mean "safe to use at full intensity" — it means
"the script doesn't have obvious bugs and runs in the sandbox". Ramp
up gradually on real hardware regardless.

---

## 16. Explain this script

`Help → Explain this script…` (`Ctrl+Shift+E`) opens a dialog with the
current editor source rendered as a 3-column table:

- Line number.
- The original code (monospace, dark background).
- A plain-English explanation in light blue.

The explanations are produced by a naive pattern-matcher in
`src/codegen/Explainer.cpp`. It recognises:

- Top-level callback definitions (`function Setup()`, `function Loop()`, …).
- `zc.*` calls — every function in the API gets a sentence with its
  arguments interpolated.
- `for ch = 1, 4`, `while X do`, `if menu_id == N then`,
  `elseif menu_id == N`, `if pushed`, `if active`, `if time_ms > X`.
- `local x = …` and bare `_global = …` assignments.
- `require("foo")`, `module("foo", …)`.
- `print(…)`.
- Standalone `then` / `end` / `else` / `do` / `return` / `break`.

Lines that don't match any pattern get an empty note rather than a
fabricated guess.

This feature is "best-effort" by design — it's an aid, not an
authoritative translation. Use it to get started reading an unfamiliar
script (e.g. one of the official presets).

---

## 17. Timeline editor

The **Timeline (Beta)** tab on the right pane (next to **Editor** and
**Simulator**) lets you build a pattern visually by placing events on a
4-lane timeline, and round-trip with the Lua source via an embedded
JSON sentinel.

A yellow banner at the top of the tab reminds you that this feature is
experimental — the round-trip is stable, but the API may still evolve.

### Project toolbar (top row)

| Control | Effect |
|---|---|
| **Loop** checkbox | When ON, generated `Loop()` wraps `time_ms` modulo the cycle. The pattern restarts automatically every cycle. |
| **Cycle (ms)** | Length of one cycle. Used for the loop wrap and as the default viewport width. |
| **📸 Capture from Sim** | Run the editor's current script in a sandboxed runtime for the configured cycle length, capture every `ChannelOn` / `Off` / `PulseMs` it emits, and overlay them on the lanes in dim green/orange (read-only). Use this to *visualise* dynamically-scheduled scripts (e.g. `tens.lua`, `climb.lua`) that have no JSON sentinel to read. |
| **← Read from Lua** | Re-read the timeline JSON sentinel from the current editor. Useful if you've manually edited the Lua and the timeline is stale. |
| **→ Push to Lua** | Generate `Setup()` + `Loop()` from the current timeline and write them into the editor (replacing any previous timeline block). The sentinel comment is also updated. |
| **Clear all** | Wipe every event (undoable with **Ctrl+Z**). |

### Edit toolbar (second row)

A tool palette + view controls. The currently active tool determines
what happens when you click on an empty lane.

| Control | Effect |
|---|---|
| **↖ Select** (`S`) | Click an event to select / drag-move. Click on empty lane = deselect. |
| **▱ Pulse** (`P`) | Click-drag on empty lane = create a Pulse with that duration. |
| **▶ On** (`O`) | Click on empty lane = drop a `ChannelOn` at that instant. |
| **⏹ Off** (`F`) | Click on empty lane = drop a `ChannelOff` at that instant. |
| **Snap** dropdown | Quantise create / move / nudge to `Off / 25 / 50 / 100 / 250 / 500 / 1000 ms`. Default **100 ms**. Hold **Shift** during a drag to bypass. |
| **t = … ms** | Live time under the cursor (updated on hover and during drag). |
| **↶ Undo** (`Ctrl+Z`) / **↷ Redo** (`Ctrl+Y`) | Step through up to 100 prior project snapshots. Loop / cycle / property edits are also recorded. |
| **− % +** / **Fit** | Zoom out, current zoom indicator, zoom in, fit to one full cycle. The mouse wheel still works as a power-user shortcut. |

### Canvas interactions

| Action | Effect |
|---|---|
| **Click on event** | Select it (yellow outline) — tool independent. |
| **Drag selected event body** | Move it (across channels too — drag up/down). |
| **Drag right edge** of a Pulse | Resize duration. |
| **Right-click event** | Context menu: convert to Pulse / ChannelOn / ChannelOff, **Duplicate**, or Delete. |
| **Click on empty lane** | Depends on the active tool — see the tool table above. |
| **Wheel** | Zoom in/out around the cursor (or use the toolbar buttons). |
| **Ctrl+Wheel** | Pan horizontally. |
| **← →** | Nudge the selected event by one snap step. |
| **Ctrl+← →** | Nudge by 10 snap steps. |
| **Ctrl+D** | Duplicate the selected event (offset by one snap step). |
| **Delete** / **Backspace** | Delete the selected event. |
| **Esc** | Deselect the current event. |
| **S / P / O / F** | Switch tool (Select / Pulse / On / Off). |
| **Shift** held during drag | Bypass snap for that gesture only. |

Bars are coloured by type:

- **Orange** — `Pulse` (sealed segment, fires `zc.ChannelPulseMs`).
- **Green** — `ChannelOn` (fires `zc.ChannelOn`, channel stays driven).
- **Red** — `ChannelOff` (fires `zc.ChannelOff`).

A dashed red vertical line marks the end of cycle when **Loop** is ON.
A faint vertical grid is drawn at every snap step when zoomed in
enough (≥ 6 px between steps), so you can eyeball alignment.

The header line above the lanes also reports the active tool and snap
setting so the canvas is self-describing.

### Property panel

The right side of the timeline tab shows the selected event's editable
properties:

- **Type** — Pulse / ChannelOn / ChannelOff.
- **Channel** — 1-4.
- **Start (ms)** — when the event fires within the cycle.
- **Duration** (Pulse only) — pulse length.
- **Power**, **Freq**, **Width** — each can be a **literal** integer
  (e.g. `power = 800`) or a **variable** reference (e.g. `power = _intensity`).
  The Variable dropdown auto-suggests names from the form's MIN_MAX
  items so dynamic patterns are easy to wire.
- **Note** — optional comment that ends up as a `-- comment` in the
  generated Lua.
- **Delete** button — same as the Delete key.

### Capture overlay (read-only ghost trace)

The Timeline editor was originally limited to scripts it had generated
itself — the **← Read from Lua** path looks for a `ZC95_TIMELINE_V1`
sentinel and gives up if it's missing. That made the tab visually empty
when you loaded any of the bundled official scripts (`tens.lua`,
`climb.lua`, `combo.lua`, …) because they compute their schedules
dynamically and have no sentinel.

**📸 Capture from Sim** fills that gap. It:

1. Reads the editor's current source.
2. Spins up a fresh sandbox `LuaRuntime` (separate from the Simulator
   tab — your live simulator session is not disturbed).
3. Runs `Setup()` then `Loop()` in **4 ms steps** for at least **60 s**
   of simulated time (or longer if `Cycle (ms)` is bigger). The fine
   4 ms step is intentional: it matches the ~244 Hz tick rate that the
   bundled `ettot.at244hz`-family scripts (torment, climb, combo,
   phasing, rhythm, orgasm, random2, stroke, intense) count internally.
   At the live simulator's default 20 ms/tick those scripts undersample
   their own block timer by ~5× and never enter their main pattern.
4. Drains all `ChannelOn` / `ChannelOff` / `ChannelPulseMs` events.
5. Overlays them on the 4 lanes as a **dimmed gray-green/orange ghost**
   trace, drawn *behind* any editable events.

Notes:

- **Snapshot, not live.** If you move a slider in the Simulator tab or
  edit the Lua, click **📸 Capture from Sim** again to refresh.
- The overlay is read-only — you can't drag or right-click it. It's
  there to *see* what the script does.
- A small grey caption ("Sim snapshot · *t* s · gray = script behaviour
  (read-only)") appears at the bottom of the canvas while a capture is
  loaded, so it's never confused with your editable events.
- The capture is auto-cleared when you load a different file (Open,
  preset, official script, New, Wizard) so a stale ghost from one
  script never sticks to another.
- If the script's `Loop()` raises an error mid-run, the status bar
  reports the failure time and you still see the events captured up
  to that point.
- **Cycle auto-bumped.** Slow ettot scripts often emit their first
  channel activity around t=20-50 s. If your editor's `Cycle (ms)` was
  10 000 (the default) when you clicked Capture and the captured trace
  extends past it, the spinbox is automatically rounded up to the next
  5 s multiple so the trace fits in the viewport. The bump is
  undoable (Ctrl+Z) like any cycle change.
- **Status bar messages** tell you what happened: number of channel
  events captured, whether the cycle was bumped, or a hint when the
  script seems "stuck off" (e.g. all activity in the first 100 ms).

### Round-trip via JSON sentinel

When you push to Lua, the timeline's project state is serialised to
JSON and embedded in the script as a sentinel block:

```lua
--[[ ZC95_TIMELINE_V1
{"v":1,"loop":false,"cycleMs":10000,"events":[{...}, ...]}
]]
```

This block lives near the top of the script (just before the first
`function`). When you click **← Read from Lua**, the editor scans for
this sentinel and rebuilds the project from it — so you can quit the
app, reopen the file later, and resume editing the timeline visually.

If you delete the sentinel manually (or the script has none), Read
from Lua reports "no timeline sentinel found" and leaves the canvas
unchanged.

### Generated Lua structure

For each event, a one-shot `if` branch is emitted in `Loop()`:

```lua
if t >= 500 and not _tl_fired_3 then
    _tl_fired_3 = true
    zc.SetPower(1, _intensity)       -- variable reference
    zc.SetFrequency(1, 150)
    zc.SetPulseWidth(1, 150, 150)
    zc.ChannelPulseMs(1, 100)
end
```

The `_tl_fired_<N>` flags make each event fire **once per cycle**. In
loop mode, they're reset whenever `time_ms` wraps modulo `cycleMs`.

### Limitations

- No nested logic (no `if/else` based on script state — only time-based
  triggers).
- No expressions in params (only literal or single variable name).
- Timeline events fire in chronological order; if two events overlap
  on the same channel they'll both fire, the second potentially
  cutting short the first's `ChannelPulseMs` auto-off.
- Manual edits to the generated `Setup()` / `Loop()` will be
  overwritten on the next **→ Push to Lua**. Keep helper functions and
  hand-rolled callbacks elsewhere in the script.
- The **📸 Capture from Sim** overlay is a one-shot snapshot. Slider
  changes, code edits, or different starting parameters won't update
  the trace — re-click Capture to refresh.

---

## 18. ▶ Test buttons on menu items

In the **Menu Items** tab, the **▶ Test** button (next to Add / Edit /
Duplicate / Remove) drives the **selected** item's value through a sweep
and reports whether the script reacts:

1. Switches the right pane to the **Simulator** tab.
2. Loads the editor source if not already loaded; calls `Setup()` if
   needed.
3. Snapshots the 4 channels' state (on/off, power, frequency, pulse
   width).
4. For a MIN_MAX item: drives the value through `min → default → max`,
   running 5 Loop ticks between each change.
5. For a MULTI_CHOICE item: iterates through every `choice_id`, 5 ticks
   each.
6. Snapshots the channels again, compares to the before-snapshot.
7. Logs `[TEST] Pattern reacted to the value changes — looks wired.`
   if anything changed, or
   `[TEST] Pattern did NOT react. Check that MinMaxChange is defined
   AND that it handles this menu_id.` otherwise.

Use this when you've added a new menu item and want quick confirmation
the `if (menu_id == N) then ... end` branch is correctly wired.

---

## 19. Auto-save and draft recovery

Every 30 seconds, if the document has unsaved changes, the editor's
content is dumped to:

```
<temp>/zc95-lua-builder/autosave-<pid>.lua
<temp>/zc95-lua-builder/autosave-<pid>.meta   (original path + ISO timestamp)
```

(`<temp>` is whatever `QStandardPaths::TempLocation` resolves to:
`%LOCALAPPDATA%\Temp` on Windows, `/tmp` on Linux, …)

When the application starts, it scans this directory for autosave files
that **don't** belong to the current process ID — those are leftovers
from a previous (possibly crashed) run. For each one, a dialog opens:

> An unsaved draft was found from a previous session.
> Original file: …
> Last saved: 2026-04-30T17:23:11
> Recover the draft into the editor? [Yes] [No] [Discard]

- **Yes** — load the draft into the editor, parse the form, push to the
  simulator, mark dirty (so it'll be auto-saved again until you save
  manually).
- **No** — leave the file in place; you'll be asked again next launch.
- **Discard** — delete the file and don't ask again.

On a graceful close (`closeEvent`), the current process's autosave files
are deleted automatically — only crashes leave drafts behind.

Multiple instances of the application can run simultaneously without
interfering, because each writes to its own PID-suffixed file.

---

## 20. Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New script |
| `Ctrl+Shift+N` | New from wizard… (6-step beginner-friendly generator) |
| `Ctrl+O` | Open `.lua` |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As… |
| `Ctrl+Q` | Quit |
| `Ctrl+G` | Regenerate code from form (smart merge) |
| `Ctrl+Shift+Alt+G` | Regenerate from scratch (overwrite, with confirm) |
| `Ctrl+Shift+G` | Re-parse form from editor |
| `Ctrl+L` | Run linter |
| `Ctrl+Shift+P` | Pre-flight check (lint + sim dry-run, scored verdict) |
| `Ctrl+Shift+E` | Explain this script (plain-English line-by-line) |
| `Ctrl+R` | Reload editor into simulator |
| `Ctrl+F` | Find… |
| `Ctrl+H` | Replace… |
| `F3` | Find next |
| `Shift+F3` | Find previous |
| `Ctrl+Space` | Trigger autocomplete |
| `Esc` | Close find/replace bar / dismiss completer popup |
| `Tab` / `Enter` | Accept completion (when popup is open) |

#### Timeline editor (when the canvas has focus)

| Shortcut | Action |
|----------|--------|
| `S` / `P` / `O` / `F` | Switch tool — Select / Pulse / On / Off |
| `← →` | Nudge selected event by one snap step |
| `Ctrl+← →` | Nudge by 10 snap steps |
| `Ctrl+D` | Duplicate selected event |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo (up to 100 snapshots) |
| `Delete` / `Backspace` | Delete selected event |
| `Esc` | Deselect current event |
| `Shift` (held during drag) | Bypass snap for that gesture only |
| Wheel / `Ctrl+`Wheel | Zoom around cursor / pan horizontally |

---

## 21. Settings, recent files, and persistence

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
- `beginnerMode` — boolean, persists the View → Beginner mode toggle.
- `showRegenDiff` — boolean, persists the "show diff after regenerate"
  preference.

To reset to defaults: close the app, delete the registry key (Windows)
or the config file (Linux/macOS), reopen.

---

## 22. Portable Windows build

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

## 23. Troubleshooting / FAQ

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

**The simulator runs but `torment.lua` (or `climb.lua`, `combo.lua`,
`phasing2.lua`, `rhythm.lua`, `orgasm.lua`, `random2.lua`, `stroke.lua`,
`intense.lua`) doesn't seem to do anything — channels turn ON briefly
at t=0 then go OFF and stay off.** This is an undersampling issue: those
scripts use the `ettot` library which counts its own ticks at 244 Hz
(`ettot.at244hz`). At the simulator's default **20 ms/tick** the script's
internal block timer increments only ~50 times per simulated second
instead of ~244 — too slow to ever cross its threshold and trigger the
real pattern. **Fix:** lower the **N ms/tick** spinbox (top right of the
Simulator) to **4 or 5** before clicking Run. Channels will then start
firing as designed within ~5-25 simulated seconds. The Timeline (Beta)'s
**📸 Capture from Sim** button already forces dt=4 ms internally for the
same reason.

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

**`bad argument #2 to 'SetFrequency' (number has no integer
representation)`** — fixed. The simulator now truncates float arguments
the same way the device firmware does. Make sure you've rebuilt after
pulling recent commits.

**After a Reset, the second Run looks frozen / channels stuck ON.** —
fixed. Reset now reloads the cached source into a fresh `lua_State`,
so every script global goes back to its initial value. If you still
see this, you're on an older build — pull and rebuild.

**The same value appears for every slider in the LCD preview.** —
fixed. Caused by the regex parser failing to read identifier-based
menu IDs (`id = MenuId.FREQ`), so they all shared `id = 0`. Now the
simulator extracts the real integer IDs from Lua and patches them
back into the form. Any script loaded from disk or from the bundled
official presets benefits automatically.

---

## 24. Known limitations

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
- **Loop tick rate is fixed at 1/`ms-per-tick`**, not the device's
  actual ~600 Hz. Scripts that count their own ticks internally
  (everything using `ettot.at244hz` — torment, climb, combo, phasing2,
  rhythm, orgasm, random2, stroke, intense) need **ms/tick ≤ 5** to
  behave at intended speed. Default is 20, which works for tens, waves,
  trifade and any custom script that doesn't rely on a high-rate
  internal counter. See the FAQ entry above for symptoms and fix.

---

## 25. Glossary

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
- **Pre-flight check** — the combined "lint + 1-second simulator
  dry-run" that scores a script 0-100% before you flash it.
- **Beginner mode** — UI toggle that hides advanced options (triphase,
  Bluetooth HID, audio, …) and the "quick presets" submenu.
- **Wizard** — the 6-step "New from wizard…" dialog that generates a
  starting script from beginner-friendly multiple-choice answers.
- **Kill-switch** — any user-visible mechanism to stop the pattern
  immediately. The linter expects at least one of: a `SoftButton`
  callback, an `ExternalTrigger` callback, or a MULTI_CHOICE menu item.
  The front-panel power dial is always available as a hardware fallback.
- **Autosave / draft** — the 30-second editor backup written to
  `<temp>/zc95-lua-builder/autosave-<pid>.lua`. Recovered on next
  launch if the previous session crashed.
- **`extractScriptConfig`** — the LuaRuntime method that walks the
  resolved `Config` table after pcall, returning a `ScriptConfig` with
  real integer menu IDs even when the source uses identifiers like
  `MenuId.FREQ`. Used by MainWindow to patch the form after every load.
- **Live mirror** — the mechanism by which moving a slider in the
  Simulator's *Menu controls* section instantly updates the matching
  bar in the LCD Preview. Implemented via a per-menu_id override map
  in `LcdPreviewPanel`.
- **Reset reload** — the simulator's Reset button discards the
  `lua_State` and re-loads from a cached source so a 2nd Run starts
  from a genuinely fresh state.
- **Timeline editor** — the visual 4-lane pattern builder accessible
  via the **Timeline (Beta)** tab on the right pane. Round-trips with
  the editor source through a JSON sentinel comment block.
- **Sentinel block** — the `--[[ ZC95_TIMELINE_V1 … ]]` comment that
  carries the timeline editor's serialised project state inside the
  `.lua` script.
- **Capture overlay** — the read-only ghost trace that **📸 Capture from
  Sim** paints on the Timeline editor's lanes. Generated by running
  the script in a sandbox `LuaRuntime` for one cycle and recording
  every `ChannelOn` / `Off` / `PulseMs` it emits. Snapshot, not live —
  re-click to refresh.
- **TimelineParam** — a single event parameter (power / freq / width)
  that can either be a literal integer or a reference to a Lua
  variable name (e.g. `_intensity`). Lets timeline events react to
  the form's MIN_MAX sliders at runtime.
- **Safety profile** — a process-wide set of caps (max power, max
  frequency, max pulse width, max pulse duration) applied at the
  simulator level and surfaced in the linter. Configurable via
  `View → Safety profile…` and lockable with a PIN.
- **Locked profile** — a safety profile with `locked = true`.
  Forces beginner mode ON, makes the View → Beginner mode toggle
  read-only, and promotes safety-cap warnings to errors. Unlocking
  requires the PIN set when locking.

---

*If something is missing or wrong, open an issue. — End of HELP.*
