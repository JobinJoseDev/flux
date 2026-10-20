# Flux changes

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

1. Multi-select, selection rectangles, grouping, and batch alignment and
   distribution. Extend the clipboard and undo operations to selections.
2. Motion authoring: visible draggable path handles, click-triggered builds,
   and a clearer animation sequence editor.
3. Fedora release hardening: fonts, display routing, recovery, larger decks,
   GPU profiling, accessibility, and an installable package.
4. Broader content tools and PPTX interchange after the editing and motion
   model is stable. Import should report unsupported content explicitly.
