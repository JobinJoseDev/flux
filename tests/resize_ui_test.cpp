#include "Document.hpp"
#include "MotionPose.hpp"
#include "Render.hpp"
#include "TextMorph.hpp"
#include <QGuiApplication>
#include <QJsonObject>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <array>
#include <cmath>
#include <memory>

namespace {
QQuickItem* findItem(QQuickItem* root, const QString& name) {
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (auto* child : root->childItems())
        if (auto* found = findItem(child, name))
            return found;
    return nullptr;
}
constexpr std::array<int, 8> Horizontal{-1, 0, 1, 1, 1, 0, -1, -1};
constexpr std::array<int, 8> Vertical{-1, -1, -1, 0, 1, 1, 1, 0};
}

// Drive the actual EditCanvas MouseAreas. The geometric assertions use rendered
// item coordinates, independently of the C++ resize implementation.
class ResizeUiTest : public QObject {
    Q_OBJECT
    std::unique_ptr<Document> deck;
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;
    QQuickItem* editor = nullptr;
    QStringList warnings;
    QString id;

    QQuickItem* object() const { return findItem(editor, "object-" + id); }
    QQuickItem* handle(int index) const {
        return findItem(object(), "objectResize-" + QString::number(index));
    }
    void pointer(QEvent::Type type, const QPointF& p, Qt::MouseButton button,
                 Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QMouseEvent event(type, p, window->mapToGlobal(p.toPoint()), button, buttons, modifiers);
        QCoreApplication::sendEvent(window.get(), &event);
        QCoreApplication::processEvents();
    }
    void press(const QPointF& p, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        pointer(QEvent::MouseButtonPress, p, Qt::LeftButton, Qt::LeftButton, modifiers);
    }
    void move(const QPointF& p, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        pointer(QEvent::MouseMove, p, Qt::NoButton, Qt::LeftButton, modifiers);
    }
    void release(const QPointF& p, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        pointer(QEvent::MouseButtonRelease, p, Qt::LeftButton, Qt::NoButton, modifiers);
    }
    QPointF anchor(QQuickItem* item, int sx, int sy, bool center) const {
        return item->mapToScene({item->width() * (center ? .5 : (1 - sx) * .5),
                                 item->height() * (center ? .5 : (1 - sy) * .5)});
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
        deck->addObject("rect");
        id = deck->selectedId();
        deck->updateObject(id, {{"x", 400}, {"y", 260}, {"w", 240}, {"h", 160}});
        engine = std::make_unique<QQmlEngine>();
        connect(engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError>& errors) {
            for (const auto& error : errors)
                warnings.append(error.toString());
        });
        engine->rootContext()->setContextProperty("deck", deck.get());
        QQmlComponent component(engine.get(), QUrl::fromLocalFile(
            QStringLiteral(FLUX_SOURCE_DIR "/qml/EditCanvas.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        editor = qobject_cast<QQuickItem*>(component.createWithInitialProperties(
            {{"width", 1352}, {"height", 784}, {"snap", false}}));
        QVERIFY2(editor, qPrintable(component.errorString()));
        window = std::make_unique<QQuickWindow>();
        window->resize(1352, 784);
        editor->setParentItem(window->contentItem());
        editor->setParent(window->contentItem());
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
        QCoreApplication::processEvents();
        QVERIFY(object());
    }
    void cleanup() {
        if (window && window->mouseGrabberItem())
            window->mouseGrabberItem()->ungrabMouse();
        window.reset();
        editor = nullptr;
        engine.reset();
        deck->cleanupRecovery();
        deck.reset();
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void eightHandles_data() {
        QTest::addColumn<int>("index");
        QTest::addColumn<double>("angle");
        QTest::addColumn<int>("modifiers");
        for (int index = 0; index < 8; ++index)
            for (double angle : {0., 37., 90.})
                for (int modifiers : {0, int(Qt::ShiftModifier), int(Qt::ControlModifier),
                                      int(Qt::ShiftModifier | Qt::ControlModifier)}) {
                    const auto name = QString("handle%1-angle%2-modifiers%3")
                                          .arg(index).arg(angle).arg(modifiers).toUtf8();
                    QTest::newRow(name.constData()) << index << angle << modifiers;
                }
    }
    void eightHandles() {
        QFETCH(int, index);
        QFETCH(double, angle);
        QFETCH(int, modifiers);
        const auto keys = Qt::KeyboardModifiers(modifiers);
        const bool center = keys.testFlag(Qt::ControlModifier);
        const bool locked = keys.testFlag(Qt::ShiftModifier);
        const int sx = Horizontal[index], sy = Vertical[index];
        deck->updateObject(id, {{"rotation", angle}});
        QCoreApplication::processEvents();
        for (int i = 0; i < 8; ++i) {
            QVERIFY(handle(i));
            QVERIFY(handle(i)->isVisible());
        }
        auto* item = object();
        auto* grip = handle(index);
        const auto start = grip->mapToScene({grip->width() / 2, grip->height() / 2});
        const auto fixed = anchor(item, sx, sy, center);
        const QPointF localDelta(sx ? sx * 40. : 19., sy ? sy * 24. : -17.);
        const auto delta = item->mapToScene(localDelta) - item->mapToScene({0, 0});
        const auto before = deck->json();
        QSignalSpy changed(deck.get(), &Document::documentChanged);
        QSignalSpy history(deck.get(), &Document::historyChanged);
        press(start, keys);
        for (double fraction : {.25, .5, 1.}) {
            move(start + delta * fraction, keys);
            QVERIFY(editor->property("objectResizing").toBool());
            QCOMPARE(deck->json(), before);
            QVERIFY(QLineF(fixed, anchor(object(), sx, sy, center)).length() < .001);
        }
        QCOMPARE(changed.size(), 0);
        QCOMPARE(history.size(), 0);
        const auto previewSize = QSizeF(object()->width(), object()->height());
        QVERIFY(previewSize.width() > 240 || previewSize.height() > 160);
        if (locked)
            QVERIFY(std::abs(previewSize.width() / previewSize.height() - 1.5) < .000001);
        else {
            QVERIFY(std::abs(previewSize.width() - (240. + (sx ? 40. * (center ? 2 : 1) : 0.))) < .001);
            QVERIFY(std::abs(previewSize.height() - (160. + (sy ? 24. * (center ? 2 : 1) : 0.))) < .001);
        }
        release(start + delta, keys);
        QVERIFY(!editor->property("objectResizing").toBool());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(history.size(), 1);
        QCOMPARE(deck->selected()["w"].toDouble(), previewSize.width());
        QCOMPARE(deck->selected()["h"].toDouble(), previewSize.height());
        QCOMPARE(deck->selected()["rotation"].toDouble(), angle);
        QVERIFY(QLineF(fixed, anchor(object(), sx, sy, center)).length() < .001);
        const auto after = deck->json();
        QVERIFY(after != before);
        deck->undo();
        QCOMPARE(deck->json(), before);
        QCOMPARE(deck->selectedId(), id);
        deck->redo();
        QCOMPARE(deck->json(), after);
    }
    void cancel_data() {
        QTest::addColumn<QString>("cause");
        QTest::newRow("escape") << QString("escape");
        QTest::newRow("lost-grab") << QString("lost-grab");
        QTest::newRow("selection-changed") << QString("selection-changed");
        QTest::newRow("document-changed") << QString("document-changed");
    }
    void cancel() {
        QFETCH(QString, cause);
        deck->updateObject(id, {{"rotation", 37}});
        QCoreApplication::processEvents();
        auto* grip = handle(0);
        QVERIFY(grip);
        const auto start = grip->mapToScene({grip->width() / 2, grip->height() / 2});
        const auto end = start - QPointF(45, 25);
        const auto before = deck->json();
        const auto geometry = deck->selected();
        press(start);
        move(end);
        QVERIFY(editor->property("objectResizing").toBool());
        QCOMPARE(deck->json(), before);
        if (cause == "escape")
            QTest::keyClick(window.get(), Qt::Key_Escape);
        else if (cause == "lost-grab") {
            QVERIFY(window->mouseGrabberItem());
            window->mouseGrabberItem()->ungrabMouse();
        } else if (cause == "selection-changed")
            deck->select("");
        else
            deck->setTitle("An independent edit during a resize");
        QCoreApplication::processEvents();
        QVERIFY(!editor->property("objectResizing").toBool());
        const auto canceled = deck->json();
        move(end - QPointF(20, 20));
        release(end - QPointF(20, 20));
        QCOMPARE(deck->json(), canceled);
        if (cause != "document-changed")
            QCOMPARE(deck->json(), before);
        if (cause != "selection-changed") {
            QCOMPARE(deck->selectedId(), id);
            QCOMPARE(deck->selected(), geometry);
        }
        QVERIFY(object());
        QCOMPARE(object()->width(), 240.);
        QCOMPARE(object()->height(), 160.);
    }
    void noDragDoesNotEdit() {
        auto* grip = handle(4);
        QVERIFY(grip);
        const auto start = grip->mapToScene({grip->width() / 2, grip->height() / 2});
        const auto before = deck->json();
        QSignalSpy history(deck.get(), &Document::historyChanged);
        press(start);
        release(start);
        QCOMPARE(deck->json(), before);
        QCOMPARE(history.size(), 0);
        QVERIFY(!editor->property("objectResizing").toBool());
    }
    void groupResizeIsPreserved() {
        deck->addObjectAt("ellipse", 720, 240);
        deck->selectMany({id, deck->selectedId()});
        deck->groupSelection();
        QCoreApplication::processEvents();
        QVERIFY(deck->groupedSelection());
        for (int i = 0; i < 8; ++i)
            QVERIFY(!handle(i) || !handle(i)->isVisible());
        auto* grip = findItem(editor, "selectionResize-3");
        auto* paper = findItem(editor, "slidePaper");
        QVERIFY(grip && grip->isVisible() && paper);
        const auto bounds = deck->selectionBounds();
        const auto before = deck->json();
        const auto start = grip->mapToScene({grip->width() / 2, grip->height() / 2});
        const QPointF localDelta(bounds["w"].toDouble() * .2, bounds["h"].toDouble() * .2);
        const auto end = start + paper->mapToScene(localDelta) - paper->mapToScene({0, 0});
        press(start);
        move(end);
        QVERIFY(editor->property("selectionResizing").toBool());
        QVERIFY(!editor->property("objectResizing").toBool());
        QCOMPARE(deck->json(), before);
        release(end);
        const auto resized = deck->selectionBounds();
        QVERIFY(std::abs(resized["w"].toDouble() - bounds["w"].toDouble() * 1.2) < .001);
        QVERIFY(std::abs(resized["h"].toDouble() - bounds["h"].toDouble() * 1.2) < .001);
        QCOMPARE(resized["x"], bounds["x"]);
        QCOMPARE(resized["y"], bounds["y"]);
        QVERIFY(deck->groupedSelection());
        deck->undo();
        QCOMPARE(deck->json(), before);
    }
};

int main(int argc, char** argv) {
    QTemporaryDir data;
    if (!data.isValid())
        return 2;
    qputenv("FLUX_DATA_DIR", data.path().toUtf8());
    QGuiApplication app(argc, argv);
    ResizeUiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "resize_ui_test.moc"
