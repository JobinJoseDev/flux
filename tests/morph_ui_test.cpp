#include "Document.hpp"
#include "MotionPose.hpp"
#include "Render.hpp"
#include "TextMorph.hpp"
#include <QGuiApplication>
#include <QDir>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>
#include <memory>

namespace {
template <typename T> QList<T*> items(QQuickItem* root) {
    QList<T*> result;
    if (auto* match = qobject_cast<T*>(root))
        result.append(match);
    for (auto* child : root->childItems())
        result.append(items<T>(child));
    return result;
}
template <typename T> QList<T*> visibleItems(QQuickItem* root) {
    auto result = items<T>(root);
    result.removeIf([](const T* item) { return !item->isVisible(); });
    return result;
}
}

// These checks exercise the shipped QML components and their native painted
// items together, including the transition compiler and scene-graph clipping.
class MorphUiTest : public QObject {
    Q_OBJECT
    std::unique_ptr<Document> deck;
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;
    QQuickItem* stage = nullptr;
    QQuickItem* slide = nullptr;
    QStringList warnings;
    QVariantMap fromSlide, toSlide, fromNode, toNode;

    QQuickItem* component(const QString& file) {
        QQmlComponent qml(engine.get(), QUrl::fromLocalFile(
            QStringLiteral(FLUX_SOURCE_DIR "/qml/") + file));
        if (!qml.isReady()) {
            warnings.append(qml.errorString());
            return nullptr;
        }
        auto* item = qobject_cast<QQuickItem*>(qml.create());
        if (!item) {
            warnings.append(qml.errorString());
            return nullptr;
        }
        item->setParentItem(window->contentItem());
        item->setParent(window->contentItem());
        item->setVisible(false);
        return item;
    }
    void prepare(const QString& text, const QString& alignment = "right", bool clipped = false) {
        deck->addObject("text");
        const auto id = deck->selectedId();
        deck->updateObject(id, {{"text", text}, {"x", 120}, {"y", 90}, {"w", 470},
                               {"h", clipped ? 29 : 132}, {"fontSize", 29},
                               {"align", "left"}, {"color", "#eddfc8"}, {"opacity", .8},
                               {"rotation", clipped ? 0 : -9}, {"easing", "out"}});
        fromNode = deck->selected();
        fromSlide = deck->currentSlide();
        deck->duplicateSlide();
        deck->updateObject(id, {{"x", 525}, {"y", 230}, {"w", 310},
                               {"h", clipped ? 35 : 110}, {"fontSize", 43},
                               {"align", alignment}, {"color", "#98e5df"}, {"opacity", .9},
                               {"rotation", clipped ? 0 : 17}});
        toNode = deck->selected();
        toSlide = deck->currentSlide();
        stage->setProperty("tracks", deck->morphTracks(deck->slideIndex()));
        stage->setProperty("fromBackground", fromSlide["background"]);
        stage->setProperty("toBackground", toSlide["background"]);
    }
    QImage capture(QQuickItem* showing) {
        slide->setVisible(showing == slide);
        stage->setVisible(showing == stage);
        window->update();
        // Allow polish/paint events to drain before the synchronous framebuffer grab.
        QTest::qWait(25);
        return window->grabWindow();
    }
    QImage at(double progress) {
        stage->setProperty("progress", progress);
        return capture(stage);
    }
    QImage still(const QVariantMap& data) {
        slide->setProperty("slide", data);
        return capture(slide);
    }

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        qmlRegisterType<MotionPose>("Flux.Native", 1, 0, "MotionPose");
        qmlRegisterType<ContentItem>("Flux.Native", 1, 0, "ContentItem");
        qmlRegisterType<TextItem>("Flux.Native", 1, 0, "TextItem");
        qmlRegisterType<TextMorphItem>("Flux.Native", 1, 0, "TextMorphItem");
    }
    void init() {
        warnings.clear();
        deck = std::make_unique<Document>();
        deck->newDeck(false);
        deck->addSlide("blank");
        engine = std::make_unique<QQmlEngine>();
        connect(engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError>& errors) {
            for (const auto& error : errors)
                warnings.append(error.toString());
        });
        engine->rootContext()->setContextProperty("deck", deck.get());
        window = std::make_unique<QQuickWindow>();
        window->resize(1280, 720);
        slide = component("SlideView.qml");
        stage = component("MotionStage.qml");
        QVERIFY2(slide && stage, qPrintable(warnings.join('\n')));
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    }
    void cleanup() {
        window.reset();
        slide = stage = nullptr;
        engine.reset();
        deck->cleanupRecovery();
        deck.reset();
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void endpointsAndReversibleSeeking_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("alignment");
        QTest::newRow("latin-right")
            << QString("Office affinity: keep these words together as the text box changes.\n"
                       "The same words take a different path.")
            << QString("right");
        QTest::newRow("unicode-center")
            << QString::fromUtf8("Café a\u0301 — Ελληνικά العربية हिन्दी 👩‍💻\n"
                                 "Unicode remains intact as these words wrap.")
            << QString("center");
    }
    void endpointsAndReversibleSeeking() {
        QFETCH(QString, text);
        QFETCH(QString, alignment);
        prepare(text, alignment);
        const auto tracks = deck->morphTracks(deck->slideIndex());
        QCOMPARE(tracks.size(), 1);
        QVERIFY(tracks.first().toMap()["spatialText"].toBool());
        const auto staticFrom = still(fromSlide);
        const auto staticTo = still(toSlide);
        QVERIFY(!staticFrom.isNull());
        QVERIFY(!staticTo.isNull());
        QVERIFY(staticFrom != staticTo);
        QCOMPARE(at(0), staticFrom);
        QCOMPARE(visibleItems<TextMorphItem>(stage).size(), 1);
        QVERIFY(visibleItems<TextItem>(stage).isEmpty());
        QCOMPARE(at(1), staticTo);
        const auto forward = at(.36);
        QVERIFY(forward != staticFrom);
        QVERIFY(forward != staticTo);
        const auto captures = qEnvironmentVariable("FLUX_MORPH_CAPTURE_DIR");
        if (!captures.isEmpty()) {
            QVERIFY(QDir().mkpath(captures));
            const auto prefix = captures + "/" + QString::fromLatin1(QTest::currentDataTag());
            QVERIFY(staticFrom.save(prefix + "-from.png"));
            QVERIFY(forward.save(prefix + "-middle.png"));
            QVERIFY(staticTo.save(prefix + "-to.png"));
        }
        at(.81);
        QCOMPARE(at(.36), forward);
        at(.12);
        QCOMPARE(at(.36), forward);
        // Returning to either endpoint after arbitrary direction changes retains
        // exactly the final static rendering, not a stale layout or texture.
        QCOMPARE(at(1), staticTo);
        QCOMPARE(at(0), staticFrom);
    }
    void fontAndLayoutUsePoseEasing() {
        prepare("The same text changes size and wraps across lines with cubic-out easing.");
        at(.25);
        const auto morphs = visibleItems<TextMorphItem>(stage);
        QCOMPARE(morphs.size(), 1);
        auto* morph = morphs.first();
        const double eased = 1. - std::pow(1. - .25, 3.);
        QVERIFY(std::abs(morph->progress() - eased) < 1e-12);
        auto* object = morph->parentItem();
        QVERIFY(object);
        auto interpolate = [eased](double a, double b) { return a + (b - a) * eased; };
        QVERIFY(std::abs(object->x() - interpolate(120, 525)) < 1e-9);
        QVERIFY(std::abs(object->y() - interpolate(90, 230)) < 1e-9);
        QVERIFY(std::abs(object->width() - interpolate(470, 310)) < 1e-9);
        QVERIFY(std::abs(object->height() - interpolate(132, 110)) < 1e-9);
        // On the reverse route the original asymmetric easing is evaluated at
        // the same scalar, rather than eased again with swapped endpoints.
        at(.9);
        at(.25);
        QVERIFY(std::abs(morph->progress() - eased) < 1e-12);
    }
    void clippingMatchesStaticText() {
        prepare("Line one must be clipped.\nLine two must not escape the box.\nLine three.",
                "center", true);
        const auto staticFrom = still(fromSlide);
        const auto staticTo = still(toSlide);
        QCOMPARE(at(0), staticFrom);
        QCOMPARE(at(1), staticTo);
        const auto middle = at(.5);
        const auto morphs = visibleItems<TextMorphItem>(stage);
        QCOMPARE(morphs.size(), 1);
        auto* object = morphs.first()->parentItem();
        const auto box = object->mapRectToScene(QRectF(0, 0, object->width(), object->height()));
        const double sx = double(middle.width()) / window->width();
        const double sy = double(middle.height()) / window->height();
        const auto pixels = QRectF(box.x() * sx, box.y() * sy,
                                  box.width() * sx, box.height() * sy).toAlignedRect();
        const auto background = middle.pixel(0, 0);
        int ink = 0;
        for (int y = 0; y < middle.height(); ++y)
            for (int x = 0; x < middle.width(); ++x)
                if (middle.pixel(x, y) != background) {
                    ++ink;
                    QVERIFY2(pixels.contains(x, y), "Text ink escaped its interpolated clip rectangle");
                }
        QVERIFY(ink > 20);
    }
    void changedTextRetainsCrossfade() {
        prepare("Original words remain the source of this crossfade.");
        deck->updateObject(deck->selectedId(), {{"text", "Different words are the target."}});
        toSlide = deck->currentSlide();
        const auto tracks = deck->morphTracks(deck->slideIndex());
        QCOMPARE(tracks.size(), 1);
        QVERIFY(!tracks.first().toMap()["spatialText"].toBool());
        QVERIFY(tracks.first().toMap()["crossfade"].toBool());
        stage->setProperty("tracks", tracks);
        const auto start = at(0);
        const auto end = at(1);
        const auto middle = at(.4);
        QVERIFY(start != end);
        QVERIFY(middle != start && middle != end);
        QVERIFY(visibleItems<TextMorphItem>(stage).isEmpty());
        QCOMPARE(visibleItems<TextItem>(stage).size(), 2);
        QCOMPARE(end, still(toSlide));
    }
    void entranceBuildsUseStaticText() {
        prepare("Entrance text continues to use its existing animation.");
        deck->updateObject(deck->selectedId(), {{"animation", "rise"}, {"delay", .2}, {"duration", .8}});
        toSlide = deck->currentSlide();
        stage->setProperty("tracks", deck->entranceTracks(deck->slideIndex()));
        stage->setProperty("builds", true);
        stage->setProperty("totalDuration", deck->buildDuration(deck->slideIndex()));
        const auto start = at(0);
        const auto middle = at(.6);
        const auto end = at(1);
        QVERIFY(start != middle && middle != end);
        QVERIFY(visibleItems<TextMorphItem>(stage).isEmpty());
        QCOMPARE(visibleItems<TextItem>(stage).size(), 1);
        QCOMPARE(end, still(toSlide));
    }
};

int main(int argc, char** argv) {
    QTemporaryDir data;
    if (!data.isValid())
        return 2;
    qputenv("FLUX_DATA_DIR", data.path().toUtf8());
    QGuiApplication app(argc, argv);
    MorphUiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "morph_ui_test.moc"
