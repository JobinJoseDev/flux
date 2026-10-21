# Verification

## Individual resizing (unreleased)

Validated locally on Fedora Linux 44, Qt 6.11.2, C++20, Release, CMake/Ninja.
The original clean `f439f906` baseline passed its three CTest targets before edits.

All five CTest targets pass after this phase: `motion`, `resize`, `document`,
`resize-ui`, and the existing `qml-smoke`. The portable resize suite runs
2,822 checks. The real QML suite covers 96 handle/rotation/modifier combinations,
three successive pointer moves per gesture, rendered fixed-anchor positions,
immutable previews, one-step undo/redo, no-op clicks, Escape, lost grab,
selection/document changes, and preserved collective resizing.

The full run took 14.11 seconds using Qt offscreen/software rendering.
Real Wayland pointer and GPU behavior remain device checks; headless input
coverage is not evidence of compositor behavior. No document-format or tag
changes were made.

## Historical verification for Flux 0.2.0

## Environment

- Linux x86_64, Ubuntu 24.04 build container
- GCC 13.3, C++20, Release configuration
- Qt 6.8.3, CMake 4.4, GNU Make for the local validation build
- Qt offscreen platform with software rendering for UI checks
- The Fedora install script selects Ninja.
- Published baseline `9e1dd33` passed Fedora CI, including all three CTest
  targets: https://github.com/JobinJoseDev/flux/actions/runs/39388712353.
  The v0.2 changes below are validated locally; their Fedora CI run is pending push.

The Qt SDK and build tooling were obtained only for development. They are not
included in the source bundle and are not runtime services used by Flux.

## Regression coverage

The existing suite is retained. The v0.2 run covers the following new behavior
in addition to the original checks listed below:

- Groups: selection as a unit, full rectangle enclosure, nesting, one-level
  ungrouping, version upgrade/undo, save/reopen, and v1 compatibility.
- Clipboard: independent object/group IDs on copy and duplicate, nested
  structure preservation, and importing a group into a v1 deck.
- Validation: rejects non-array memberships, cycles, inconsistent parents,
  single-member groups, v1 group payloads, and excessive nesting.
- Resize: rotated bounds, fixed anchor, shared scale limits, font/stroke/radius
  and Bézier-control scaling, unselected-object preservation, stable Morph
  matches, one-step undo/redo, and non-finite/no-op input handling.
- Real QML input: Group/Ungroup menu clicks and shortcuts, whole-group click
  selection, corner-drag preview and commit, Escape and lost-grab cancellation,
  keyboard duplication, and zero QML runtime warnings.

The group-resize screenshot is an actual 1440 × 960 native framebuffer,
captured through the smoke test and visually inspected.

## Existing checks retained

| Check | Evidence |
|---|---|
| C++ and QML compilation | Release executable built successfully; actual ELF header and nonzero binary size verified |
| Motion tests | Endpoint and midpoint interpolation, geometry, shortest rotation, color/opacity, cubic controls, easing, delay/duration boundaries and non-finite progress |
| Document tests | Stable identity across slide duplication; undo/redo; unlinking; save/reopen equivalence; invalid-file rejection without replacing the active deck; failed-save behavior; embedded image portability; PDF generation; entrance tracks; geometry bounds; clipboard identity/content/image preservation; cut and paste undo; malformed clipboard rejection; plain-text paste; single-step positional insertion |
| Native UI integration | Real pointer events drag an object and click the timeline; the first scrub preserves its requested position; object/canvas/thumbnail right-click menus open on the correct target; actual menu clicks copy/paste and duplicate slides; Shift+F10 and Escape work; keyboard nudge works after dismissal; content and notes dialogs commit; the audience window opens and advances; zero QML runtime warnings |
| Visual editor check | Actual native framebuffer inspected at 1440 × 960; included as `docs/editor.png` |
| Context-menu visual check | Actual menu framebuffer inspected; included as `docs/context-menu.png` |
| Rectangle selection | Rotated bounds, enclosure vs intersection, reverse drag direction, additive selection, selection history, batch geometry/clipboard/delete/duplicate, relative layout, and atomic capacity rejection |
| Selection UI | Real pointer rectangle, selection persistence, shared drag, right-click preservation, Shift/Ctrl-click, select-all, batch keyboard clipboard/delete, and Escape cancellation; native framebuffer in `docs/rectangle-selection.png` |
| Reverse playback | Backward transition start and progress, edge ownership, completion without entrance replay, right-click navigation, mid-flight direction changes, first-slide boundary, and Cut behavior |
| PDF inspection | Exported the five-slide example, confirmed five pages, rendered every page with Poppler, and visually inspected text, code, chart, formula, and geometry |
| Installer and metadata | Fedora install script passes `bash -n`; desktop MIME/AppStream XML parses |

CTest contains three targets: `motion`, `document`, and `qml-smoke`. All
passed in the v0.2 local Release build: 1,018 motion checks, 13 document test
cases (plus setup/cleanup), and the expanded 11.96-second UI probe. Total
CTest time was 12.26 seconds. The UI test additionally requires its explicit completion message,
so an empty or non-running executable cannot silently satisfy the check.

## Still requires a Fedora device

- Real GPU frame-time, memory, and battery measurements
- Wayland/X11 compositor and input-method behavior
- Monitor routing, fullscreen behavior, and presenter-window placement with
  two physical displays
- Native file-picker behavior and desktop-menu/file-association integration
- Larger image-heavy decks, accessibility audit, and long editing sessions

No 60/120-fps, memory-footprint, or production-readiness guarantee is made.
This bundle contains the source and a Fedora build/install script, not a
prebuilt RPM, Flatpak, AppImage, or bundled Qt runtime.
