# Apply the Flux 0.2 update

The patch is based on published `main` commit `9e1dd33` (the green Fedora
logging fix). It adds grouping and proportional multi-object resizing.
It does not replace your repository or modify any historical tags.

Download `Flux-0.2.0.patch` to `~/Downloads`. Close Flux, then:

```bash
cd /home/swizzy/Projects/flux
git status --short
```

If you have local changes, commit or stash them before applying the update.
With a clean working tree:

```bash
git switch main
git pull --ff-only origin main
git apply --check ~/Downloads/Flux-0.2.0.patch
git apply ~/Downloads/Flux-0.2.0.patch
bash packaging/install-fedora.sh
```

The existing installer configures CMake/Ninja, builds, runs all three CTest
targets, and installs to `~/.local`. It does not change your saved projects.
It needs the Fedora development packages listed in the main README.

After all tests pass, launch `~/.local/bin/flux`. Select several objects,
right-click **Group** (or Ctrl+G), and drag a corner handle. Test Escape,
undo/redo, save/reopen, and Ctrl+Shift+G. Existing v1 decks still open; using
groups upgrades a deck to file format v2, which needs Flux 0.2 or newer.

Review and commit the update as a new commit:

```bash
git diff --stat
git add CHANGELOG.md CMakeLists.txt README.md docs/architecture.md docs/verification.md docs/upgrading-to-0.2.md docs/group-resize.png
git add packaging/io.fluxpresent.Flux.metainfo.xml qml/EditCanvas.qml qml/Inspector.qml qml/Main.qml
git add src/Document.cpp src/Document.hpp src/SmokeTest.hpp src/main.cpp tests/document_test.cpp
git commit -m "Add persistent grouping and collective resizing"
git push origin main
```

Check the new run at https://github.com/JobinJoseDev/flux/actions. The
published baseline is green; these changes have passed locally on Qt 6.8.3
with the offscreen platform and software rendering. A fresh Fedora CI result
requires pushing this commit. Keep the existing v0.1, v0.1.1, and v0.1.2 tags.

The optional `Flux-0.2.0-source.zip` contains the same source without Git
metadata or build products. Use the patch for your existing repository;
the source archive is useful for a separate build.
