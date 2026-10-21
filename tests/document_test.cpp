#include "Document.hpp"
#include <QClipboard>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>
#include <limits>

class DocumentTest : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void duplicateMorphUndo() {
        Document d;
        d.newDeck(false);
        const auto first = d.objects();
        const int count = first.size();
        d.duplicateSlide();
        QCOMPARE(d.slideIndex(), 1);
        QCOMPARE(d.objects().size(), count);
        const auto id = first.last().toMap()["id"].toString();
        d.select(id);
        d.updateObject(id, {{"x", 250},
                            {"y", 300},
                            {"path", "bezier"},
                            {"c1x", 50},
                            {"c1y", 100},
                            {"c2x", 200},
                            {"c2y", 350}});
        auto tracks = d.morphTracks(1);
        QCOMPARE(tracks.size(), count);
        bool found = false;
        for (const auto& v : tracks) {
            auto t = v.toMap();
            auto b = t["to"].toMap();
            if (b["id"] == id) {
                QCOMPARE(b["x"].toDouble(), 250.);
                QCOMPARE(t["from"].toMap()["x"], first.last().toMap()["x"]);
                found = true;
            }
        }
        QVERIFY(found);
        d.undo();
        QCOMPARE(d.selected()["x"], first.last().toMap()["x"]);
        d.redo();
        QCOMPARE(d.selected()["x"].toDouble(), 250.);
        d.unlinkObject();
        QCOMPARE(d.morphTracks(1).size(), count + 1);
    }
    void unchangedTextTracks() {
        Document d;
        d.newDeck(false);
        d.addSlide("blank");
        d.addObject("text");
        const auto id = d.selectedId();
        const QString text = QString::fromUtf8("Office é العربية 👩‍👩‍👧‍👦\nSame text");
        d.updateObject(id, {{"text", text}, {"fontSize", 24}, {"w", 600}, {"h", 180}});
        const auto original = d.selected();
        const auto version = d.json().value("version");
        d.duplicateSlide();
        d.updateObject(id, {{"fontSize", 48}, {"w", 240}, {"h", 72}, {"align", "right"}});
        const auto track = d.morphTracks(d.slideIndex()).first().toMap();
        QVERIFY(track["spatialText"].toBool());
        QVERIFY(!track["crossfade"].toBool());
        QCOMPARE(track["from"].toMap(), original);
        QCOMPARE(track["to"].toMap()["text"].toString(), text);
        QCOMPARE(d.json()["version"], version);
        const auto before = d.json();
        d.updateObject(id, {{"text", text + " changed"}});
        const auto changed = d.morphTracks(d.slideIndex()).first().toMap();
        QVERIFY(!changed["spatialText"].toBool());
        QVERIFY(changed["crossfade"].toBool());
        d.undo();
        QCOMPARE(d.json(), before);
        QCOMPARE(d.morphTracks(d.slideIndex()).first().toMap(), track);
        d.cleanupRecovery();
    }
    void persistenceAndValidation() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Document d;
        d.newDeck(false);
        d.addObject("equation");
        auto id = d.selectedId();
        d.updateObject(id, {{"text", "\\frac{-b + \\sqrt{b^2-4ac}}{2a}"},
                            {"animation", "path"},
                            {"delay", .4},
                            {"duration", 1.2}});
        d.updateSlide({{"notes", "Presenter-only notes"}});
        const auto expected = d.json();
        const QUrl file = QUrl::fromLocalFile(tmp.filePath("portable.flux"));
        QVERIFY(d.save(file));
        QVERIFY(!d.dirty());
        Document loaded;
        QVERIFY(loaded.open(file));
        QCOMPARE(loaded.json(), expected);
        QCOMPARE(loaded.buildDuration(0), 1.6);
        auto broken = expected;
        auto ss = broken["slides"].toArray();
        auto s = ss[0].toObject();
        auto ns = s["objects"].toArray();
        ns.append(ns[0]);
        s["objects"] = ns;
        ss[0] = s;
        broken["slides"] = ss;
        QString why;
        QVERIFY(!Document::validate(broken, &why));
        QVERIFY(why.contains("unique"));
        QFile corrupt(tmp.filePath("bad.flux"));
        QVERIFY(corrupt.open(QIODevice::WriteOnly));
        corrupt.write(QJsonDocument(broken).toJson());
        corrupt.close();
        QVERIFY(!loaded.open(QUrl::fromLocalFile(corrupt.fileName())));
        QCOMPARE(loaded.json(), expected);
        QVERIFY(!loaded.save(QUrl::fromLocalFile(tmp.filePath("missing/fail.flux"))));
        QCOMPARE(loaded.json(), expected);
    }
    void embedImageAndExport() {
        QTemporaryDir tmp;
        QImage image(200, 100, QImage::Format_ARGB32);
        image.fill(QColor("#ffb48d"));
        auto path = tmp.filePath("image.png");
        QVERIFY(image.save(path));
        Document d;
        d.newDeck(false);
        QVERIFY(d.importImage(QUrl::fromLocalFile(path)));
        QVERIFY(d.selected()["source"].toString().startsWith("data:image/png;base64,"));
        const auto embedded = d.selected()["source"];
        QFile::remove(path);
        QVERIFY(d.copyObject());
        QVERIFY(d.pasteObjectAt(40, 40));
        QCOMPARE(d.selected()["source"], embedded);
        for (auto kind : {"code", "equation", "chart", "table", "arrow"})
            d.addObject(kind);
        auto pdf = QUrl::fromLocalFile(tmp.filePath("deck.pdf"));
        QVERIFY(d.exportPdf(pdf));
        QFile f(pdf.toLocalFile());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.read(5), QByteArray("%PDF-"));
        QVERIFY(f.size() > 2000);
    }
    void editsAndBuilds() {
        Document d;
        d.newDeck(false);
        d.addObject("rect");
        auto id = d.selectedId();
        d.updateObject(id, {{"animation", "rise"}, {"delay", .5}, {"duration", .8}});
        auto ts = d.entranceTracks(0);
        auto t = ts.last().toMap();
        QCOMPARE(t["from"].toMap()["opacity"].toDouble(), 0.);
        QCOMPARE(t["from"].toMap()["y"].toDouble(), d.selected()["y"].toDouble() + 90);
        d.duplicateObject();
        QVERIFY(d.selectedId() != id);
        d.undo();
        QCOMPARE(d.selectedId(), id);
        d.updateObject(id, {{"w", -9}, {"opacity", 9}, {"fontSize", 999}});
        QCOMPARE(d.selected()["w"].toDouble(), 16.);
        QCOMPARE(d.selected()["opacity"].toDouble(), 1.);
        QCOMPARE(d.selected()["fontSize"].toDouble(), 240.);
        d.deleteObject();
        QVERIFY(d.selectedId().isEmpty());
        d.undo();
        QCOMPARE(d.selectedId(), id);
        d.cleanupRecovery();
    }
    void clipboardAndInsertion() {
        Document d;
        d.newDeck(false);
        const auto beforeInsert = d.json();
        d.addObjectAt("code", 1200, 700);
        QCOMPARE(d.selected()["x"].toDouble(), 640.);
        QCOMPARE(d.selected()["y"].toDouble(), 420.);
        d.undo();
        QCOMPARE(d.json(), beforeInsert); // Placement and creation are one undo step.
        d.redo();
        d.updateObject(
            d.selectedId(),
            {{"text", "const int answer = 42;"}, {"path", "bezier"}, {"c1x", 500.}, {"c1y", 250.}});
        const auto original = d.selected();
        const auto originalId = d.selectedId();
        const auto beforeCopy = d.json();
        QVERIFY(d.copyObject());
        QVERIFY(d.canPaste());
        QCOMPARE(d.json(), beforeCopy);
        QCOMPARE(QGuiApplication::clipboard()->text(), original["text"].toString());
        d.addSlide("blank");
        const auto beforePaste = d.json();
        QVERIFY(d.pasteObjectAt(120, 100));
        QVERIFY(d.selectedId() != originalId);
        QCOMPARE(d.selected()["text"], original["text"]);
        QCOMPARE(d.selected()["kind"], original["kind"]);
        QCOMPARE(d.selected()["x"].toDouble(), 120.);
        QCOMPARE(d.selected()["y"].toDouble(), 100.);
        QCOMPARE(d.selected()["c1x"].toDouble(), -20.);
        QCOMPARE(d.selected()["c1y"].toDouble(), -70.);
        d.undo();
        QCOMPARE(d.json(), beforePaste);
        d.redo();
        const auto pastedId = d.selectedId();
        const auto beforeCut = d.json();
        d.cutObject();
        QVERIFY(d.objects().isEmpty());
        d.undo();
        QCOMPARE(d.json(), beforeCut);
        QCOMPARE(d.selectedId(), pastedId);
        QVERIFY(d.pasteObject());
        QCOMPARE(d.selected()["x"].toDouble(), 144.);
        QVERIFY(d.selectedId() != pastedId);
        QVERIFY(d.pasteObject());
        QCOMPARE(d.selected()["x"].toDouble(), 168.);
        d.cleanupRecovery();
    }
    void clipboardValidationAndText() {
        Document d;
        d.newDeck(false);
        auto clipboard = QGuiApplication::clipboard();
        clipboard->clear();
        QVERIFY(!d.canPaste());
        const auto before = d.json();
        auto malformed = new QMimeData;
        malformed->setData("application/x-flux-object+json", "{broken");
        clipboard->setMimeData(malformed);
        QVERIFY(!d.pasteObject());
        QCOMPARE(d.json(), before);
        QVERIFY(!d.canUndo());
        d.addObject("rect");
        auto invalid = QJsonObject::fromVariantMap(d.selected());
        invalid["kind"] = "unknown";
        auto unsupported = new QMimeData;
        unsupported->setData(
            "application/x-flux-object+json",
            QJsonDocument(
                QJsonObject{{"format", "flux-object"}, {"version", 1}, {"object", invalid}})
                .toJson());
        clipboard->setMimeData(unsupported);
        const auto beforeInvalid = d.json();
        QVERIFY(!d.pasteObject());
        QCOMPARE(d.json(), beforeInvalid);
        clipboard->setText("A pasted idea\nSecond line");
        QVERIFY(d.pasteObjectAt(80, 90));
        QCOMPARE(d.selected()["kind"].toString(), QString("text"));
        QCOMPARE(d.selected()["text"].toString(), QString("A pasted idea\nSecond line"));
        const auto beforeLong = d.json();
        clipboard->setText(QString(12001, 'a'));
        QVERIFY(!d.pasteObject());
        QCOMPARE(d.json(), beforeLong);
        d.cleanupRecovery();
        clipboard->clear();
    }
    void rectangleSelectionAndBatchEdits() {
        Document d;
        d.newDeck(false);
        d.addSlide("blank");
        d.addObjectAt("rect", 100, 100);
        const auto first = d.selectedId();
        d.updateObject(first, {{"w", 80}, {"h", 60}});
        d.addObjectAt("ellipse", 250, 100);
        const auto second = d.selectedId();
        d.updateObject(second, {{"w", 80}, {"h", 60}});
        d.addObjectAt("rect", 500, 100);
        const auto rotated = d.selectedId();
        d.updateObject(rotated, {{"w", 100}, {"h", 100}, {"rotation", 45}});
        const auto before = d.json();
        d.selectRectangle(50, 50, 350, 200);
        QCOMPARE(d.selectedIds(), QStringList({first, second}));
        QCOMPARE(d.json(), before); // Selection is not a document mutation.
        d.selectRectangle(400, 250, -350, -200);
        QCOMPARE(d.selectedIds(), QStringList({first, second}));
        d.selectRectangle(120, 110, 30, 30);
        QCOMPARE(d.selectionCount(), 0); // Merely touching an object is insufficient.
        d.selectRectangle(500, 100, 100, 100);
        QCOMPARE(d.selectionCount(), 0); // Rotation extends outside the unrotated bounds.
        d.selectRectangle(470, 70, 160, 160);
        QCOMPARE(d.selectedIds(), QStringList({rotated}));
        d.selectRectangle(50, 50, 350, 200, d.selectedIds());
        QCOMPARE(d.selectionCount(), 3);
        d.toggleSelection(rotated);
        QCOMPARE(d.selectedIds(), QStringList({first, second}));
        d.translateSelection(40, 20);
        QCOMPARE(d.objects()[0].toMap()["x"].toDouble(), 140.);
        QCOMPARE(d.objects()[1].toMap()["x"].toDouble(), 290.);
        QCOMPARE(d.objects()[2].toMap()["x"].toDouble(), 500.);
        d.undo();
        QCOMPARE(d.json(), before);
        QCOMPARE(d.selectedIds(), QStringList({first, second}));
        d.duplicateObject();
        QCOMPARE(d.objects().size(), 5);
        QCOMPARE(d.selectionCount(), 2);
        QVERIFY(!d.selectedIds().contains(first) && !d.selectedIds().contains(second));
        d.undo();
        QCOMPARE(d.json(), before);
        QVERIFY(d.copyObject());
        QVERIFY(d.pasteObjectAt(600, 300));
        QCOMPARE(d.objects().size(), 5);
        QCOMPARE(d.objects()[3].toMap()["x"].toDouble(), 600.);
        QCOMPARE(d.objects()[4].toMap()["x"].toDouble(), 750.);
        QCOMPARE(d.selectionCount(), 2);
        d.undo();
        QCOMPARE(d.json(), before);
        QCOMPARE(d.selectedIds(), QStringList({first, second}));
        d.cutObject();
        QCOMPARE(d.objects().size(), 1);
        d.undo();
        QCOMPARE(d.json(), before);
        QCOMPARE(d.selectionCount(), 2);
        d.alignObject("center");
        QCOMPARE(d.selectionBounds()["x"].toDouble(), 525.);
        QCOMPARE(d.objects()[1].toMap()["x"].toDouble() - d.objects()[0].toMap()["x"].toDouble(),
                 150.);
        d.undo();
        QCOMPARE(d.json(), before);
        d.selectAll();
        QCOMPARE(d.selectionCount(), 3);
        d.setSlideIndex(0);
        QCOMPARE(d.selectionCount(), 0);
        d.cleanupRecovery();
    }
    void groupsSelectionPersistenceAndUndo() {
        Document d;
        d.addSlide("blank");
        for (int i = 0; i < 3; ++i) {
            d.addObjectAt("rect", 100 + 200 * i, 100);
            d.updateObject(d.selectedId(), {{"w", 100}, {"h", 100}});
        }
        const auto first = d.objects()[0].toMap()["id"].toString();
        const auto second = d.objects()[1].toMap()["id"].toString();
        const auto third = d.objects()[2].toMap()["id"].toString();
        const auto old = d.json();
        d.selectMany({first, second});
        QVERIFY(d.canGroup());
        d.groupSelection();
        const auto grouped = d.json();
        QCOMPARE(grouped["version"].toInt(), 2);
        QVERIFY(d.groupedSelection() && d.canUngroup() && !d.canGroup());
        d.undo();
        QCOMPARE(d.json(), old);
        d.redo();
        QCOMPARE(d.json(), grouped);
        d.select(first);
        QCOMPARE(d.selectionCount(), 2);
        d.toggleSelection(second);
        QCOMPARE(d.selectionCount(), 0);
        d.selectRectangle(90, 90, 120, 120);
        QCOMPARE(d.selectionCount(), 0); // One enclosed member does not select half a group.
        d.selectRectangle(90, 90, 320, 120);
        QCOMPARE(d.selectionCount(), 2);
        d.toggleSelection(third);
        QCOMPARE(d.selectionCount(), 3);
        d.ungroupSelection();
        QVERIFY(!d.canUngroup());
        QCOMPARE(d.selectionCount(), 3);
        d.undo();
        QCOMPARE(d.json(), grouped);
        QTemporaryDir tmp;
        const auto path = QUrl::fromLocalFile(tmp.filePath("groups.flux"));
        QVERIFY(d.save(path));
        Document loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.json(), grouped);
        loaded.setSlideIndex(d.slideIndex());
        loaded.select(second);
        QCOMPARE(loaded.selectionCount(), 2);
        QFile v1(tmp.filePath("v1.flux"));
        QVERIFY(v1.open(QIODevice::WriteOnly));
        v1.write(QJsonDocument(old).toJson());
        v1.close();
        QVERIFY(loaded.open(QUrl::fromLocalFile(v1.fileName())));
        QCOMPARE(loaded.json(), old);
        d.cleanupRecovery();
        loaded.cleanupRecovery();
    }
    void nestedGroupsClipboardAndDuplication() {
        Document d;
        d.addSlide("blank");
        d.addObject("rect");
        const auto first = d.selectedId();
        d.addObject("ellipse");
        d.selectAll();
        d.groupSelection();
        const auto inner = d.objects()[0].toMap()["groups"].toList()[0];
        d.addObject("text");
        d.selectAll();
        d.groupSelection();
        const auto original = d.objects();
        QCOMPARE(original[0].toMap()["groups"].toList().size(), 2);
        QVERIFY(d.copyObject());
        const auto payload = QJsonDocument::fromJson(QGuiApplication::clipboard()->mimeData()->data(
                                                         "application/x-flux-object+json"))
                                 .object();
        QCOMPARE(payload["version"].toInt(), 2);
        const auto before = d.json();
        d.duplicateObject();
        QCOMPARE(d.objects().size(), 6);
        QCOMPARE(d.selectionCount(), 3);
        const auto copy = d.objects()[3].toMap();
        QVERIFY(copy["id"] != original[0].toMap()["id"]);
        QVERIFY(copy["groups"].toList()[0] != original[0].toMap()["groups"].toList()[0]);
        QVERIFY(copy["groups"].toList()[1] != inner);
        d.select(first);
        QCOMPARE(d.selectionCount(), 3);
        d.undo();
        QCOMPARE(d.json(), before);
        d.ungroupSelection();
        d.select(first);
        QCOMPARE(d.selectionCount(), 2);
        QCOMPARE(d.objects()[0].toMap()["groups"].toList()[0], inner);
        d.ungroupSelection();
        d.select(first);
        QCOMPARE(d.selectionCount(), 1);
        Document pasted;
        pasted.addSlide("blank");
        QCOMPARE(pasted.json()["version"].toInt(), 1);
        QVERIFY(pasted.pasteObject());
        QCOMPARE(pasted.json()["version"].toInt(), 2);
        QCOMPARE(pasted.selectionCount(), 3);
        QVERIFY(pasted.groupedSelection());
        pasted.ungroupSelection();
        pasted.select(pasted.objects()[0].toMap()["id"].toString());
        QCOMPARE(pasted.selectionCount(), 2);
        d.cleanupRecovery();
        pasted.cleanupRecovery();
    }
    void individualResizePreviewCommitAndUndo() {
        Document d;
        d.addSlide("blank");
        d.addObjectAt("text", 300, 200);
        const auto id = d.selectedId();
        d.updateObject(id, {{"w", 200}, {"h", 120}, {"rotation", 30}, {"fontSize", 24},
                            {"path", "bezier"}, {"c1x", 80}, {"c1y", 60}});
        const auto original = d.selected();
        const auto before = d.json();
        QSignalSpy changes(&d, &Document::documentChanged);
        const auto preview = d.previewObjectResize(id, -1, -1, -60, -40, true, false);
        QVERIFY(!preview.isEmpty());
        QCOMPARE(d.json(), before);
        QCOMPARE(changes.size(), 0);
        d.resizeObject(id, -1, -1, -60, -40, true, false);
        QCOMPARE(changes.size(), 1);
        for (auto it = preview.begin(); it != preview.end(); ++it)
            QCOMPARE(d.selected()[it.key()], it.value());
        for (const auto& key : original.keys())
            if (!preview.contains(key))
                QCOMPARE(d.selected()[key], original[key]);
        QCOMPARE(d.json()["version"].toInt(), 1);
        QVERIFY(Document::validate(d.json()));
        const auto after = d.json();
        d.undo();
        QCOMPARE(d.json(), before);
        QCOMPARE(d.selectedId(), id);
        d.redo();
        QCOMPARE(d.json(), after);
        // No-op, invalid, and stale selection requests must not add history.
        d.resizeObject(id, 1, 1, 0, 0, true, true);
        d.resizeObject(id, 0, 0, 30, 30, false, false);
        d.resizeObject(id, 1, 1, std::numeric_limits<double>::quiet_NaN(), 0, false, false);
        d.resizeObject("missing", 1, 1, 30, 30, false, false);
        QCOMPARE(d.json(), after);
        d.undo();
        QCOMPARE(d.json(), before);
        d.resizeObject(id, 0, -1, 0, -20, true, true);
        const auto centered = d.selected();
        QVERIFY(std::abs(centered["x"].toDouble() + centered["w"].toDouble() / 2 - 400) < 1e-8);
        QVERIFY(std::abs(centered["y"].toDouble() + centered["h"].toDouble() / 2 - 260) < 1e-8);
        QVERIFY(std::abs(centered["w"].toDouble() / centered["h"].toDouble() - 200. / 120.) < 1e-8);
        d.addObject("rect");
        d.selectAll();
        const auto multiple = d.json();
        QVERIFY(d.previewObjectResize(id, 1, 1, 30, 30, false, false).isEmpty());
        d.resizeObject(id, 1, 1, 30, 30, false, false);
        QCOMPARE(d.json(), multiple);
        d.groupSelection();
        const auto grouped = d.json();
        QVERIFY(d.previewObjectResize(id, 1, 1, 30, 30, false, false).isEmpty());
        d.resizeObject(id, 1, 1, 30, 30, false, false);
        QCOMPARE(d.json(), grouped);
        d.cleanupRecovery();
    }
    void collectiveResizeGeometryUndoAndMorph() {
        Document d;
        d.addSlide("blank");
        d.addObjectAt("rect", 100, 120);
        const auto rect = d.selectedId();
        d.updateObject(
            rect, {{"w", 80}, {"h", 60}, {"rotation", 30}, {"strokeWidth", 3}, {"radius", 12}});
        d.addObjectAt("text", 300, 160);
        const auto text = d.selectedId();
        d.updateObject(text, {{"w", 160},
                              {"h", 100},
                              {"fontSize", 24},
                              {"path", "bezier"},
                              {"c1x", 280},
                              {"c1y", 110},
                              {"c2x", 400},
                              {"c2y", 200}});
        d.addObject("ellipse");
        d.selectMany({rect, text});
        d.groupSelection();
        d.duplicateSlide();
        d.select(rect);
        const auto before = d.json();
        const auto nodes = d.objects();
        const auto bounds = d.selectionBounds();
        const double ax = bounds["x"].toDouble(), ay = bounds["y"].toDouble();
        QSignalSpy changes(&d, &Document::documentChanged);
        d.scaleSelection(1.5, ax, ay);
        QCOMPARE(changes.size(), 1);
        const auto scaled = d.objects();
        const auto after = d.json();
        for (int i = 0; i < 2; ++i) {
            const auto a = nodes[i].toMap(), b = scaled[i].toMap();
            QCOMPARE(b["id"], a["id"]);
            QCOMPARE(b["groups"], a["groups"]);
            QCOMPARE(b["w"].toDouble(), a["w"].toDouble() * 1.5);
            QCOMPARE(b["h"].toDouble(), a["h"].toDouble() * 1.5);
            QCOMPARE(b["x"].toDouble(), ax + (a["x"].toDouble() - ax) * 1.5);
            QCOMPARE(b["y"].toDouble(), ay + (a["y"].toDouble() - ay) * 1.5);
        }
        QCOMPARE(scaled[0].toMap()["rotation"].toDouble(), 30.);
        QCOMPARE(scaled[0].toMap()["strokeWidth"].toDouble(), 4.5);
        QCOMPARE(scaled[0].toMap()["radius"].toDouble(), 18.);
        QCOMPARE(scaled[1].toMap()["fontSize"].toDouble(), 36.);
        QCOMPARE(scaled[1].toMap()["c1x"].toDouble(), ax + (280 - ax) * 1.5);
        QCOMPARE(scaled[1].toMap()["c2y"].toDouble(), ay + (200 - ay) * 1.5);
        QCOMPARE(scaled[2], nodes[2]);
        const auto newBounds = d.selectionBounds();
        QVERIFY(std::abs(newBounds["x"].toDouble() - ax) < 1e-8);
        QVERIFY(std::abs(newBounds["w"].toDouble() - bounds["w"].toDouble() * 1.5) < 1e-8);
        const auto tracks = d.morphTracks(d.slideIndex());
        QCOMPARE(tracks.size(), 3);
        for (const auto& value : tracks) {
            const auto track = value.toMap();
            QCOMPARE(track["from"].toMap()["id"], track["to"].toMap()["id"]);
            if (track["to"].toMap()["id"] == text) {
                QCOMPARE(track["from"].toMap()["fontSize"].toDouble(), 24.);
                QCOMPARE(track["to"].toMap()["fontSize"].toDouble(), 36.);
            }
        }
        QVERIFY(Document::validate(after));
        d.undo();
        QCOMPARE(d.json(), before);
        d.redo();
        QCOMPARE(d.json(), after);
        d.scaleSelection(1, ax, ay);
        d.scaleSelection(-1, ax, ay);
        d.scaleSelection(std::numeric_limits<double>::quiet_NaN(), ax, ay);
        d.scaleSelection(2, std::numeric_limits<double>::infinity(), ay);
        QCOMPARE(d.json(), after);
        d.undo();
        QCOMPARE(d.json(), before); // Invalid/no-op resizes do not add history entries.
        d.cleanupRecovery();
    }
    void collectiveResizeLimitsAndUngroupedSelection() {
        Document d;
        d.addSlide("blank");
        d.addObjectAt("text", 100, 100);
        d.updateObject(d.selectedId(), {{"w", 100}, {"h", 100}, {"fontSize", 24}});
        d.addObjectAt("rect", 300, 100);
        d.updateObject(d.selectedId(), {{"w", 100}, {"h", 100}});
        d.selectAll();
        const auto before = d.json();
        QCOMPARE(d.constrainSelectionScale(100, 0, 0), 10.);
        d.scaleSelection(100, 0, 0);
        QCOMPARE(d.objects()[0].toMap()["fontSize"].toDouble(), 240.);
        QCOMPARE(d.objects()[1].toMap()["x"].toDouble(), 3000.);
        QCOMPARE(d.json()["version"].toInt(), 1);
        d.undo();
        QCOMPARE(d.json(), before);
        d.scaleSelection(.01, 0, 0);
        QCOMPARE(d.objects()[0].toMap()["fontSize"].toDouble(), 8.);
        QCOMPARE(d.objects()[1].toMap()["x"].toDouble(), 100.);
        QVERIFY(Document::validate(d.json()));
        d.cleanupRecovery();
    }
    void rejectInvalidGroupHierarchies() {
        Document d;
        d.addSlide("blank");
        d.addObject("rect");
        d.addObject("rect");
        d.selectAll();
        d.groupSelection();
        const auto good = d.json();
        auto withGroups = [&](QJsonValue first, QJsonValue second, int version = 2) {
            auto doc = good;
            auto slides = doc["slides"].toArray();
            auto page = slides.last().toObject();
            auto nodes = page["objects"].toArray();
            auto a = nodes[0].toObject(), b = nodes[1].toObject();
            a["groups"] = first;
            b["groups"] = second;
            nodes[0] = a;
            nodes[1] = b;
            page["objects"] = nodes;
            slides[slides.size() - 1] = page;
            doc["slides"] = slides;
            doc["version"] = version;
            return doc;
        };
        QVERIFY(Document::validate(good));
        QVERIFY(!Document::validate(withGroups("bad", QJsonArray{})));
        QVERIFY(!Document::validate(withGroups(QJsonArray{"a"}, QJsonArray{"a"}, 1)));
        QVERIFY(!Document::validate(withGroups(QJsonArray{"a"}, QJsonArray{})));
        QVERIFY(!Document::validate(withGroups(QJsonArray{"a", "a"}, QJsonArray{"a", "a"})));
        QVERIFY(!Document::validate(withGroups(QJsonArray{"a", "b"}, QJsonArray{"b"})));
        QJsonArray deep;
        for (int i = 0; i < 16; ++i)
            deep.append(QString::number(i));
        QVERIFY(Document::validate(withGroups(deep, deep)));
        deep.append("too deep");
        QVERIFY(!Document::validate(withGroups(deep, deep)));
        d.cleanupRecovery();
    }
    void batchPasteCapacityIsAtomic() {
        Document d;
        d.addSlide("blank");
        d.addObject("rect");
        d.addObject("ellipse");
        d.selectAll();
        QVERIFY(d.copyObject());
        for (int i = 2; i < 249; ++i)
            d.addObject("rect");
        const auto before = d.json();
        QVERIFY(!d.pasteObject());
        QCOMPARE(d.json(), before);
        QCOMPARE(d.objects().size(), 249);
        d.cleanupRecovery();
    }
};
QTEST_MAIN(DocumentTest)
#include "document_test.moc"
