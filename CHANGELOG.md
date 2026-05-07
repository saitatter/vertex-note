# Changelog

## vertex-note-v0.1.0 (2026-05-07)

Initial VertexNote foundation release, focused on CAD-inspired geometry architecture,
click-based technical drawing, snapping, and a repeatable Windows development path.

### ✨ Features

* Initialize the VertexNote geometry foundation on top of Xournal++
* Add object-based geometry elements with vertices, edges, stable IDs, constraints, and stroke fallbacks
* Add a provider-based snapping engine with geometry candidates
* Add click-based vertex line, polyline, and rectangle tools
* Snap VertexNote drawing tools to explicit vertices, midpoints, projections, and intersections

### 🐛 Fixes

* Address geometry snapping review feedback
* Limit intersection snapping to the active snap window instead of scanning every segment pair globally
* Rename pending input cleanup away from spline-only terminology

### 🧰 CI & Build

* Add semantic-release workflow for `main`
* Fix release publishing to use the repository GitHub token

### 📚 Docs

* Add VertexNote architecture notes and subsystem roadmap
* Add Windows MSYS2 MinGW64 setup docs and helper script
* Add upstream Xournal++ changelog reference for fork context

Full comparison: https://github.com/saitatter/vertex-note/compare/0340d0df8c85c173928c5415a3bd9ae6640018b3...vertex-note-v0.1.0
