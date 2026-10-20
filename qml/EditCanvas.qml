import QtQuick
import QtQuick.Controls

Item {
    id: editor
    objectName: "editCanvas"
    property bool snap: true
    property bool showGrid: false
    property bool guideX: false
    property bool guideY: false
    property real dragDx: 0
    property real dragDy: 0
    property bool selectionResizing: false
    property real selectionScale: 1
    property point resizeAnchor: Qt.point(0, 0)
    property bool gestureCanceled: false
    property bool marqueeActive: false
    property point marqueeStart
    property point marqueeEnd
    signal editContent
    signal removeRequested
    signal objectContextRequested(real x, real y)
    signal canvasContextRequested(real x, real y, real slideX, real slideY)
    property real canvasScale: Math.min((width - 72) / 1280, (height - 64) / 720)
    function snapped(v) {
        return snap ? Math.round(v / 8) * 8 : Math.round(v);
    }
    function nudge(dx, dy) {
        deck.translateSelection(dx, dy);
    }
    function cancelSelectionResize() {
        selectionResizing = false;
        selectionScale = 1;
    }
    Connections {
        target: deck
        function onDocumentChanged() {
            editor.cancelSelectionResize();
        }
        function onSelectionChanged() {
            editor.cancelSelectionResize();
        }
    }
    function openKeyboardMenu() {
        if (deck.selectedId) {
            var bounds = deck.selectionBounds;
            var point = paper.mapToItem(editor, bounds.x + bounds.w / 2, bounds.y + bounds.h / 2);
            objectContextRequested(point.x, point.y);
        } else {
            var center = paper.mapToItem(editor, 640, 360);
            canvasContextRequested(center.x, center.y, 640, 360);
        }
    }
    function openCanvasMenu(area, x, y) {
        deck.select("");
        editor.forceActiveFocus();
        var point = area.mapToItem(editor, x, y);
        var position = area.mapToItem(paper, x, y);
        canvasContextRequested(point.x, point.y, snapped(position.x), snapped(position.y));
    }
    focus: true
    Keys.onPressed: function (event) {
        var step = event.modifiers & Qt.ShiftModifier ? 10 : 1;
        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
            openKeyboardMenu();
            event.accepted = true;
        } else if (event.key === Qt.Key_Left) {
            nudge(-step, 0);
            event.accepted = true;
        } else if (event.key === Qt.Key_Right) {
            nudge(step, 0);
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            nudge(0, -step);
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            nudge(0, step);
            event.accepted = true;
        } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
            removeRequested();
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            if (selectionResizing) {
                cancelSelectionResize();
            } else if (background.pressed) {
                gestureCanceled = true;
                marqueeActive = false;
                deck.selectMany(background.originalSelection);
            } else {
                gestureCanceled = true;
                dragDx = 0;
                dragDy = 0;
                deck.select("");
            }
            event.accepted = true;
        }
    }
    MouseArea {
        id: background
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        property var originalSelection: []
        property var baseSelection: []
        property bool additive: false
        onPressed: function (mouse) {
            editor.forceActiveFocus();
            editor.gestureCanceled = false;
            if (mouse.button !== Qt.LeftButton)
                return;
            additive = !!(mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier));
            originalSelection = deck.selectedIds.slice();
            baseSelection = additive ? originalSelection : [];
            editor.marqueeStart = mapToItem(paper, mouse.x, mouse.y);
            editor.marqueeEnd = editor.marqueeStart;
            editor.marqueeActive = false;
        }
        onPositionChanged: function (mouse) {
            if (!(pressedButtons & Qt.LeftButton) || editor.gestureCanceled)
                return;
            editor.marqueeEnd = mapToItem(paper, mouse.x, mouse.y);
            if (Math.abs(editor.marqueeEnd.x - editor.marqueeStart.x) * editor.canvasScale > 4 || Math.abs(editor.marqueeEnd.y - editor.marqueeStart.y) * editor.canvasScale > 4)
                editor.marqueeActive = true;
            if (editor.marqueeActive)
                deck.selectRectangle(editor.marqueeStart.x, editor.marqueeStart.y, editor.marqueeEnd.x - editor.marqueeStart.x, editor.marqueeEnd.y - editor.marqueeStart.y, baseSelection);
        }
        onReleased: function (mouse) {
            if (mouse.button !== Qt.LeftButton || editor.gestureCanceled)
                return;
            if (!editor.marqueeActive && !additive)
                deck.select("");
            editor.marqueeActive = false;
        }
        onCanceled: {
            if (editor.marqueeActive)
                deck.selectMany(originalSelection);
            editor.marqueeActive = false;
        }
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton)
                editor.openCanvasMenu(this, mouse.x, mouse.y);
        }
    }
    Rectangle {
        id: paper
        objectName: "slidePaper"
        width: 1280
        height: 720
        x: (editor.width - 1280 * editor.canvasScale) / 2
        y: (editor.height - 720 * editor.canvasScale) / 2
        scale: editor.canvasScale
        transformOrigin: Item.TopLeft
        color: deck.currentSlide.background || "#101e2c"
        clip: false
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1 / editor.canvasScale
            color: "transparent"
            border.color: "#344754"
            border.width: 1 / editor.canvasScale
        }
        Canvas {
            anchors.fill: parent
            visible: editor.showGrid
            onPaint: {
                var c = getContext("2d");
                c.clearRect(0, 0, width, height);
                c.fillStyle = "#41515c";
                for (var x = 0; x < 1280; x += 32)
                    for (var y = 0; y < 720; y += 32)
                        c.fillRect(x, y, 1.5, 1.5);
            }
        }
        Repeater {
            model: deck.objects
            delegate: Item {
                id: item
                objectName: "object-" + modelData.id
                required property var modelData
                property real dw: 0
                property real dh: 0
                property bool selected: deck.selectedIds.indexOf(modelData.id) >= 0
                property real resizeScale: selected ? editor.selectionScale : 1
                x: selected && editor.selectionResizing ? editor.resizeAnchor.x + (modelData.x - editor.resizeAnchor.x) * resizeScale : modelData.x + (selected ? editor.dragDx : 0)
                y: selected && editor.selectionResizing ? editor.resizeAnchor.y + (modelData.y - editor.resizeAnchor.y) * resizeScale : modelData.y + (selected ? editor.dragDy : 0)
                width: modelData.w * resizeScale + dw
                height: modelData.h * resizeScale + dh
                rotation: modelData.rotation || 0
                NodeVisual {
                    node: item.modelData
                    opacity: item.modelData.opacity === undefined ? 1 : item.modelData.opacity
                    transform: Scale {
                        xScale: item.width / item.modelData.w
                        yScale: item.height / item.modelData.h
                    }
                }
                MouseArea {
                    id: mover
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: (pressedButtons & Qt.LeftButton) ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property point pressAt
                    onPressed: function (mouse) {
                        editor.forceActiveFocus();
                        editor.gestureCanceled = false;
                        if (mouse.button === Qt.LeftButton && (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)))
                            deck.toggleSelection(item.modelData.id);
                        else if (!item.selected)
                            deck.select(item.modelData.id);
                        pressAt = mapToItem(paper, mouse.x, mouse.y);
                    }
                    onPositionChanged: function (mouse) {
                        if (!(pressedButtons & Qt.LeftButton) || !item.selected || editor.gestureCanceled)
                            return;
                        var p = mapToItem(paper, mouse.x, mouse.y);
                        var bounds = deck.selectionBounds;
                        var nx = editor.snapped(bounds.x + p.x - pressAt.x);
                        var ny = editor.snapped(bounds.y + p.y - pressAt.y);
                        editor.guideX = editor.snap && Math.abs(nx + bounds.w / 2 - 640) < 8;
                        editor.guideY = editor.snap && Math.abs(ny + bounds.h / 2 - 360) < 8;
                        if (editor.guideX)
                            nx = 640 - bounds.w / 2;
                        if (editor.guideY)
                            ny = 360 - bounds.h / 2;
                        editor.dragDx = nx - bounds.x;
                        editor.dragDy = ny - bounds.y;
                    }
                    onReleased: function (mouse) {
                        if (mouse.button !== Qt.LeftButton)
                            return;
                        editor.guideX = false;
                        editor.guideY = false;
                        var dx = editor.dragDx, dy = editor.dragDy;
                        editor.dragDx = 0;
                        editor.dragDy = 0;
                        if (!editor.gestureCanceled)
                            deck.translateSelection(dx, dy);
                    }
                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton) {
                            var point = mapToItem(editor, mouse.x, mouse.y);
                            editor.objectContextRequested(point.x, point.y);
                        }
                    }
                    onCanceled: {
                        editor.dragDx = 0;
                        editor.dragDy = 0;
                        editor.guideX = false;
                        editor.guideY = false;
                    }
                    onDoubleClicked: function (mouse) {
                        if (mouse.button === Qt.LeftButton && deck.selectionCount === 1)
                            editor.editContent();
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -3 / editor.canvasScale
                    visible: item.selected
                    color: "transparent"
                    border.color: "#8edbd3"
                    border.width: 1.5 / editor.canvasScale
                }
                Rectangle {
                    visible: item.selected && deck.selectionCount === 1
                    x: parent.width - width / 2
                    y: parent.height - height / 2
                    width: 10 / editor.canvasScale
                    height: width
                    radius: 2 / editor.canvasScale
                    color: "#d2fff7"
                    border.color: "#285959"
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -5 / editor.canvasScale
                        cursorShape: Qt.SizeFDiagCursor
                        property point pressAt
                        onPressed: function (mouse) {
                            pressAt = mapToItem(item, mouse.x, mouse.y);
                        }
                        onPositionChanged: function (mouse) {
                            if (!pressed)
                                return;
                            var p = mapToItem(item, mouse.x, mouse.y);
                            var w = Math.max(24, editor.snapped(item.modelData.w + p.x - pressAt.x));
                            var h = Math.max(24, editor.snapped(item.modelData.h + p.y - pressAt.y));
                            if (mouse.modifiers & Qt.ShiftModifier)
                                h = w * item.modelData.h / item.modelData.w;
                            item.dw = w - item.modelData.w;
                            item.dh = h - item.modelData.h;
                        }
                        onReleased: {
                            if (item.dw !== 0 || item.dh !== 0)
                                deck.updateObject(item.modelData.id, {
                                    w: item.width,
                                    h: item.height
                                });
                        }
                        onCanceled: {
                            item.dw = 0;
                            item.dh = 0;
                        }
                    }
                }
            }
        }
        Rectangle {
            id: selectionFrame
            objectName: "selectionBounds"
            visible: deck.selectionCount > 1
            x: editor.selectionResizing ? editor.resizeAnchor.x + (deck.selectionBounds.x - editor.resizeAnchor.x) * editor.selectionScale : deck.selectionBounds.x + editor.dragDx
            y: editor.selectionResizing ? editor.resizeAnchor.y + (deck.selectionBounds.y - editor.resizeAnchor.y) * editor.selectionScale : deck.selectionBounds.y + editor.dragDy
            width: deck.selectionBounds.w * editor.selectionScale
            height: deck.selectionBounds.h * editor.selectionScale
            color: "transparent"
            border.color: "#b5f7e9"
            border.width: 1 / editor.canvasScale
            z: 1000
            Text {
                x: 12 / editor.canvasScale
                y: -23 / editor.canvasScale
                text: (deck.groupedSelection ? "GROUP" : "SELECTION") + " · " + deck.selectionCount + (editor.selectionResizing ? " · " + Math.round(editor.selectionScale * 100) + "%" : "")
                color: "#b5f7e9"
                font.pixelSize: 10 / editor.canvasScale
                font.letterSpacing: 1 / editor.canvasScale
            }
            Repeater {
                model: 4
                delegate: Rectangle {
                    id: resizeHandle
                    required property int index
                    property bool rightCorner: index % 2 === 1
                    property bool bottomCorner: index >= 2
                    objectName: "selectionResize-" + index
                    width: 10 / editor.canvasScale
                    height: width
                    x: (rightCorner ? selectionFrame.width : 0) - width / 2
                    y: (bottomCorner ? selectionFrame.height : 0) - height / 2
                    radius: 2 / editor.canvasScale
                    color: "#d2fff7"
                    border.color: "#285959"
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -5 / editor.canvasScale
                        acceptedButtons: Qt.LeftButton
                        preventStealing: true
                        cursorShape: resizeHandle.rightCorner === resizeHandle.bottomCorner ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                        property point pressAt
                        property point diagonal
                        property bool moved: false
                        onPressed: function (mouse) {
                            editor.forceActiveFocus();
                            editor.gestureCanceled = false;
                            var bounds = deck.selectionBounds;
                            pressAt = mapToItem(paper, mouse.x, mouse.y);
                            diagonal = Qt.point(bounds.w * (resizeHandle.rightCorner ? 1 : -1), bounds.h * (resizeHandle.bottomCorner ? 1 : -1));
                            editor.resizeAnchor = Qt.point(bounds.x + (resizeHandle.rightCorner ? 0 : bounds.w), bounds.y + (resizeHandle.bottomCorner ? 0 : bounds.h));
                            editor.selectionScale = 1;
                            editor.selectionResizing = true;
                            moved = false;
                        }
                        onPositionChanged: function (mouse) {
                            if (!pressed || !editor.selectionResizing)
                                return;
                            var point = mapToItem(paper, mouse.x, mouse.y);
                            var dx = point.x - pressAt.x, dy = point.y - pressAt.y;
                            if (!moved && Math.hypot(dx, dy) * editor.canvasScale < 3)
                                return;
                            moved = true;
                            var factor = 1 + (dx * diagonal.x + dy * diagonal.y) / (diagonal.x * diagonal.x + diagonal.y * diagonal.y);
                            editor.selectionScale = deck.constrainSelectionScale(factor, editor.resizeAnchor.x, editor.resizeAnchor.y);
                        }
                        onReleased: {
                            if (!editor.selectionResizing)
                                return;
                            var factor = editor.selectionScale;
                            var anchor = editor.resizeAnchor;
                            editor.cancelSelectionResize();
                            deck.scaleSelection(factor, anchor.x, anchor.y);
                        }
                        onCanceled: editor.cancelSelectionResize()
                    }
                }
            }
        }
        Rectangle {
            objectName: "selectionRectangle"
            visible: editor.marqueeActive
            x: Math.min(editor.marqueeStart.x, editor.marqueeEnd.x)
            y: Math.min(editor.marqueeStart.y, editor.marqueeEnd.y)
            width: Math.abs(editor.marqueeEnd.x - editor.marqueeStart.x)
            height: Math.abs(editor.marqueeEnd.y - editor.marqueeStart.y)
            color: "#258edbd3"
            border.color: "#a9f5e5"
            border.width: 1 / editor.canvasScale
            z: 1001
        }
        Rectangle {
            x: 640
            y: 0
            width: 1 / editor.canvasScale
            height: 720
            color: "#9ae4d9"
            visible: editor.guideX
        }
        Rectangle {
            x: 0
            y: 360
            width: 1280
            height: 1 / editor.canvasScale
            color: "#9ae4d9"
            visible: editor.guideY
        }
        Rectangle {
            x: 0
            y: 720 + 14 / editor.canvasScale
            width: 180 / editor.canvasScale
            height: 20 / editor.canvasScale
            color: "transparent"
            Text {
                text: "16:9  /  1280 × 720"
                color: "#617988"
                font.pixelSize: 10 / editor.canvasScale
            }
        }
    }
}
