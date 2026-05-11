# VertexNote

[![License: GPL-2.0-or-later](https://img.shields.io/badge/License-GPL--2.0--or--later-blue.svg)](LICENSE)
![GitHub Release](https://img.shields.io/github/v/release/saitatter/vertex-note)
[![Issues](https://img.shields.io/github/issues/saitatter/vertex-note)](https://github.com/saitatter/vertex-note/issues)
![Made with C++](https://img.shields.io/badge/Made%20with-C%2B%2B-00599C?logo=cplusplus&logoColor=white)
![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)
![Cairo Legacy](https://img.shields.io/badge/Rendering-Cairo%20Legacy-8A2BE2)
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

VertexNote has a working geometry system and a Qt Widgets shell. The inherited GTK3 shell has been removed; remaining legacy work is limited to Cairo/Poppler-era rendering, preview, and export paths.

| Area | Status |
|------|--------|
| VertexNote fork baseline | ✅ Done |
| Architecture notes | ✅ Done |
| Geometry value types | ✅ Done |
| Vertex / edge object model | ✅ Done |
| Stroke fallback generation | ✅ Done |
| Snap engine | ✅ Done |
| Click-based shape tools | ✅ Done |
| Geometry constraints | ✅ Done (6 kinds) |
| Object-based rendering | ✅ Done |
| Qt Widgets shell | ✅ Done |
| Legacy GTK3 shell | ✅ Removed |
| `.xopp` extension metadata | 🚧 Planned |
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
- Persistent constraints (Coincident, Horizontal, Vertical, FixedLength, Radius, Parallel, Perpendicular)
- Future projected 3D wireframes

### Qt Widgets shell

The Qt6-based shell (`ENABLE_QT_SHELL`) is the active product shell and the recommended path for new feature work. It includes:

- Full document open/save/export (`.xopp`, `.xoj`, `.xopt`, `.pdf`)
- Pressure-sensitive pen, highlighter, eraser (whole-stroke & segment)
- Click-based shape tools (line, rectangle, circle, arc, polyline, construction geometry)
- Geometry editing with vertex drag, snapping, and constraint application
- Element selection, clipboard, z-order, page & layer management
- Navigation history, annotated page navigation, text search
- Settings, printing, image insertion, plugin UI bridge

See [docs/vertex-note-architecture.md](docs/vertex-note-architecture.md) for the core roadmap and
[docs/qt-migration-bootstrap.md](docs/qt-migration-bootstrap.md) for the Qt shell feature log.

---

## 🛠️ Local Development on Windows

The current recommended Windows path is **MSYS2 MinGW64**.

```powershell
.\scripts\mingw64-dev.ps1
```

### Qt shell

Requires `mingw-w64-x86_64-qt6-base` and `mingw-w64-x86_64-qt6-printsupport`.

- `.\scripts\mingw64-dev.ps1 configure`
- `.\scripts\mingw64-dev.ps1 build`
- `.\scripts\mingw64-dev.ps1 test`
- `.\scripts\mingw64-dev.ps1 run`

See [docs/windows-mingw64.md](docs/windows-mingw64.md) for the full setup and manual commands.

---

## 🎯 Geometry Roadmap

### Phase 1: Vertex System ✅

- Stable geometry IDs, vertices, edges
- Object-local geometry containers
- Stroke fallback generation
- Core model tests

### Phase 2: Snapping Engine ✅

- Provider-based `SnapEngine`
- `GridSnapProvider` and `GeometrySnapProvider`
- Vertex, endpoint, midpoint, edge projection, and intersection snapping

### Phase 3: Geometric Primitives ✅

- Click-based line, polyline, rectangle, arc, circle tools
- Construction line and construction circle tools
- Geometry snapping with grid fallback

### Phase 4: Editable Constraints ✅

- Coincident, Horizontal, Vertical, FixedLength, Radius, Parallel, Perpendicular
- Constraint creation, deletion, and value editing
- Connected-component solver
- Vertex drag with constraint enforcement

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

Cairo, Poppler, CMake, Qt6, and platform-specific dependencies are still required for several migration-era rendering/export paths. Until VertexNote has dedicated build docs, use the upstream Xournal++ setup guides as the baseline:

- Linux setup: `linux-setup/`
- macOS setup: `mac-setup/`
- Windows setup: `windows-setup/`

---

## 🔄 Releases

VertexNote uses **semantic-release** with Conventional Commits. Releases are published from the manual GitHub Actions release workflow after `main` is ready.

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
- VertexNote itself builds on ideas and code from the original Xournal project
