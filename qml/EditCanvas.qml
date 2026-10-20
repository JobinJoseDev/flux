import QtQuick
import QtQuick.Controls

Item {
    id: editor
    objectName: "editCanvas"
    property bool snap: true
    property bool showGrid: false
    property bool guideX: false
    property bool guideY: false
    signal editContent
    signal removeRequested
    property real canvasScale: Math.min((width - 72) / 1280, (height - 64) / 720)
    function snapped(v) {
        return snap ? Math.round(v / 8) * 8 : Math.round(v);
    }
    function nudge(dx, dy) {
        if (!deck.selectedId)
            return;
        deck.updateObject(deck.selectedId, {
            x: deck.selected.x + dx,
            y: deck.selected.y + dy
        });
    }
    focus: true
    Keys.onPressed: function (event) {
        var step = event.modifiers & Qt.ShiftModifier ? 10 : 1;
        if (event.key === Qt.Key_Left) {
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
            deck.select("");
            event.accepted = true;
        }
    }
    Rectangle {
        id: paper
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
        MouseArea {
            anchors.fill: parent
            onClicked: {
                deck.select("");
                editor.forceActiveFocus();
            }
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
                property real dx: 0
                property real dy: 0
                property real dw: 0
                property real dh: 0
                property bool selected: deck.selectedId === modelData.id
                x: modelData.x + dx
                y: modelData.y + dy
                width: modelData.w + dw
                height: modelData.h + dh
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
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property point pressAt
                    onPressed: function (mouse) {
                        editor.forceActiveFocus();
                        deck.select(item.modelData.id);
                        pressAt = mapToItem(paper, mouse.x, mouse.y);
                    }
                    onPositionChanged: function (mouse) {
                        if (!pressed)
                            return;
                        var p = mapToItem(paper, mouse.x, mouse.y);
                        var nx = editor.snapped(item.modelData.x + p.x - pressAt.x);
                        var ny = editor.snapped(item.modelData.y + p.y - pressAt.y);
                        editor.guideX = editor.snap && Math.abs(nx + item.width / 2 - 640) < 8;
                        editor.guideY = editor.snap && Math.abs(ny + item.height / 2 - 360) < 8;
                        if (editor.guideX)
                            nx = 640 - item.width / 2;
                        if (editor.guideY)
                            ny = 360 - item.height / 2;
                        item.dx = nx - item.modelData.x;
                        item.dy = ny - item.modelData.y;
                    }
                    onReleased: {
                        editor.guideX = false;
                        editor.guideY = false;
                        if (item.dx !== 0 || item.dy !== 0)
                            deck.updateObject(item.modelData.id, {
                                x: item.x,
                                y: item.y
                            });
                    }
                    onCanceled: {
                        item.dx = 0;
                        item.dy = 0;
                        editor.guideX = false;
                        editor.guideY = false;
                    }
                    onDoubleClicked: editor.editContent()
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
                    visible: item.selected
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
