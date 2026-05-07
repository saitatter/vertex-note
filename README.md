# VertexNote

[![License: GPL-2.0-or-later](https://img.shields.io/badge/License-GPL--2.0--or--later-blue.svg)](LICENSE)
![GitHub Release](https://img.shields.io/github/v/release/saitatter/vertex-note)
[![Issues](https://img.shields.io/github/issues/saitatter/vertex-note)](https://github.com/saitatter/vertex-note/issues)
![Made with C++](https://img.shields.io/badge/Made%20with-C%2B%2B-00599C?logo=cplusplus&logoColor=white)
![GTK3](https://img.shields.io/badge/GTK-3-4A90D9?logo=gtk&logoColor=white)
![Cairo](https://img.shields.io/badge/Rendering-Cairo-8A2BE2)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)

> CAD-inspired technical note-taking based on Xournal++, with precision geometry, vertex editing, snapping, and future lightweight 3D wireframe tools.

VertexNote is a long-term fork of **Xournal++**. The goal is not to replace Xournal++'s handwriting workflow, but to preserve it and add a clean object-based geometry layer for technical notes, diagrams, engineering sketches, measurements, construction lines, and precise click-based drawing.

---

## ✨ Goals

- Preserve existing Xournal++ note-taking, PDF annotation, handwriting, image, text, LaTeX, audio, export, and plugin behavior
- Add explicit vertices, edges, and object-based geometric primitives
- Support snapping to grid points, custom vertices, endpoints, midpoints, projections, and intersections
- Support line and polyline creation from discrete clicks, not only drag gestures
- Introduce editable geometric constraints incrementally
- Prepare the architecture for lightweight 3D wireframe primitives and 2D projection tools
- Keep `.xopp` compatibility through stroke-compatible fallbacks

---

## 🧭 Current Status

VertexNote is in early foundation work.

| Area | Status |
|------|--------|
| Xournal++ fork baseline | ✅ Started |
| Architecture notes | ✅ Added |
| Geometry value types | ✅ Started |
| Vertex / edge object model | ✅ Started |
| Stroke fallback generation | ✅ Started |
| Snap engine | 🚧 Planned |
| Object-based rendering | 🚧 Planned |
| `.xopp` extension metadata | 🚧 Planned |
| Constraint solver | 🧪 Future |
| 3D wireframe layer | 🧪 Future |

---

## 🧱 Architecture

VertexNote keeps Xournal++'s existing stroke model for handwritten content and adds a separate object-based geometry model.

### Stroke-based systems stay stroke-based

- Pen and highlighter input
- Pressure-sensitive strokes
- Eraser behavior
- Existing drag-created line, rectangle, ellipse, arrow, and spline tools
- Shape recognizer output
- Existing `.xopp` documents
- Export fallback paths

### Object-based systems become VertexNote geometry

- Explicit vertices
- Edges, polylines, arcs, circles, and construction geometry
- Editable endpoints and control points
- Snapping targets and intersections
- Persistent constraints
- Future projected 3D wireframes

See [docs/vertex-note-architecture.md](docs/vertex-note-architecture.md) for the current roadmap and subsystem map.

---

## 🎯 Geometry Roadmap

### Phase 1: Vertex System

- Add stable geometry IDs
- Add vertices and edges
- Add object-local geometry containers
- Add stroke fallback generation
- Add tests for core model behavior

### Phase 2: Snapping Engine

- Add provider-based snapping
- Wrap existing grid snapping as a provider
- Add endpoint, midpoint, vertex, edge projection, and intersection snapping
- Add per-page spatial indexes for large notebooks

### Phase 3: Geometric Primitives

- Add click-based line and polyline tools
- Add object-based rectangle, arc, and circle tools
- Keep existing Xournal++ drag tools unchanged

### Phase 4: Editable Constraints

- Add constraint objects
- Add vertex handles and geometry edit mode
- Add grouped undo/redo for geometry edits
- Add a small connected-component solver

### Phase 5: Lightweight 3D Wireframe Layer

- Add 3D vertices and edges
- Add projection caches
- Add camera/projection settings
- Expose projected vertices and intersections to the snap engine

---

## ⚙️ Build From Source

VertexNote currently inherits the Xournal++ build system.

```bash
git clone https://github.com/saitatter/vertex-note.git
cd vertex-note
cmake -S . -B build
cmake --build build
```

GTK3, Cairo, Poppler, CMake, and platform-specific dependencies are required. Until VertexNote has dedicated build docs, use the upstream Xournal++ setup guides as the baseline:

- Linux setup: `linux-setup/`
- macOS setup: `mac-setup/`
- Windows setup: `windows-setup/`

---

## 🔄 Releases

VertexNote uses **semantic-release** with Conventional Commits. On every push to `main`, CI checks if a new version should be published.

- Use Conventional Commits: `feat: ...`, `fix: ...`, `chore: ...`
- Breaking changes: use `!` or a `BREAKING CHANGE:` footer
- Release tags use the `vertex-note-v{version}` format to avoid collisions with inherited Xournal++ tags
- Release notes are grouped with emoji categories
- Changelog generation is configured through `pyproject.toml` and `templates/CHANGELOG.md.j2`

Examples:

```text
feat: add geometry object model
fix: preserve fallback stroke bounds
test: cover vertex edge validation
chore: update release metadata
```

---

## 🧪 Testing

Targeted unit tests live under `test/unit_tests`.

```bash
cmake -S . -B build -DENABLE_GTEST=ON
cmake --build build --target test-units
ctest --test-dir build
```

When touching geometry, add focused tests before integrating the code into rendering, input handling, or serialization.

---

## 🤝 Contributing

PRs are welcome once the public fork is connected. Please:

- Keep commits small and conventional
- Preserve existing Xournal++ behavior unless the change is explicitly VertexNote-related
- Prefer incremental refactors over rewrites
- Add tests for geometry model, snapping, serialization, undo/redo, and user-facing regressions
- Document architectural tradeoffs when touching shared model or rendering paths

---

## 📄 License

GPL-2.0-or-later, inherited from Xournal++.

---

## 🙏 Credits

- VertexNote is based on **Xournal++** by the Xournal++ team and contributors
- Xournal++ itself builds on ideas and code from the original Xournal project
