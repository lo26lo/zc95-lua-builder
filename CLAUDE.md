# Claude working notes for this repo

This file is read automatically by Claude Code when working in this
repository. It encodes the contributor agreements and house rules that
should survive across sessions.

---

## House rules

### 1. Documentation is part of every change

> **Whenever you change behavior the user can see, update `README.md`
> AND `HELP.md` in the same change.**

Concretely:

- New menu entry / new keyboard shortcut → updated in:
  - `README.md` "Keyboard shortcuts" table
  - `HELP.md` §18 "Keyboard shortcuts"
  - `HELP.md` relevant feature section
- New linter rule → row added to the rules tables in `HELP.md` §8.
- New simulator behavior (live sliders, reset semantics, cursor
  rendering, …) → mentioned in `HELP.md` §7 (toolbar / state /
  timeline) and/or §11 (architecture deep-dive).
- New tooltip / pedagogical text on the form → if it changes the
  *meaning* of a field (not just wording), reflect it in `HELP.md` §5.
- Bug fix that the user might hit again → add a FAQ entry in
  `HELP.md` §21 ("Troubleshooting / FAQ") so the next person sees the
  remedy.
- New limitation discovered → `HELP.md` §22 "Known limitations".
- New term that appears in the UI or docs → `HELP.md` §23 "Glossary".

If a change is purely internal (refactor, perf, build infrastructure)
and produces no observable effect, the docs don't need to move. Use
judgment, but err toward documenting.

### 2. The EXPERIMENTAL banner stays at the top

`README.md` and `HELP.md` both open with a `> [!CAUTION]` blockquote
warning that the project hasn't been validated on real hardware. Don't
remove or downgrade that banner unless the user says hardware
validation is done.

### 3. Run the tests before declaring "done"

After any non-trivial code change:

```bat
_build.bat                              :: build (calls vcvarsall first)
build\test_roundtrip.exe                :: 102 cases, ~70 ms
```

Target is 102/102 passing. New behavior should come with a test, ideally
in `tests/test_roundtrip.cpp` (extend the smoke-test data tables when
the change touches official scripts).

### 4. Don't bypass the autosave / dirty-state machinery

`MainWindow` carefully manages `m_suppressDirty` so reload paths don't
falsely mark a freshly-loaded script as dirty. When adding a new code
path that calls `m_editor->setPlainText()` or `applyConfigToForm()`,
either go through an existing helper or save/restore `m_suppressDirty`
yourself.

### 5. Commit messages

Follow the format already established in the repo:

```
<title — under 60 chars, imperative>

<short paragraph>

<bulleted list of grouped changes>
```

Don't squash unrelated changes into one commit. If a session touched
both code and docs, that can be one commit (because of rule #1, they
go together by design).

---

## Project shape (quick reference)

- `src/MainWindow.{h,cpp}` — GUI shell, every menu, autosave, beginner
  mode, fixupFormFromSimulator (fixes menu IDs after every load),
  runPreflight, explainScript, newFromWizard.
- `src/sim/LuaRuntime.{h,cpp}` — Lua 5.4 + `zc.*` stubs + Lua 5.1
  polyfill (`module`, `package.seeall`) + Qt-resource searcher for
  `require` + `extractScriptConfig` for ID resolution.
- `src/ui/SimulatorPanel.{h,cpp}` — toolbar, channel state, live menu
  controls, timeline, log, reset reload, `pushUserInput` to stamp
  slider/button markers on the timeline, `testMenuItemDrive`.
- `src/codegen/` — `LuaGenerator` (model → Lua + smart merge),
  `LuaParser` (regex, literal-only), `Linter` (incl. 7 safety rules),
  `ApiDoc`, `Explainer`, `LineDiff`.
- `src/ui/WizardDialog.{h,cpp}` — 6-step beginner wizard with
  contextual side-info panels.
- `src/ui/LcdPreviewPanel.{h,cpp}` — LCD mock-up with live override
  map mirroring simulator sliders.
- `src/ui/TimelineWidget.cpp` — 4-lane channel timeline + yellow
  high-visibility now-cursor.
- `tests/test_roundtrip.cpp` — 102 cases including 60 simulator
  smoke tests across the 12 official scripts.

---

## Status

| | State |
|---|---|
| Build | Green on MSVC 2022 + Qt 6.8.3 |
| Tests | 102/102, ~70 ms |
| Hardware | Never tested on real ZC95 (banner stays) |
| Portable | `_deploy.bat` produces `dist/zc95-lua-builder/` |

See `SESSION.md` for a richer session-handoff document.
