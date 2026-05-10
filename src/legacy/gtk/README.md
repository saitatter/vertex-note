# Legacy GTK Boundary

This folder defines the **legacy GTK shell boundary** for VertexNote.

The files are still physically located across `src/core/` and `src/util/`, but
the build now tracks them as one logical surface through
`src/legacy/LegacyBoundaries.cmake`.

## What belongs here

- GTK window/controller glue
- GTK dialogs, menus, sidebars, toolbar customization
- plugin/runtime hooks that still require the GTK shell
- utility helpers that expose GTK-only primitives

## Migration rule

No new product features should start here unless they are required to keep the
legacy shell working during transition.

New shell behavior should land in the Qt shell first, with GTK changes limited
to compatibility, bug fixes, or migration support.
