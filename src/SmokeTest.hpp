#pragma once
#include "Document.hpp"
#include <QCoreApplication>
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
        }
    });
    QTimer::singleShot(800, root, [root, &deck, check] {
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
    QTimer::singleShot(2200, root, [root, &deck, &qmlWarnings, failures, check] {
        auto audience = root->findChild<QObject*>("audience");
        check(audience && audience->property("current").toInt() == 2,
              "Presentation advances slides");
        if (audience)
            QMetaObject::invokeMethod(audience, "end");
        check(qmlWarnings == 0, "No QML runtime warnings");
        deck.cleanupRecovery();
        if (failures->isEmpty())
            qInfo("Flux UI smoke passed: drag, Morph scrub, content, notes, presentation.");
        else
            for (const auto& failure : *failures)
                qCritical().noquote() << failure;
        QCoreApplication::exit(failures->isEmpty() ? 0 : 3);
    });
}
