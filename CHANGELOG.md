# Flux changes

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

1. Persistent grouping/ungrouping, collective resizing, and alignment and
   distribution between objects.
2. Motion authoring: visible draggable path handles, click-triggered builds,
   and a clearer animation sequence editor.
3. Fedora release hardening: fonts, display routing, recovery, larger decks,
   GPU profiling, accessibility, and an installable package.
4. Broader content tools and PPTX interchange after the editing and motion
   model is stable. Import should report unsupported content explicitly.
