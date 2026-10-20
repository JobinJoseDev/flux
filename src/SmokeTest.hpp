#pragma once
#include "Document.hpp"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <cmath>
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
                        Qt::MouseButton button, Qt::MouseButtons buttons,
                        Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, point, window->mapToGlobal(point.toPoint()), button, buttons,
                      modifiers);
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
    struct ResizeProbe {
        QJsonObject before;
        QVariantMap bounds;
        QPointF releaseAt;
        double factor = 1;
    };
    auto resize = std::make_shared<ResizeProbe>();
    auto root = engine.rootObjects().first();
    auto check = [failures](bool ok, const QString& message) {
        if (!ok)
            failures->append(message);
    };
    QTimer::singleShot(100, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(250, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(500, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(750, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(900, Qt::PreciseTimer, root, [root, check] {
        const auto path = qEnvironmentVariable("FLUX_MENU_SCREENSHOT");
        if (!path.isEmpty()) {
            auto item = root->findChild<QQuickItem*>("contextCopyObject");
            check(item && item->window() && item->window()->grabWindow().save(path),
                  "Save context menu screenshot");
        }
    });
    QTimer::singleShot(1000, Qt::PreciseTimer, root, [root, &deck, check] {
        auto item = root->findChild<QQuickItem*>("contextCopyObject");
        check(item && item->isVisible(), "Find Copy menu item");
        clickItem(item);
        check(deck.canPaste(), "Copy menu action writes the clipboard");
    });
    QTimer::singleShot(1200, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(1400, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(1600, Qt::PreciseTimer, root, [root, &deck, check] {
        deck.setSlideIndex(0);
        auto window = qobject_cast<QQuickWindow*>(root);
        auto item = window ? visualItem(window->contentItem(), "slideThumbnail-1") : nullptr;
        check(item != nullptr, "Find second slide thumbnail");
        clickItem(item, Qt::RightButton);
        check(deck.slideIndex() == 1, "Right-click selects the intended slide");
        auto menu = root->findChild<QObject*>("slideContextMenu");
        check(menu && menu->property("visible").toBool(), "Slide context menu opens");
    });
    QTimer::singleShot(1800, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(2000, Qt::PreciseTimer, root, [root, &deck, check] {
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
    QTimer::singleShot(2200, Qt::PreciseTimer, root, [root, check] {
        auto item = root->findChild<QQuickItem*>("contextCopyObject");
        if (item && item->window())
            sendKey(item->window(), Qt::Key_Escape);
        auto menu = root->findChild<QObject*>("objectContextMenu");
        check(menu && !menu->property("visible").toBool(), "Escape dismisses the context menu");
    });
    QTimer::singleShot(2400, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        const double before = deck.selected()["x"].toDouble();
        if (window)
            sendKey(window, Qt::Key_Right);
        check(deck.selected()["x"].toDouble() == before + 1.,
              "Keyboard editing resumes after menu dismissal");
        deck.undo();
    });
    QTimer::singleShot(2600, Qt::PreciseTimer, root, [root, &deck, check] {
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
        // Exercise Fade on the next edge, then Morph on the edge before it.
        deck.setSlideIndex(2);
        deck.updateSlide({{"transition", "fade"}});
        deck.setSlideIndex(1);
        QMetaObject::invokeMethod(root, "present", Q_ARG(QVariant, true));
        auto audience = root->findChild<QObject*>("audience");
        check(audience && audience->property("visible").toBool(), "Audience window opens");
        if (audience)
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, 1));
    });
    QTimer::singleShot(4100, Qt::PreciseTimer, root, [root, &deck, check] {
        auto audience = root->findChild<QObject*>("audience");
        check(audience && audience->property("current").toInt() == 2,
              "Presentation advances slides");
        if (audience) {
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, -1));
            check(audience->property("current").toInt() == 1 &&
                      audience->property("playing").toBool() &&
                      !audience->property("building").toBool() &&
                      audience->property("reversing").toBool(),
                  "Backward navigation starts a reverse transition, not entrance builds");
            check(audience->property("transitionEdge").toInt() == 2,
                  "Backward uses the transition on the slide being left");
        }
    });
    QTimer::singleShot(4450, Qt::PreciseTimer, root, [root, check] {
        auto audience = root->findChild<QObject*>("audience");
        if (audience) {
            const double p = audience->property("progress").toDouble();
            check(p > 0 && p < 1, "Backward transition progresses between endpoints");
        }
    });
    QTimer::singleShot(5600, Qt::PreciseTimer, root, [root, check] {
        auto audience = root->findChild<QQuickWindow*>("audience");
        check(audience && !audience->property("playing").toBool(),
              "Backward finishes without replaying entrances");
        if (audience) {
            const QPointF point(100, 100);
            sendPointer(audience, QEvent::MouseButtonPress, point, Qt::RightButton,
                        Qt::RightButton);
            sendPointer(audience, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
            check(audience->property("current").toInt() == 0 &&
                      audience->property("reversing").toBool(),
                  "Presentation right-click navigates backward once");
            check(audience->property("transitionEdge").toInt() == 1,
                  "Reverse Morph uses the preceding edge");
        }
    });
    QTimer::singleShot(5800, Qt::PreciseTimer, root, [root, check] {
        auto audience = root->findChild<QObject*>("audience");
        if (audience) {
            const double before = audience->property("progress").toDouble();
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, 1));
            check(std::abs(audience->property("progress").toDouble() - before) < .001,
                  "Direction change preserves the current animation position");
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, -1));
            check(std::abs(audience->property("progress").toDouble() - before) < .001,
                  "Repeated direction change does not jump");
        }
    });
    QTimer::singleShot(7100, Qt::PreciseTimer, root, [root, &deck, check] {
        auto audience = root->findChild<QObject*>("audience");
        if (audience) {
            check(audience->property("current").toInt() == 0 &&
                      !audience->property("playing").toBool(),
                  "Reverse Morph reaches the first slide");
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, -1));
            check(audience->property("current").toInt() == 0, "Cannot navigate before slide one");
            deck.setSlideIndex(0);
            deck.updateObject(deck.objects().first().toMap()["id"].toString(),
                              {{"animation", "fade"}, {"duration", 2}});
            deck.setSlideIndex(1);
            deck.updateSlide({{"transition", "cut"}});
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, 1));
            QMetaObject::invokeMethod(audience, "go", Q_ARG(QVariant, -1));
            check(audience->property("current").toInt() == 0 &&
                      !audience->property("playing").toBool(),
                  "Cut stays immediate backward, without replaying the previous slide's entrances");
            QMetaObject::invokeMethod(audience, "end");
        }
        deck.newDeck(false);
        deck.addSlide("blank");
        deck.addObjectAt("rect", 160, 180);
        deck.updateObject(deck.selectedId(), {{"w", 100}, {"h", 100}});
        deck.addObjectAt("ellipse", 340, 180);
        deck.updateObject(deck.selectedId(), {{"w", 100}, {"h", 100}});
        deck.addObjectAt("rect", 700, 400);
        deck.updateObject(deck.selectedId(), {{"w", 100}, {"h", 100}});
        deck.select("");
    });
    QTimer::singleShot(7500, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto paper = visualItem(window->contentItem(), "slidePaper");
        if (paper) {
            const auto start = paper->mapToScene({120, 150});
            const auto end = paper->mapToScene({480, 320});
            sendPointer(window, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
            sendPointer(window, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
            check(deck.selectionCount() == 2, "Rectangle pointer gesture selects enclosed objects");
            auto rectangle = visualItem(window->contentItem(), "selectionRectangle");
            check(rectangle && rectangle->isVisible(),
                  "Selection rectangle is visible during dragging");
        } else
            check(false, "Find rectangle selection canvas");
    });
    QTimer::singleShot(7700, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        const auto screenshot = qEnvironmentVariable("FLUX_SELECTION_SCREENSHOT");
        if (!screenshot.isEmpty())
            check(window->grabWindow().save(screenshot), "Capture rectangle selection framebuffer");
        auto paper = visualItem(window->contentItem(), "slidePaper");
        if (paper)
            sendPointer(window, QEvent::MouseButtonRelease, paper->mapToScene({480, 320}),
                        Qt::LeftButton, Qt::NoButton);
        check(deck.selectionCount() == 2, "Selection persists after releasing the rectangle");
    });
    QTimer::singleShot(7900, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        const auto before = deck.json();
        auto first = deck.objects().first().toMap();
        auto second = deck.objects()[1].toMap();
        auto item = visualItem(window->contentItem(), "object-" + first["id"].toString());
        if (item) {
            auto point = item->mapToScene({50, 50});
            sendPointer(window, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
            sendPointer(window, QEvent::MouseMove, point + QPointF(40, 20), Qt::NoButton,
                        Qt::LeftButton);
            sendPointer(window, QEvent::MouseButtonRelease, point + QPointF(40, 20), Qt::LeftButton,
                        Qt::NoButton);
            const double dx1 =
                deck.objects().first().toMap()["x"].toDouble() - first["x"].toDouble();
            const double dx2 = deck.objects()[1].toMap()["x"].toDouble() - second["x"].toDouble();
            check(dx1 != 0 && dx1 == dx2 && deck.selectionCount() == 2,
                  "Dragging selected object moves the whole selection");
            deck.undo();
            check(deck.json() == before && deck.selectionCount() == 2,
                  "Batch drag is one undo step");
        } else
            check(false, "Find selected object for batch dragging");
        item = visualItem(window->contentItem(), "object-" + first["id"].toString());
        clickItem(item, Qt::RightButton);
        check(deck.selectionCount() == 2, "Right-click preserves an existing multi-selection");
        auto menuItem = root->findChild<QQuickItem*>("contextCopyObject");
        if (menuItem && menuItem->window())
            sendKey(menuItem->window(), Qt::Key_Escape);
    });
    QTimer::singleShot(8200, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto clickModified = [&](int index, Qt::KeyboardModifiers modifiers) {
            auto item = visualItem(window->contentItem(),
                                   "object-" + deck.objects()[index].toMap()["id"].toString());
            if (!item) {
                check(false, "Find object for modified click");
                return;
            }
            auto point = item->mapToScene({50, 50});
            sendPointer(window, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton,
                        modifiers);
            sendPointer(window, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton,
                        modifiers);
        };
        clickModified(2, Qt::ShiftModifier);
        check(deck.selectionCount() == 3, "Shift-click adds an object");
        clickModified(1, Qt::ControlModifier);
        check(deck.selectionCount() == 2, "Ctrl-click removes an object");
        sendKey(window, Qt::Key_A, Qt::ControlModifier);
        check(deck.selectionCount() == 3, "Ctrl+A selects every object");
        sendKey(window, Qt::Key_C, Qt::ControlModifier);
        sendKey(window, Qt::Key_V, Qt::ControlModifier);
        check(deck.objects().size() == 6 && deck.selectionCount() == 3,
              "Keyboard clipboard pastes the whole selection");
        deck.undo();
        check(deck.objects().size() == 3 && deck.selectionCount() == 3,
              "Undo restores selection after batch paste");
        sendKey(window, Qt::Key_Delete);
        check(deck.objects().isEmpty(), "Delete removes the whole selection");
        deck.undo();
        check(deck.objects().size() == 3 && deck.selectionCount() == 3,
              "Undo restores all deleted objects");
    });
    QTimer::singleShot(8600, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto paper = visualItem(window->contentItem(), "slidePaper");
        if (deck.objects().size() < 3) {
            check(false, "Selection test setup still contains three objects");
            return;
        }
        const auto third = deck.objects()[2].toMap()["id"].toString();
        deck.select(third);
        if (paper) {
            const auto start = paper->mapToScene({480, 320});
            const auto end = paper->mapToScene({120, 150});
            sendPointer(window, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton,
                        Qt::ShiftModifier);
            sendPointer(window, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton,
                        Qt::ShiftModifier);
            check(deck.selectionCount() == 3,
                  "Shift rectangle adds enclosed objects in either drag direction");
            sendKey(window, Qt::Key_Escape);
            sendPointer(window, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
            check(deck.selectedIds() == QStringList{third},
                  "Escape cancels rectangle and restores previous selection");
        }
    });
    QTimer::singleShot(9000, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto editor = visualItem(window->contentItem(), "editCanvas");
        deck.selectRectangle(120, 150, 360, 170);
        if (editor)
            editor->forceActiveFocus();
        sendKey(window, Qt::Key_G, Qt::ControlModifier);
        check(deck.groupedSelection() && deck.selectionCount() == 2,
              "Ctrl+G groups the selected objects");
        deck.select("");
    });
    QTimer::singleShot(9300, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto item = visualItem(window->contentItem(),
                               "object-" + deck.objects().first().toMap()["id"].toString());
        clickItem(item);
        check(deck.groupedSelection() && deck.selectionCount() == 2,
              "Clicking a member selects the whole group");
        clickItem(item, Qt::RightButton);
        auto group = root->findChild<QQuickItem*>("contextGroupSelection");
        auto ungroup = root->findChild<QQuickItem*>("contextUngroupSelection");
        check(group && !group->isEnabled() && ungroup && ungroup->isEnabled(),
              "Context menu offers Ungroup for an existing group");
    });
    QTimer::singleShot(9500, Qt::PreciseTimer, root, [root, &deck, check] {
        clickItem(root->findChild<QQuickItem*>("contextUngroupSelection"));
        check(!deck.canUngroup() && deck.canGroup() && deck.selectionCount() == 2,
              "Ungroup menu action preserves the selected objects");
    });
    QTimer::singleShot(9700, Qt::PreciseTimer, root, [root, &deck] {
        auto window = qobject_cast<QQuickWindow*>(root);
        clickItem(visualItem(window->contentItem(),
                             "object-" + deck.objects().first().toMap()["id"].toString()),
                  Qt::RightButton);
    });
    QTimer::singleShot(9900, Qt::PreciseTimer, root, [root, &deck, check] {
        clickItem(root->findChild<QQuickItem*>("contextGroupSelection"));
        check(deck.groupedSelection(), "Group menu action creates a persistent group");
        if (auto inspector = root->findChild<QObject*>("inspector"))
            inspector->setProperty("tab", 0);
    });
    QTimer::singleShot(10200, Qt::PreciseTimer, root, [root, &deck, resize, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto editor = visualItem(window->contentItem(), "editCanvas");
        auto paper = visualItem(editor, "slidePaper");
        auto handle = visualItem(editor, "selectionResize-3");
        check(handle && handle->isVisible() && paper, "Find collective resize corner handle");
        if (!handle || !paper)
            return;
        resize->before = deck.json();
        resize->bounds = deck.selectionBounds();
        const double w = resize->bounds["w"].toDouble(), h = resize->bounds["h"].toDouble();
        const auto start = handle->mapToScene({handle->width() / 2, handle->height() / 2});
        resize->releaseAt = start + paper->mapToScene({w * .4, h * .4}) - paper->mapToScene({0, 0});
        sendPointer(window, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        sendPointer(window, QEvent::MouseMove, resize->releaseAt, Qt::NoButton, Qt::LeftButton);
        resize->factor = editor->property("selectionScale").toDouble();
        check(editor->property("selectionResizing").toBool() && resize->factor > 1.3,
              "Corner drag previews proportional group resizing");
        check(deck.json() == resize->before, "Resize preview leaves the saved document untouched");
    });
    QTimer::singleShot(10400, Qt::PreciseTimer, root, [root, &deck, resize, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        const auto screenshot = qEnvironmentVariable("FLUX_GROUP_SCREENSHOT");
        if (!screenshot.isEmpty())
            check(window->grabWindow().save(screenshot), "Capture group resize framebuffer");
        sendPointer(window, QEvent::MouseButtonRelease, resize->releaseAt, Qt::LeftButton,
                    Qt::NoButton);
        const auto bounds = deck.selectionBounds();
        check(deck.json() != resize->before && deck.groupedSelection(),
              "Releasing the corner commits the group resize");
        check(std::abs(bounds["w"].toDouble() - resize->bounds["w"].toDouble() * resize->factor) <
                      .01 &&
                  std::abs(bounds["h"].toDouble() -
                           resize->bounds["h"].toDouble() * resize->factor) < .01 &&
                  std::abs(bounds["x"].toDouble() - resize->bounds["x"].toDouble()) < .01 &&
                  std::abs(bounds["y"].toDouble() - resize->bounds["y"].toDouble()) < .01,
              "Resize keeps proportions and the opposite corner fixed");
        const auto after = deck.json();
        deck.undo();
        check(deck.json() == resize->before, "One undo restores the entire group resize");
        deck.redo();
        check(deck.json() == after, "Redo restores the resized group");
    });
    QTimer::singleShot(10700, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto editor = visualItem(window->contentItem(), "editCanvas");
        auto handle = visualItem(editor, "selectionResize-0");
        check(handle && handle->isVisible(), "Find opposite corner for resize cancellation");
        if (!handle)
            return;
        const auto before = deck.json();
        const auto start = handle->mapToScene({handle->width() / 2, handle->height() / 2});
        const auto end = start - QPointF(30, 12);
        sendPointer(window, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        sendPointer(window, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
        check(editor->property("selectionScale").toDouble() > 1, "Top-left handle also resizes");
        sendKey(window, Qt::Key_Escape);
        sendPointer(window, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
        check(deck.json() == before && deck.groupedSelection() &&
                  !editor->property("selectionResizing").toBool() &&
                  editor->property("selectionScale").toDouble() == 1,
              "Escape cancels resizing without losing the group selection");
    });
    QTimer::singleShot(11000, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        auto editor = visualItem(window->contentItem(), "editCanvas");
        auto handle = visualItem(editor, "selectionResize-1");
        check(handle && handle->isVisible(), "Find corner for lost-grab cancellation");
        if (!handle)
            return;
        const auto before = deck.json();
        const auto start = handle->mapToScene({handle->width() / 2, handle->height() / 2});
        const auto end = start + QPointF(30, -12);
        sendPointer(window, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        sendPointer(window, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
        check(editor->property("selectionResizing").toBool(), "Resize owns an active pointer grab");
        if (auto grabber = window->mouseGrabberItem())
            grabber->ungrabMouse();
        sendPointer(window, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
        check(deck.json() == before && !editor->property("selectionResizing").toBool(),
              "Losing the pointer grab cancels without a partial document edit");
    });
    QTimer::singleShot(11300, Qt::PreciseTimer, root, [root, &deck, check] {
        auto window = qobject_cast<QQuickWindow*>(root);
        sendKey(window, Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier);
        check(deck.canGroup() && !deck.canUngroup(), "Ctrl+Shift+G ungroups the selection");
        sendKey(window, Qt::Key_G, Qt::ControlModifier);
        const auto before = deck.json();
        sendKey(window, Qt::Key_D, Qt::ControlModifier);
        check(deck.objects().size() == 5 && deck.groupedSelection(),
              "Keyboard duplicate creates a new group");
        deck.select(deck.objects().first().toMap()["id"].toString());
        check(deck.selectionCount() == 2, "Duplicated group does not join its source group");
        deck.undo();
        check(deck.json() == before && deck.selectionCount() == 2,
              "Duplicate group undoes in one step");
    });
    QTimer::singleShot(11800, Qt::PreciseTimer, root, [root, &deck, &qmlWarnings, failures, check] {
        check(qmlWarnings == 0, "No QML runtime warnings");
        deck.cleanupRecovery();
        if (failures->isEmpty())
            qInfo("Flux UI smoke passed: grouping, collective resize and cancellation, "
                  "rectangle selection, batch edits, reverse transitions, "
                  "context menus, clipboard, keyboard, content, notes, presentation.");
        else
            for (const auto& failure : *failures)
                qCritical().noquote() << failure;
        QCoreApplication::exit(failures->isEmpty() ? 0 : 3);
    });
}
