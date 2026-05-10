# Copilot Instructions for VertexNote

## Communication

- Raspunde in romana, concis si practic.
- Explica tradeoff-urile importante inainte de schimbari riscante.
- Nu inventa comportamente; verifica in cod, teste sau documentatie locala.

## Project Context

- VertexNote is a long-term fork of Xournal++ for CAD-inspired technical note-taking.
- The stack is C++20, GTK3, Cairo, CMake, and the existing Xournal++ architecture.
- An alternative Qt6 Widgets shell (`ENABLE_QT_SHELL`) provides full document editing
  without GTK3. Qt shell code lives in `src/qt/`.
- Preserve existing Xournal++ behavior unless a change is explicitly part of the VertexNote roadmap.
- App code lives in `src/core`; tests live in `test/unit_tests`.
- VertexNote-specific geometry code lives under `src/core/vertexnote`.
- Platform-neutral render/input abstractions live in `src/core/view/render/` and `src/core/ui/`.

## Architecture Rules

- Keep handwriting, highlighter, eraser, existing shape recognition, text, image, PDF annotation, and legacy `.xopp` behavior stroke-based.
- Add precision geometry as object-based structures with stroke-compatible fallback rendering.
- Prefer incremental refactors over rewrites.
- Do not turn `Stroke` into a general CAD object.
- Keep geometry, snapping, constraints, and future 3D support in separable modules.
- Maintain compatibility with existing `.xopp` documents.

## Qt Shell Conventions

- Qt shell uses `QtDocumentController` for all document mutations, not the GTK `Control` class.
- Commands are registered via `QtCommandHost::registerCommand()` with `CommandDescriptor`.
- Shortcuts are plain strings (e.g. `\"Ctrl+S\"`, `\"Alt+Left\"`), not `QKeySequence`.
- Tool state lives in `QtToolState` struct on `QtCanvas`.
- All editing operations must support undo/redo via `QtHistoryEntry` variants.
- Prefer `this->window.commandHost()->` over local `host` variables in `registerBootstrapCommands()`.
- Build: `scripts/mingw64-dev.ps1 build`; run: `scripts/mingw64-dev.ps1 run`.

## Git and Releases

- Use Conventional Commits. Examples:
  - `feat: add geometry object model`
  - `fix: preserve fallback stroke bounds`
  - `test: cover vertex edge validation`
  - `chore: update semantic release metadata`
- `python-semantic-release` reads conventional commits from `pyproject.toml`.
- Do not manually edit generated release artifacts unless explicitly requested.
- Keep commits focused; do not mix unrelated UI, model, serialization, and packaging changes unless the feature requires it.

## C++ Style

- Target C++20.
- Prefer existing Xournal++ patterns and utilities over new framework-style abstractions.
- Keep ownership explicit with `std::unique_ptr`, references, and existing document/layer ownership conventions.
- Use stable IDs for VertexNote geometry objects, vertices, edges, and constraints.
- Avoid broad cross-subsystem changes until the model API is covered by tests.

## GTK/Cairo UI Rules

- Keep existing Xournal++ UI behavior intact while introducing VertexNote tools.
- Avoid blocking GTK event handling during pointer interaction.
- Use overlay views for active tool previews.
- Use Cairo rendering paths that can be cached and invalidated by bounds.
- Repaint the union of old and new bounds when geometry edits move vertices or constraints.

## Testing

- Add targeted unit tests for geometry model, snapping behavior, serialization metadata, and undo actions.
- Run focused tests first, then broader CMake targets when touching shared paths.
- If the local machine lacks GTK/CMake dependencies, report exactly which dependency blocked validation.

## Packaging and Updates

- Keep upstream Xournal++ packaging changes minimal and intentional.
- Semantic release owns version tags and changelog generation.
- Binary release asset workflows should be added only after the fork's build matrix is known to work.
