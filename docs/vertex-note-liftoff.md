# VertexNote Lift-Off Plan

This document tracks the rebrand from the inherited Xournal++ application surface to
VertexNote. The goal is a clean product identity without breaking existing `.xopp`
documents or upstream-derived internals that still need incremental refactors.

## Rebrand Guardrails

- User-facing product name: `VertexNote`.
- Repository, release tags, release notes, and CI naming should use `vertex-note`.
- Keep `.xopp`, `.xoj`, `.xojpp`, and `.xopt` compatibility names where they describe
  existing file formats or MIME compatibility.
- Keep legal attribution to VertexNote and Xournal in copyright/license files.
- Avoid large symbol renames while geometry work is active unless the rename is isolated
  and fully build-verified.

## Current High-Impact Surfaces

- Build/package identity:
  - `CMakeLists.txt` still declares `project(vertex-note)`.
  - `src/CMakeLists.txt` still builds `vertex-note` and `vertexnote-wrapper`.
  - Windows packaging still outputs `vertex-note-setup.exe` from `windows-setup/package.sh`.
- Desktop/app metadata:
  - `desktop/com.github.vertex-note.vertex-note.desktop.in`.
  - `resources-templates/com.github.vertex-note.vertex-note.appdata.xml.in`.
  - `ui/about.glade` and `ui/crashDialog.glade`.
- Runtime/user text:
  - About dialog title and product label.
  - Crash dialog title.
  - Toolbar default group name in `resources-templates/toolbar.ini.in`.
  - New-document toolbar label in `ToolMenuHandler.cpp`.
- Internal names:
  - `VertexNoteView`, `NotePage`, `VertexNoteScheduler`, `xoj::` namespaces, and many comments
    remain inherited architecture names. Rename these only in focused batches.

## Incremental Lift-Off Phases

1. Public UI text
   - Rename visible dialogs, menu strings, toolbar group labels, and docs from Xournal++
     to VertexNote where the text is not legal attribution or compatibility language.

2. App/package identity
   - Add `vertex-note` executable and wrapper target aliases.
   - Update Windows installer naming to `VertexNote` and `vertex-note-setup.exe`.
   - Update desktop/appstream IDs to a VertexNote app ID while keeping MIME associations
     for existing notebook formats.

3. Runtime paths and defaults
   - Move config/cache/data paths toward VertexNote-owned names with migration fallback
     from the inherited paths.
   - Keep old paths readable for users upgrading from early fork builds.

4. Internal architecture renames
   - Rename internal types only when touched by active work or when a module is isolated.
   - Prefer `VertexNoteView`/`NotebookPage` style aliases before hard renames if it reduces
     merge risk.

5. Compatibility cleanup
   - Keep a documented allowlist for remaining `xournal`, `xoj`, and `xopp` occurrences.
   - Remove accidental upstream changelog/history references from release notes.

## Updater/Changelog Direction

VertexNote should expose an in-app update/check-for-release surface similar to the
release flow used by `pylrcget`:

- Fetch latest GitHub release for `saitatter/vertex-note`.
- Show current version, latest version, release date, and concise release notes.
- Link to portable zip and installer assets when present.
- Never auto-install in the first implementation; start with a safe notification and
  explicit download/open-release actions.
- Cache the last check timestamp and allow users to disable automatic checks.
