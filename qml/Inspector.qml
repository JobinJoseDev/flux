import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: inspector
    color: "#111d28"
    property var selection: deck.selected
    property bool hasSelection: deck.selectionCount === 1
    property int tab: 0
    property bool matched: {
        if (!hasSelection || deck.slideIndex < 1)
            return false;
        var prior = deck.slides[deck.slideIndex - 1].objects;
        for (var i = 0; i < prior.length; i++)
            if (prior[i].id === deck.selectedId && prior[i].kind === selection.kind)
                return true;
        return false;
    }
    signal editContent
    signal editNotes
    function set(key, value) {
        var obj = {};
        obj[key] = value;
        deck.updateObject(deck.selectedId, obj);
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 8
            Tool {
                text: "Design"
                checked: inspector.tab === 0
                onClicked: inspector.tab = 0
                Layout.fillWidth: true
            }
            Tool {
                text: "Motion"
                checked: inspector.tab === 1
                onClicked: inspector.tab = 1
                Layout.fillWidth: true
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#26343f"
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: inspector.width - 32
                x: 16
                spacing: 17
                Item {
                    height: 2
                }
                Text {
                    text: deck.selectionCount > 1 ? (deck.groupedSelection ? "GROUP · " : "") + deck.selectionCount + " OBJECTS" : inspector.hasSelection ? (inspector.selection.kind || "Object").toUpperCase() : "SLIDE " + (deck.slideIndex + 1)
                    color: "#8edbd3"
                    font.pixelSize: 11
                    font.letterSpacing: 1.5
                }
                ColumnLayout {
                    visible: deck.selectionCount > 1
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: deck.groupedSelection ? "Click any member to select the group. Drag to move it; use a corner handle to resize. Ungroup to edit individual objects." : "Drag a selected object to move the selection. Use a corner handle to resize everything proportionally."
                        color: "#a8bfcc"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Tool {
                        text: "Duplicate selection"
                        Layout.fillWidth: true
                        onClicked: deck.duplicateObject()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Tool {
                            text: "Group"
                            enabled: deck.canGroup
                            Layout.fillWidth: true
                            onClicked: deck.groupSelection()
                        }
                        Tool {
                            text: "Ungroup"
                            enabled: deck.canUngroup
                            Layout.fillWidth: true
                            onClicked: deck.ungroupSelection()
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ValueField {
                            label: "Width"
                            value: deck.selectionBounds.w
                            minimum: 1
                            maximum: 40000
                            Layout.fillWidth: true
                            onEdited: function (number) {
                                var b = deck.selectionBounds;
                                deck.scaleSelection(number / b.w, b.x, b.y);
                            }
                        }
                        ValueField {
                            label: "Height"
                            value: deck.selectionBounds.h
                            minimum: 1
                            maximum: 40000
                            Layout.fillWidth: true
                            onEdited: function (number) {
                                var b = deck.selectionBounds;
                                deck.scaleSelection(number / b.h, b.x, b.y);
                            }
                        }
                    }
                    Text {
                        text: "Proportions locked · text scales too"
                        color: "#849eaf"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Tool {
                        text: "Center horizontally"
                        Layout.fillWidth: true
                        onClicked: deck.alignObject("center")
                    }
                    Tool {
                        text: "Center vertically"
                        Layout.fillWidth: true
                        onClicked: deck.alignObject("middle")
                    }
                    Tool {
                        text: "Delete selection"
                        Layout.fillWidth: true
                        onClicked: deck.deleteObject()
                    }
                    Text {
                        text: "Group: Ctrl+G · Ungroup: Ctrl+Shift+G\nCopy / paste: Ctrl+C / Ctrl+V\nNudge: arrows · Shift for 10 pixels\nEscape cancels resizing"
                        color: "#849eaf"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                ColumnLayout {
                    visible: deck.selectionCount === 0
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        text: "Slide title"
                        color: "#9db0c0"
                        font.pixelSize: 12
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: deck.currentSlide.name || ""
                        onEditingFinished: deck.updateSlide({
                            name: text
                        })
                    }
                    Text {
                        text: "Background"
                        color: "#9db0c0"
                        font.pixelSize: 12
                    }
                    RowLayout {
                        Repeater {
                            model: ["#101e2c", "#faf7f0", "#163b42", "#2b2540", "#242424"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 32
                                height: 32
                                radius: 7
                                color: modelData
                                border.width: deck.currentSlide.background === modelData ? 2 : 1
                                border.color: deck.currentSlide.background === modelData ? "#a3e6dc" : "#526471"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: deck.updateSlide({
                                        background: parent.modelData
                                    })
                                }
                            }
                        }
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: deck.currentSlide.background || ""
                        onEditingFinished: deck.updateSlide({
                            background: text
                        })
                    }
                    Tool {
                        text: "Edit presenter notes"
                        hint: "Notes stay on your presenter window"
                        onClicked: inspector.editNotes()
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#2a3944"
                    }
                    Text {
                        text: "Select something on the canvas to shape it. Duplicate a slide to start a Morph transition."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#9aafbe"
                        font.pixelSize: 13
                        lineHeight: 1.3
                    }
                }
                ColumnLayout {
                    visible: inspector.hasSelection && inspector.tab === 0
                    Layout.fillWidth: true
                    spacing: 16
                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 12
                        Layout.fillWidth: true
                        ValueField {
                            label: "X"
                            value: inspector.selection.x || 0
                            Layout.fillWidth: true
                            onEdited: function (n) {
                                inspector.set("x", n);
                            }
                        }
                        ValueField {
                            label: "Y"
                            value: inspector.selection.y || 0
                            Layout.fillWidth: true
                            onEdited: function (n) {
                                inspector.set("y", n);
                            }
                        }
                        ValueField {
                            label: "Width"
                            value: inspector.selection.w || 100
                            minimum: 16
                            Layout.fillWidth: true
                            onEdited: function (n) {
                                inspector.set("w", n);
                            }
                        }
                        ValueField {
                            label: "Height"
                            value: inspector.selection.h || 100
                            minimum: 16
                            Layout.fillWidth: true
                            onEdited: function (n) {
                                inspector.set("h", n);
                            }
                        }
                        ValueField {
                            label: "Rotation °"
                            value: inspector.selection.rotation || 0
                            Layout.fillWidth: true
                            onEdited: function (n) {
                                inspector.set("rotation", n);
                            }
                        }
                        ValueField {
                            label: "Opacity %"
                            value: 100 * (inspector.selection.opacity === undefined ? 1 : inspector.selection.opacity)
                            minimum: 0
                            maximum: 100
                            Layout.fillWidth: true
                            onEdited: function (n) {
                                inspector.set("opacity", n / 100);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Tool {
                            text: "⇤"
                            hint: "Align left"
                            onClicked: deck.alignObject("left")
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "↔"
                            hint: "Center horizontally"
                            onClicked: deck.alignObject("center")
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "⇥"
                            hint: "Align right"
                            onClicked: deck.alignObject("right")
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "↕"
                            hint: "Center vertically"
                            onClicked: deck.alignObject("middle")
                            Layout.fillWidth: true
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#2a3944"
                    }
                    Text {
                        text: "Appearance"
                        color: "#d9e5ea"
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Text {
                        text: "Fill / accent"
                        color: "#91a7b6"
                        font.pixelSize: 11
                    }
                    RowLayout {
                        Rectangle {
                            width: 28
                            height: 28
                            radius: 6
                            color: inspector.selection.fill || "#ffb48d"
                            border.color: "#546572"
                        }
                        TextField {
                            Layout.fillWidth: true
                            text: inspector.selection.fill || ""
                            onEditingFinished: inspector.set("fill", text)
                        }
                    }
                    Text {
                        text: "Text color"
                        color: "#91a7b6"
                        font.pixelSize: 11
                    }
                    RowLayout {
                        Rectangle {
                            width: 28
                            height: 28
                            radius: 6
                            color: inspector.selection.color || "#edf3f2"
                            border.color: "#546572"
                        }
                        TextField {
                            Layout.fillWidth: true
                            text: inspector.selection.color || ""
                            onEditingFinished: inspector.set("color", text)
                        }
                    }
                    RowLayout {
                        visible: ["text", "code", "equation", "table"].indexOf(inspector.selection.kind) >= 0
                        ValueField {
                            label: "Font size"
                            value: inspector.selection.fontSize || 32
                            minimum: 8
                            maximum: 240
                            Layout.fillWidth: true
                            onEdited: function (n) {
                                inspector.set("fontSize", n);
                            }
                        }
                        Tool {
                            text: "B"
                            checked: inspector.selection.bold || false
                            onClicked: inspector.set("bold", !inspector.selection.bold)
                        }
                    }
                    ComboBox {
                        visible: inspector.selection.kind === "text"
                        Layout.fillWidth: true
                        model: ["Left aligned", "Centered", "Right aligned"]
                        currentIndex: ["left", "center", "right"].indexOf(inspector.selection.align || "left")
                        onActivated: function (i) {
                            inspector.set("align", ["left", "center", "right"][i]);
                        }
                    }
                    ComboBox {
                        visible: inspector.selection.kind === "chart"
                        Layout.fillWidth: true
                        model: ["Bar chart", "Line chart"]
                        currentIndex: inspector.selection.chartStyle === "line" ? 1 : 0
                        onActivated: function (i) {
                            inspector.set("chartStyle", i === 1 ? "line" : "bar");
                        }
                    }
                    Tool {
                        visible: ["text", "code", "equation", "chart", "table"].indexOf(inspector.selection.kind) >= 0
                        text: inspector.selection.kind === "chart" || inspector.selection.kind === "table" ? "Edit data…" : "Edit content…"
                        accent: true
                        Layout.fillWidth: true
                        onClicked: inspector.editContent()
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#2a3944"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Tool {
                            text: "Lower"
                            onClicked: deck.layerObject(-1)
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "Raise"
                            onClicked: deck.layerObject(1)
                            Layout.fillWidth: true
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Tool {
                            text: "Duplicate"
                            onClicked: deck.duplicateObject()
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "Delete"
                            onClicked: deck.deleteObject()
                            Layout.fillWidth: true
                        }
                    }
                }
                ColumnLayout {
                    visible: inspector.tab === 1
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        text: "SLIDE TRANSITION"
                        color: "#93a6b6"
                        font.pixelSize: 10
                        font.letterSpacing: 1
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Morph", "Fade", "Cut"]
                        currentIndex: ["morph", "fade", "cut"].indexOf(deck.currentSlide.transition || "morph")
                        onActivated: function (i) {
                            deck.updateSlide({
                                transition: ["morph", "fade", "cut"][i]
                            });
                        }
                    }
                    ValueField {
                        label: "Transition duration · seconds"
                        value: deck.currentSlide.duration || 1.1
                        minimum: 0.1
                        maximum: 10
                        Layout.fillWidth: true
                        onEdited: function (n) {
                            deck.updateSlide({
                                duration: n
                            });
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#2a3944"
                    }
                    ColumnLayout {
                        visible: inspector.hasSelection
                        Layout.fillWidth: true
                        spacing: 14
                        Text {
                            text: "OBJECT MOTION"
                            color: "#93a6b6"
                            font.pixelSize: 10
                            font.letterSpacing: 1
                        }
                        Text {
                            text: "Morph path"
                            color: "#c5d5df"
                            font.pixelSize: 12
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["Straight", "Arc", "Bézier"]
                            currentIndex: ["straight", "arc", "bezier"].indexOf(inspector.selection.path || "straight")
                            onActivated: function (i) {
                                inspector.set("path", ["straight", "arc", "bezier"][i]);
                            }
                        }
                        GridLayout {
                            visible: inspector.selection.path === "bezier"
                            columns: 2
                            Layout.fillWidth: true
                            ValueField {
                                label: "Control 1 · X"
                                value: inspector.selection.c1x === undefined ? (inspector.selection.x || 0) : inspector.selection.c1x
                                Layout.fillWidth: true
                                onEdited: function (n) {
                                    inspector.set("c1x", n);
                                }
                            }
                            ValueField {
                                label: "Control 1 · Y"
                                value: inspector.selection.c1y === undefined ? (inspector.selection.y || 0) - 160 : inspector.selection.c1y
                                Layout.fillWidth: true
                                onEdited: function (n) {
                                    inspector.set("c1y", n);
                                }
                            }
                            ValueField {
                                label: "Control 2 · X"
                                value: inspector.selection.c2x === undefined ? (inspector.selection.x || 0) : inspector.selection.c2x
                                Layout.fillWidth: true
                                onEdited: function (n) {
                                    inspector.set("c2x", n);
                                }
                            }
                            ValueField {
                                label: "Control 2 · Y"
                                value: inspector.selection.c2y === undefined ? (inspector.selection.y || 0) - 160 : inspector.selection.c2y
                                Layout.fillWidth: true
                                onEdited: function (n) {
                                    inspector.set("c2y", n);
                                }
                            }
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["Smooth easing", "Linear easing", "Ease out"]
                            currentIndex: ["smooth", "linear", "out"].indexOf(inspector.selection.easing || "smooth")
                            onActivated: function (i) {
                                inspector.set("easing", ["smooth", "linear", "out"][i]);
                            }
                        }
                        Text {
                            text: "Entrance animation"
                            color: "#c5d5df"
                            font.pixelSize: 12
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["None", "Fade in", "Rise", "Zoom in", "Follow motion path"]
                            currentIndex: ["none", "fade", "rise", "zoom", "path"].indexOf(inspector.selection.animation || "none")
                            onActivated: function (i) {
                                inspector.set("animation", ["none", "fade", "rise", "zoom", "path"][i]);
                            }
                        }
                        RowLayout {
                            ValueField {
                                label: "Delay · s"
                                value: inspector.selection.delay || 0
                                minimum: 0
                                maximum: 20
                                Layout.fillWidth: true
                                onEdited: function (n) {
                                    inspector.set("delay", n);
                                }
                            }
                            ValueField {
                                label: "Duration · s"
                                value: inspector.selection.duration || 0.7
                                minimum: 0.1
                                maximum: 10
                                Layout.fillWidth: true
                                onEdited: function (n) {
                                    inspector.set("duration", n);
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#2a3944"
                        }
                        Text {
                            text: "OBJECT IDENTITY"
                            color: "#93a6b6"
                            font.pixelSize: 10
                            font.letterSpacing: 1
                        }
                        Text {
                            text: inspector.matched ? "● Matched on previous slide" : "○ No match on previous slide"
                            color: inspector.matched ? "#90d7d1" : "#a3b6c4"
                            font.pixelSize: 11
                        }
                        Text {
                            text: deck.selectedId.substring(0, 18) + "…"
                            color: "#90d7d1"
                            font.family: "monospace"
                            font.pixelSize: 12
                        }
                        Text {
                            text: "Duplicated slides keep this identity. Objects with the same identity morph together."
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: "#92a7b6"
                            font.pixelSize: 12
                            lineHeight: 1.25
                        }
                        Tool {
                            text: "Give object a new identity"
                            Layout.fillWidth: true
                            onClicked: deck.unlinkObject()
                        }
                    }
                }
                Item {
                    height: 20
                }
            }
        }
    }
}
