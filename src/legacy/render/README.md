# Legacy Cairo Boundary

This folder defines the **legacy Cairo rendering boundary** for VertexNote.

The files are still physically located in their historical modules, but they are
grouped in build metadata through `src/legacy/LegacyBoundaries.cmake` so we can
track what is still tied to Cairo-backed rendering and export behavior.

## What belongs here

- Cairo-backed page rendering and overlays
- Cairo-dependent background and stroke views
- Poppler/Cairo preview and export adapters
- clipboard/export helpers that still rasterize through Cairo

## Migration rule

All new interactive rendering work should target the backend-neutral render
seams and the Qt painter path first.

Changes inside this boundary should primarily reduce coupling, shrink the Cairo
surface area, or preserve compatibility while the active shell finishes moving
to Qt.
