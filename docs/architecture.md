# Architecture decisions

## C++20 + Qt 6 Quick/QML

This project uses direct Qt/C++ integration. Rust could own the core through
CXX-Qt, but would add a Rust toolchain and an interop boundary without removing
the need to understand Qt object lifetimes and its rendering model. The first
version favors a smaller build and integration surface. This is not a claim
that C++ is intrinsically faster than Rust.

QML owns layout, input, panels, dialogs, and the retained scene graph. C++ owns
the deck, operations, file validation, saves, recovery, interpolation, and
technical rendering. There is no web view and no Python runtime in the app.

| Module | Responsibility |
|---|---|
| `src/core/resize.hpp` | Qt-free individual resize geometry with rotated anchors and shared constraints |
| `src/core/motion.hpp` | Qt-free pose interpolation, easing, cubic paths, timeline normalization |
| `src/MotionPose.hpp` | Typed QObject pose adapter; scalar properties update existing QML delegates |
| `src/Document.*` | Document state, slide/object operations, matching, history, local persistence, PDF export |
| `src/Render.*` | Shared content/PDF painter, code formatting, bounded formula layout, charts and tables |
| `qml/NodeVisual.qml` | Native text/shape/image items; technical texture item |
| `qml/MotionStage.qml` | Persistent delegate per transition track; no per-frame model replacement |
| `qml/EditCanvas.qml` | Local drag/resize feedback, one document operation at gesture completion |
| `qml/Presentation.qml` | Audience playback and timed entrance sequence |
| `qml/Presenter.qml` | Current/next slide, notes, elapsed time, controls |

## Data and identity

A deck stores a title, fixed slide size, and an ordered array of
slides. Slides contain a UUID, name, background, notes, transition settings,
and an ordered array of objects. Object IDs are unique within one slide.
Duplicating a slide preserves its object IDs; duplicating an individual
object creates a new one. Morph matches equal IDs of the same kind.

Version 2 adds an optional `groups` array to each leaf object, ordered from
outermost to innermost group ID. Geometry stays in slide coordinates and
objects retain their layer order and Morph IDs. A group path must have
consistent ancestors, no repeated IDs, and at most 16 levels; every group
must contain at least two leaves. Creating or pasting groups upgrades the
document atomically to v2. Ungrouping removes only the outer path entry.
Existing v1 documents load without migration until they use groups.

Selection expands any selected member to its whole outer group. Rectangle
selection uses the union of member bounds. Copy and duplicate remap every
object ID and group ID consistently; slide duplication preserves them.
Native grouped clipboard payloads use v2 so older apps reject them.

The transition compiler creates a union of source/target objects once.
Matching objects interpolate geometry and colors. Unmatched objects fade
in or out. Content changes crossfade. The interpolator uses the shortest
rotation across the 0/360-degree seam. Bézier paths operate on the object's
top-left position in document coordinates.

Selection is an ordered list of IDs in editor state, separate from the saved
deck. History snapshots retain the whole selection. Rectangle selection uses
the rotated object's axis-aligned bounds; the rectangle must enclose the
whole object. Batch edits commit one document snapshot. Drag feedback uses
shared visual offsets until release, without rebuilding the document per frame.

Individual resizing computes geometry from the original node and pointer delta
in slide coordinates, inverse-rotated into the original object axes. A fixed
opposite corner/edge midpoint (or Ctrl center) determines the new position.
Shift uses a common scale; limits constrain the geometry before reconstructing
the anchored position. QML never maps deltas through a changing local frame.
The document API recomputes the same geometry on release and adds one history
entry. Cancellation discards only the preview.

Collective resizing uses a common positive scale about the opposite corner of
the selection bounds. Proportional scaling preserves rotated rectangles
without introducing shear. It transforms leaf positions, dimensions, font
size, stroke width, corner radius, and Bézier controls. A shared allowable
scale interval enforces geometry and font limits without distorting the
selection. QML transforms existing visuals during the gesture; release
validates and commits one document operation. Escape, selection/document
changes, or a lost pointer grab cancel the preview.

Reverse presentation playback evaluates the original transition tracks from
progress 1 to 0. Swapping endpoints would change curved paths and asymmetric
easing, so the tracks and their ordering remain intact. The later slide owns
the transition edge in either direction. A mid-transition direction change
restarts the clock from its current progress with a proportional duration.
Backward completion shows the prior slide's final state without entrance builds.

The current JSON format keeps images inside each image object. Qt's implicit
sharing helps avoid copying unchanged state, but large image-heavy documents
and snapshot history need profiling. A future format should deduplicate
assets by content hash and store them in an archive next to document JSON.

## Frame-time choices

- Transition tracks are prepared once. Existing QML delegates receive pose
  updates, so the model is not rebuilt at 60 Hz.
- The pure C++ interpolation path does not allocate memory.
- Dragging/resizing is visual until the gesture ends, then becomes one undo
  action and one autosave scheduling event.
- Technical content is painted into a Qt Quick texture. During Morph the
  texture is transformed at its original dimensions; it is not repainted
  for every interpolated width/height.
- No periodic editor repaint timer runs while idle. Only animation and the
  presenter elapsed-time timer are active when needed.
- Slide thumbnails are direct small scene views in a virtualized ListView.

These are architectural choices, not measured GPU performance guarantees.
The validation environment uses offscreen/software rendering; benchmark on
Fedora hardware before setting frame-rate or memory targets.

## Follow-on work

1. Hardware/Wayland validation, accessibility and keyboard focus audit,
   alignment/distribution between objects, and richer text and group editing.
2. Click-triggered build sequences, interactive path handles, attached
   connectors, slide master/theme system, and asset deduplication.
3. Full equation renderer integration, richer chart semantics and language
   detection, large-document profiling.
4. PPTX adapter with explicit feature-loss reporting; round-trip tests using
   real decks. Keep native motion semantics independent of PPTX serialization.
5. Reproducible Flatpak/RPM release pipeline after Fedora hardware validation.

## Primary references

- Qt Quick scene graph: https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html
- QML/CMake modules: https://doc.qt.io/qt-6/qt-add-qml-module.html
- Atomic file writes: https://doc.qt.io/qt-6/qsavefile.html
- PDF writer: https://doc.qt.io/qt-6/qpdfwriter.html
- Rust/Qt alternative: https://kdab.github.io/cxx-qt/book/
- Fedora Qt base: https://packages.fedoraproject.org/pkgs/qt6-qtbase/qt6-qtbase-devel/
- Fedora QML development package: https://packages.fedoraproject.org/pkgs/qt6-qtdeclarative/qt6-qtdeclarative-devel/
