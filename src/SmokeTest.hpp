#pragma once
#include "Document.hpp"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <memory>

inline QQuickItem* visualItem(QQuickItem* root, const QString& name) {
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (auto child : root->childItems())
        if (auto found = visualItem(child, name))
            return found;
    return nullptr;
}
inline void sendPointer(QQuickWindow* window, QEvent::Type type, QPointF point,
                        Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent event(type, point, window->mapToGlobal(point.toPoint()), button, buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(window, &event);
}
inline void clickItem(QQuickItem* item, Qt::MouseButton button = Qt::LeftButton) {
    if (!item || !item->window())
        return;
    const auto point = item->mapToScene({item->width() / 2, item->height() / 2});
    sendPointer(item->window(), QEvent::MouseButtonPress, point, button, button);
    sendPointer(item->window(), QEvent::MouseButtonRelease, point, button, Qt::NoButton);
}
inline void sendKey(QQuickWindow* window, int key,
                    Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(window, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QCoreApplication::sendEvent(window, &release);
}

// A bounded integration probe for CI. It drives the real Qt Quick application.
inline void runSmokeTest(QQmlApplicationEngine& engine, Document& deck, int& qmlWarnings) {
    auto failures = std::make_shared<QStringList>();
    auto root = engine.rootObjects().first();
    auto check = [failures](bool ok, const QString& message) {
        if (!ok)
            failures->append(message);
    };
    QTimer::singleShot(100, root, [root, &deck, check] {
        deck.setSlideIndex(1);
        for (const auto& value : deck.objects()) {
            auto node = value.toMap();
            if (node.value("kind") == "ellipse") {
                deck.select(node.value("id").toString());
                break;
            }
        }
        check(!deck.selectedId().isEmpty(), "Select object on duplicated slide");
        if (auto inspector = root->findChild<QObject*>("inspector"))
            inspector->setProperty("tab", 1);
    });
    QTimer::singleShot(250, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto editor = window ? visualItem(window->contentItem(), "editCanvas") : nullptr;
        auto item = visualItem(editor, "object-" + deck.selectedId());
        check(item && window, "Find editable object in actual canvas");
        if (item && window) {
            const double before = deck.selected().value("x").toDouble();
            QPointF start = item->mapToScene({item->width() / 2, item->height() / 2});
            auto send = [window](QEvent::Type type, QPointF point, Qt::MouseButton button,
                                 Qt::MouseButtons buttons) {
                QMouseEvent event(type, point, window->mapToGlobal(point.toPoint()), button,
                                  buttons, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &event);
            };
            send(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
            send(QEvent::MouseMove, start + QPointF(45, 20), Qt::NoButton, Qt::LeftButton);
            send(QEvent::MouseButtonRelease, start + QPointF(45, 20), Qt::LeftButton, Qt::NoButton);
            check(deck.selected().value("x").toDouble() != before,
                  "Drag commits geometry through the canvas");
            deck.undo();
        }
        auto slider = window ? visualItem(window->contentItem(), "motionScrubber") : nullptr;
        check(slider != nullptr, "Find timeline slider");
        if (slider && window) {
            auto point = slider->mapToScene({slider->width() * .65, slider->height() / 2});
            QMouseEvent press(QEvent::MouseButtonPress, point, window->mapToGlobal(point.toPoint()),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &press);
            QMouseEvent release(QEvent::MouseButtonRelease, point,
                                window->mapToGlobal(point.toPoint()), Qt::LeftButton, Qt::NoButton,
                                Qt::NoModifier);
            QCoreApplication::sendEvent(window, &release);
        }
        check(root->property("previewing").toBool(), "Scrubbing opens Morph preview");
        check(root->property("previewProgress").toDouble() > .35,
              "First scrub keeps requested progress");
    });
    QTimer::singleShot(500, root, [root, &deck, check] {
        QMetaObject::invokeMethod(root, "stopPreview");
        deck.addObject("code");
        QMetaObject::invokeMethod(root, "editObject");
        auto input = root->findChild<QObject*>("contentEditor");
        auto dialog = root->findChild<QObject*>("contentDialog");
        check(input && dialog, "Content editor opens");
        if (input && dialog) {
            input->setProperty("text", "const auto motion = 42;\n// Edited through the UI");
            QMetaObject::invokeMethod(dialog, "accept");
            check(deck.selected().value("text").toString().contains("motion = 42"),
                  "Content editor commits changes");
            deck.undo();
            deck.undo(); // Return to the original composition before testing context menus.
        }
    });
    QTimer::singleShot(750, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        QVariantMap target;
        for (const auto& value : deck.objects())
            if (value.toMap().value("kind") == "ellipse") {
                target = value.toMap();
                break;
            }
        deck.select(deck.objects().first().toMap()["id"].toString());
        auto item = window ? visualItem(window->contentItem(), "object-" + target["id"].toString())
                           : nullptr;
        check(item != nullptr, "Find a different object for right-click selection");
        if (item) {
            const auto point = item->mapToScene({item->width() / 2, item->height() / 2});
            sendPointer(window, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
            sendPointer(window, QEvent::MouseMove, point + QPointF(3, 2), Qt::NoButton,
                        Qt::RightButton);
            sendPointer(window, QEvent::MouseButtonRelease, point + QPointF(3, 2), Qt::RightButton,
                        Qt::NoButton);
            check(deck.selectedId() == target["id"].toString(), "Right-click selects its target");
            check(deck.selected()["x"] == target["x"] && deck.selected()["y"] == target["y"],
                  "Right-click motion does not drag the object");
        }
        auto menu = root->findChild<QObject*>("objectContextMenu");
        check(menu && menu->property("visible").toBool(), "Object context menu opens");
    });
    QTimer::singleShot(900, root, [root, check] {
        const auto path = qEnvironmentVariable("FLUX_MENU_SCREENSHOT");
        if (!path.isEmpty()) {
            auto item = root->findChild<QQuickItem*>("contextCopyObject");
            check(item && item->window() && item->window()->grabWindow().save(path),
                  "Save context menu screenshot");
        }
    });
    QTimer::singleShot(1000, root, [root, &deck, check] {
        auto item = root->findChild<QQuickItem*>("contextCopyObject");
        check(item && item->isVisible(), "Find Copy menu item");
        clickItem(item);
        check(deck.canPaste(), "Copy menu action writes the clipboard");
    });
    QTimer::singleShot(1200, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto paper = window ? visualItem(window->contentItem(), "slidePaper") : nullptr;
        check(paper != nullptr, "Find canvas for blank-space context menu");
        if (paper) {
            auto point = paper->mapToScene({24, 680});
            sendPointer(window, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
            sendPointer(window, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
        }
        check(deck.selectedId().isEmpty(), "Canvas context menu clears object selection");
        auto menu = root->findChild<QObject*>("canvasContextMenu");
        check(menu && menu->property("visible").toBool(), "Canvas context menu opens");
    });
    QTimer::singleShot(1400, root, [root, &deck, check] {
        const auto before = deck.json();
        auto item = root->findChild<QQuickItem*>("contextPasteHere");
        check(item && item->isVisible(), "Find Paste here menu item");
        clickItem(item);
        check(deck.json() != before && deck.selected()["kind"] == "ellipse",
              "Paste here inserts copied object");
        check(deck.selected()["x"].toDouble() == 24., "Paste uses the clicked slide position");
        deck.undo();
        check(deck.json() == before, "One undo removes the pasted object");
    });
    QTimer::singleShot(1600, root, [root, &deck, check] {
        deck.setSlideIndex(0);
        auto window = qobject_cast<QQuickWindow*>(root);
        auto item = window ? visualItem(window->contentItem(), "slideThumbnail-1") : nullptr;
        check(item != nullptr, "Find second slide thumbnail");
        clickItem(item, Qt::RightButton);
        check(deck.slideIndex() == 1, "Right-click selects the intended slide");
        auto menu = root->findChild<QObject*>("slideContextMenu");
        check(menu && menu->property("visible").toBool(), "Slide context menu opens");
    });
    QTimer::singleShot(1800, root, [root, &deck, check] {
        const auto before = deck.json();
        const auto objects = deck.objects();
        auto item = root->findChild<QQuickItem*>("contextDuplicateSlide");
        check(item && item->isVisible(), "Find Duplicate slide menu item");
        clickItem(item);
        check(deck.slideIndex() == 2 && deck.objects() == objects,
              "Slide menu duplicate preserves Morph identities");
        deck.undo();
        check(deck.json() == before && deck.slideIndex() == 1, "Undo restores slide order");
    });
    QTimer::singleShot(2000, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto editor = window ? visualItem(window->contentItem(), "editCanvas") : nullptr;
        for (const auto& value : deck.objects())
            if (value.toMap().value("kind") == "ellipse") {
                deck.select(value.toMap().value("id").toString());
                break;
            }
        if (editor)
            editor->forceActiveFocus();
        if (window)
            sendKey(window, Qt::Key_F10, Qt::ShiftModifier);
        auto menu = root->findChild<QObject*>("objectContextMenu");
        check(menu && menu->property("visible").toBool(), "Shift+F10 opens the context menu");
    });
    QTimer::singleShot(2200, root, [root, check] {
        auto item = root->findChild<QQuickItem*>("contextCopyObject");
        if (item && item->window())
            sendKey(item->window(), Qt::Key_Escape);
        auto menu = root->findChild<QObject*>("objectContextMenu");
        check(menu && !menu->property("visible").toBool(), "Escape dismisses the context menu");
    });
    QTimer::singleShot(2400, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        const double before = deck.selected()["x"].toDouble();
        if (window)
            sendKey(window, Qt::Key_Right);
        check(deck.selected()["x"].toDouble() == before + 1.,
              "Keyboard editing resumes after menu dismissal");
        deck.undo();
    });
    QTimer::singleShot(2600, root, [root, &deck, check] {
        QMetaObject::invokeMethod(root, "editNotes");
        auto input = root->findChild<QObject*>("notesEditor");
        auto dialog = root->findChild<QObject*>("notesDialog");
        if (input && dialog) {
            input->setProperty("text", "Notes entered through the real dialog.");
            QMetaObject::invokeMethod(dialog, "accept");
            check(deck.currentSlide().value("notes").toString().startsWith("Notes entered"),
                  "Presenter notes commit");
        } else
            check(false, "Find notes dialog");
        QMetaObject::invokeMethod(root, "present", Q_ARG(QVariant, true));
        auto audience = root->findChild<QObject*>("audience");
        check(audience && audience->property("visible").toBool(), "Audience window opens");
        if (audience)
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, 1));
    });
    QTimer::singleShot(4100, root, [root, &deck, &qmlWarnings, failures, check] {
        auto audience = root->findChild<QObject*>("audience");
        check(audience && audience->property("current").toInt() == 2,
              "Presentation advances slides");
        if (audience)
            QMetaObject::invokeMethod(audience, "end");
        check(qmlWarnings == 0, "No QML runtime warnings");
        deck.cleanupRecovery();
        if (failures->isEmpty())
            qInfo("Flux UI smoke passed: context menus, clipboard, keyboard focus, drag, Morph "
                  "scrub, content, notes, presentation.");
        else
            for (const auto& failure : *failures)
                qCritical().noquote() << failure;
        QCoreApplication::exit(failures->isEmpty() ? 0 : 3);
    });
}
