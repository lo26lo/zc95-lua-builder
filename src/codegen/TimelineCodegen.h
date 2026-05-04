#pragma once

#include "TimelineProject.h"
#include <QString>

// Round-trip glue between the visual TimelineEditorWidget and the
// Lua editor. The compiled Lua embeds the project's JSON inside a
// sentinel-marked block-comment, so the editor can re-read its own
// output and reconstruct the project.
//
// Sentinel format:
//   --[[ ZC95_TIMELINE_V1
//   {"v":1,"loop":false,"cycleMs":10000,"events":[...]}
//   ]]
//
// The generated Lua provides a Loop() that fires each event at the
// right time. Setup() resets the per-event "fired" flags. If
// `loop=true`, time_ms is wrapped modulo cycleMs so the pattern
// restarts automatically.
namespace TimelineCodegen {

constexpr const char* kSentinelOpen  = "--[[ ZC95_TIMELINE_V1";
constexpr const char* kSentinelClose = "]]";

// Generate a complete Lua snippet (Setup + Loop + sentinel comment)
// that replays the timeline. Does NOT generate Config — that's the
// job of LuaGenerator. The caller usually does:
//   1. LuaGenerator::generate(config) for skeleton
//   2. TimelineCodegen::generateBody(project) for the timeline body
//   3. Splice the body into the script (replacing Setup + Loop).
QString generateBody(const TimelineProject& project);

// Find a previously-embedded JSON sentinel block in `source`. Returns
// the parsed project, or an empty project if no sentinel was found.
// `found` is set to true iff a sentinel was located.
TimelineProject extractProject(const QString& source, bool* found = nullptr);

// Replace any existing sentinel block in `source` with a fresh one
// derived from `project`. If no sentinel exists, the new block is
// inserted just before the first `function` definition (or at the end
// if no function exists). Returns the modified source.
QString updateSentinel(const QString& source, const TimelineProject& project);

}  // namespace TimelineCodegen
