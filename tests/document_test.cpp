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
