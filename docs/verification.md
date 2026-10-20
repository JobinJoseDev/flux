# Verification for Flux 0.1

## Environment

- Linux x86_64, Ubuntu 24.04 build container
- GCC 13.3, C++20, Release configuration
- Qt 6.8.3, CMake 4.4, GNU Make for the local validation build
- Qt offscreen platform with software rendering for UI checks
- The Fedora install script selects Ninja; native Fedora execution is pending.

The Qt SDK and build tooling were obtained only for development. They are not
included in the source bundle and are not runtime services used by Flux.

## Passed

| Check | Evidence |
|---|---|
| C++ and QML compilation | Release executable built successfully; actual ELF header and nonzero binary size verified |
| Motion tests | Endpoint and midpoint interpolation, geometry, shortest rotation, color/opacity, cubic controls, easing, delay/duration boundaries and non-finite progress |
| Document tests | Stable identity across slide duplication; undo/redo; unlinking; save/reopen equivalence; invalid-file rejection without replacing the active deck; failed-save behavior; embedded image portability; PDF generation; entrance tracks; geometry bounds |
| Native UI integration | Real pointer events drag an object and click the timeline; the first scrub preserves its requested position; content and notes dialogs commit; the audience window opens and advances; zero QML runtime warnings |
| Visual editor check | Actual native framebuffer inspected at 1440 × 960; included as `docs/editor.png` |
| PDF inspection | Exported the five-slide example, confirmed five pages, rendered every page with Poppler, and visually inspected text, code, chart, formula, and geometry |
| Installer and metadata | Fedora install script passes `bash -n`; desktop MIME/AppStream XML parses |

CTest contains three targets: `motion`, `document`, and `qml-smoke`. All
passed. The UI test additionally requires its explicit completion message,
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
