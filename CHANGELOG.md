# Flux changes

## Unreleased

- Eight corner/edge handles resize individual objects in their rotated axes.
- Opposite anchors stay fixed; Shift locks aspect ratio and Ctrl holds center.
- Preview, one-step undo/redo, Escape and lost-grab cancellation cover all handles.
- Existing proportional group resizing and document formats are preserved.
- Portable geometry and real QML pointer tests cover rotations, modifiers,
  size limits, repeated movement, cancellation and history.

## 0.2.0 — persistent groups and collective resizing

- Group/Ungroup through the right-click menu, inspector, Ctrl+G and Ctrl+Shift+G.
- Nested groups preserve their structure; ungrouping removes one level.
  Clicking, toggling, and rectangle selection treat each outer group as a unit.
- Four corner handles proportionally resize groups and temporary selections.
  The opposite corner stays fixed; text, strokes, radii, and path controls scale.
- Width and height fields offer the same proportional resize numerically.
- Live previews commit once on release; Escape or a lost pointer grab cancels.
- Grouping and resizing preserve Morph identities. Copies receive independent
  object and group IDs; slide duplicates retain both.
- File format v2 stores group paths and validates nesting. Existing v1 decks
  still load and stay v1 until groups are used. Grouped clipboard data uses v2.
- Tests cover persistence, nesting, clipboard isolation, invalid hierarchies,
  geometry, limits, Morph, undo/redo, menu and keyboard actions, and real drags.
- Retains the Fedora CI fix forcing Qt diagnostics onto stderr. The expanded
  smoke test keeps its success marker and has a 30-second timeout.

## 0.1.2 — rectangle selection and reverse transitions

- Drag a rectangle in either direction to select fully enclosed objects,
  including rotated objects. Shift/Ctrl adds to a rectangle or toggles clicks.
- Selection outlines and a shared bounding box show what will be affected.
- Move, nudge, duplicate, cut/copy/paste, delete, and align selections with
  one undo step per operation. Paste preserves relative geometry and layers.
- Right-click preserves a multi-selection; single-object controls are
  disabled where a batch operation is not yet supported.
- Morph and Fade now run backward on Left, Page Up, and right-click. They
  use the original transition edge and retrace the same path and easing.
- Reversing direction during a transition preserves its current progress.
- Returning to a slide shows its completed state without replaying entrances.
- Presentation exit explicitly restores focus to the editor.
- The version-1 `.flux` format and existing saved decks are unchanged.

## 0.1.1 — editing context menus

- Object, slide, and canvas right-click menus expose working contextual actions.
- Right-click selects its target without moving the object.
- Single-object cut/copy/paste uses the system clipboard. It preserves Flux
  content and embedded images; external plain text becomes a text box.
- Insert or paste at a canvas position with one undo step. New objects are
  kept within slide bounds when their dimensions fit the slide.
- Menus support keyboard navigation, Escape, Menu, and Shift+F10.
- Slide names can be changed directly from the thumbnail menu.
- The `.flux` format remains version 1, compatible with 0.1.0 projects.
- Automated UI checks cover actual right-click events, Copy/Paste menu
  activation, slide duplication, keyboard opening/dismissal, and restored focus.

## Next milestones

1. Alignment and distribution between objects, followed by richer group editing.
2. Motion authoring: visible draggable path handles, click-triggered builds,
   and a clearer animation sequence editor.
3. Fedora release hardening: fonts, display routing, recovery, larger decks,
   GPU profiling, accessibility, and an installable package.
4. Broader content tools and PPTX interchange after the editing and motion
   model is stable. Import should report unsupported content explicitly.
