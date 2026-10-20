import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: app
    objectName: "mainWindow"
    width: 1440
    height: 960
    minimumWidth: 1080
    minimumHeight: 720
    visible: true
    title: deck.title + (deck.dirty ? " •" : "") + " — Flux"
    color: "#182630"
    font.family: "sans-serif"
    font.pixelSize: 13
    palette.window: "#111d28"
    palette.windowText: "#dce6ed"
    palette.base: "#192934"
    palette.text: "#dce6ed"
    palette.button: "#253844"
    palette.buttonText: "#dce6ed"
    palette.highlight: "#52898b"
    palette.highlightedText: "#ffffff"
    palette.mid: "#354a57"
    palette.dark: "#101a22"
    palette.light: "#405966"
    palette.placeholderText: "#738b9e"
    palette.disabled.text: "#6c8292"
    palette.disabled.buttonText: "#6c8292"
    palette.disabled.windowText: "#6c8292"
    property bool previewing: false
    property real previewProgress: 0
    property var previewTracks: []
    property bool builds: false
    property real previewSeconds: 1.1
    property string pending: ""
    property bool allowClose: false
    function startPreview(play) {
        player.stop();
        previewTracks = builds ? deck.entranceTracks(deck.slideIndex) : deck.morphTracks(deck.slideIndex);
        previewSeconds = builds ? deck.buildDuration(deck.slideIndex) : (deck.currentSlide.transition === "cut" ? 0.001 : deck.currentSlide.duration);
        previewProgress = 0;
        previewing = true;
        if (play) {
            player.duration = previewSeconds * 1000;
            player.start();
        }
    }
    function stopPreview() {
        player.stop();
        previewing = false;
        editor.forceActiveFocus();
    }
    function requestAction(name) {
        pending = name;
        if (deck.dirty)
            unsaved.open();
        else
            runPending();
    }
    function runPending() {
        var name = pending;
        pending = "";
        stopPreview();
        if (name === "new")
            deck.newDeck(false);
        else if (name === "demo")
            deck.newDeck(true);
        else if (name === "open")
            openDialog.open();
        else if (name === "close") {
            deck.cleanupRecovery();
            allowClose = true;
            Qt.quit();
        }
    }
    function saveProject() {
        if (deck.filePath) {
            if (deck.save() && pending)
                runPending();
        } else
            saveDialog.open();
    }
    function editObject() {
        if (deck.selectionCount !== 1 || ["text", "code", "equation", "chart", "table"].indexOf(deck.selected.kind) < 0)
            return;
        contentDialog.objectId = deck.selectedId;
        contentDialog.kind = deck.selected.kind;
        contentEdit.text = deck.selected.text || "";
        contentDialog.open();
        contentEdit.forceActiveFocus();
    }
    function editNotes() {
        notesEdit.text = deck.currentSlide.notes || "";
        notesDialog.open();
    }
    function present(current) {
        stopPreview();
        audience.start(current ? deck.slideIndex : 0);
        if (Qt.application.screens.length > 1)
            presenter.start();
    }
    function renameSlide() {
        slideNameEdit.text = deck.currentSlide.name || "";
        renameSlideDialog.open();
        slideNameEdit.forceActiveFocus();
        slideNameEdit.selectAll();
    }
    function showSlideMenu(item, index, x, y) {
        stopPreview();
        deck.slideIndex = index;
        item.forceActiveFocus();
        slideContextMenu.popup(item, x, y);
    }
    onClosing: function (close) {
        if (!allowClose) {
            close.accepted = false;
            requestAction("close");
        }
    }
    Connections {
        target: deck
        function onDocumentChanged() {
            if (app.previewing)
                app.stopPreview();
        }
    }
    NumberAnimation {
        id: player
        target: app
        property: "previewProgress"
        from: 0
        to: 1
        duration: 1100
    }
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: app.saveProject()
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        onActivated: saveDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: app.requestAction("open")
    }
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: app.requestAction("new")
    }
    Shortcut {
        sequence: "Ctrl+Z"
        enabled: editor.activeFocus
        onActivated: deck.undo()
    }
    Shortcut {
        sequence: "Ctrl+Shift+Z"
        enabled: editor.activeFocus
        onActivated: deck.redo()
    }
    Shortcut {
        sequence: "Ctrl+D"
        enabled: editor.activeFocus
        onActivated: deck.selectedId ? deck.duplicateObject() : deck.duplicateSlide()
    }
    Shortcut {
        sequence: "Ctrl+A"
        enabled: editor.activeFocus
        onActivated: deck.selectAll()
    }
    Shortcut {
        sequence: "Ctrl+X"
        enabled: editor.activeFocus && !!deck.selectedId
        onActivated: deck.cutObject()
    }
    Shortcut {
        sequence: "Ctrl+C"
        enabled: editor.activeFocus && !!deck.selectedId
        onActivated: deck.copyObject()
    }
    Shortcut {
        sequence: "Ctrl+V"
        enabled: editor.activeFocus
        onActivated: deck.pasteObject()
    }
    Shortcut {
        sequence: "Ctrl+Shift+D"
        onActivated: deck.duplicateSlide()
    }
    Shortcut {
        sequence: "F5"
        onActivated: app.present(false)
    }
    Shortcut {
        sequence: "Shift+F5"
        onActivated: app.present(true)
    }
    Shortcut {
        sequence: "Ctrl+Return"
        onActivated: app.startPreview(true)
    }
    Shortcut {
        sequence: "F1"
        onActivated: helpDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            height: 62
            color: "#101b25"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 18
                spacing: 12
                Rectangle {
                    width: 28
                    height: 28
                    radius: 8
                    color: "#ffb48d"
                    Text {
                        text: "f"
                        anchors.centerIn: parent
                        font.pixelSize: 27
                        font.bold: true
                        color: "#182a31"
                    }
                }
                Text {
                    text: "flux"
                    color: "#edf5f4"
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    font.letterSpacing: -1
                }
                Rectangle {
                    width: 1
                    height: 24
                    color: "#31434f"
                    Layout.leftMargin: 14
                    Layout.rightMargin: 8
                }
                Tool {
                    text: "File ▾"
                    onClicked: fileMenu.popup()
                }
                TextField {
                    id: titleField
                    text: deck.title
                    Layout.preferredWidth: 260
                    background: Rectangle {
                        color: titleField.activeFocus ? "#223541" : "transparent"
                        radius: 5
                    }
                    onEditingFinished: deck.title = text
                    selectByMouse: true
                }
                Text {
                    text: deck.dirty ? "●" : "✓"
                    color: deck.dirty ? "#ffb48d" : "#8edbd3"
                    font.pixelSize: 13
                }
                Item {
                    Layout.fillWidth: true
                }
                Tool {
                    text: "↶"
                    hint: "Undo · Ctrl+Z"
                    enabled: deck.canUndo
                    onClicked: deck.undo()
                }
                Tool {
                    text: "↷"
                    hint: "Redo · Ctrl+Shift+Z"
                    enabled: deck.canRedo
                    onClicked: deck.redo()
                }
                Tool {
                    text: "Save"
                    onClicked: app.saveProject()
                }
                Tool {
                    text: "▶ Present"
                    accent: true
                    hint: "Present from this slide · Shift+F5"
                    onClicked: app.present(true)
                }
                Tool {
                    text: "?"
                    hint: "Help and keyboard shortcuts"
                    onClicked: helpDialog.open()
                }
            }
        }
        Rectangle {
            visible: deck.recoveryAvailable
            Layout.fillWidth: true
            height: visible ? 44 : 0
            color: "#34413c"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                Text {
                    text: "A locally saved recovery draft is available."
                    color: "#d7e5d8"
                }
                Item {
                    Layout.fillWidth: true
                }
                Tool {
                    text: "Restore draft"
                    accent: true
                    onClicked: {
                        if (deck.dirty)
                            deck.report("Save or discard the current project before restoring.");
                        else
                            deck.recover();
                    }
                }
                Tool {
                    text: "Dismiss"
                    onClicked: deck.discardRecovery()
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 52
            color: "#14222d"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 18
                spacing: 5
                Text {
                    text: "INSERT"
                    color: "#738d9f"
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                    Layout.rightMargin: 12
                }
                Tool {
                    text: "T  Text"
                    onClicked: {
                        app.stopPreview();
                        deck.addObject("text");
                    }
                }
                Tool {
                    text: "▧  Image"
                    onClicked: imageDialog.open()
                }
                Tool {
                    text: "◻  Shape ▾"
                    onClicked: shapeMenu.popup()
                }
                Rectangle {
                    width: 1
                    height: 20
                    color: "#2c3e49"
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                }
                Tool {
                    text: "</>  Code"
                    onClicked: {
                        app.stopPreview();
                        deck.addObject("code");
                    }
                }
                Tool {
                    text: "ƒ  Equation"
                    onClicked: {
                        app.stopPreview();
                        deck.addObject("equation");
                    }
                }
                Tool {
                    text: "▥  Chart"
                    onClicked: {
                        app.stopPreview();
                        deck.addObject("chart");
                    }
                }
                Tool {
                    text: "▦  Table"
                    onClicked: {
                        app.stopPreview();
                        deck.addObject("table");
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Tool {
                    text: "Grid"
                    checked: editor.showGrid
                    onClicked: editor.showGrid = !editor.showGrid
                }
                Tool {
                    text: "Snap"
                    checked: editor.snap
                    onClicked: editor.snap = !editor.snap
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.preferredWidth: 204
                Layout.fillHeight: true
                color: "#111e28"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "SLIDES"
                            color: "#8ca4b5"
                            font.pixelSize: 10
                            font.letterSpacing: 1.2
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Text {
                            text: deck.slides.length
                            color: "#6e899c"
                            font.pixelSize: 11
                        }
                        Tool {
                            text: "+"
                            hint: "Add a slide from a layout"
                            onClicked: layoutMenu.popup()
                        }
                    }
                    ListView {
                        id: filmstrip
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 17
                        clip: true
                        model: deck.slides
                        currentIndex: deck.slideIndex
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Item {
                            id: slideThumbnail
                            objectName: "slideThumbnail-" + index
                            required property var modelData
                            required property int index
                            activeFocusOnTab: true
                            width: filmstrip.width
                            height: 125
                            Keys.onPressed: function (event) {
                                if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                                    app.showSlideMenu(slideThumbnail, index, width / 2, 50);
                                    event.accepted = true;
                                }
                            }
                            Rectangle {
                                x: 3
                                y: 2
                                width: parent.width - 6
                                height: 99
                                radius: 5
                                color: "transparent"
                                border.width: deck.slideIndex === index ? 2 : 1
                                border.color: deck.slideIndex === index ? "#a5e7dc" : "#3d505d"
                                Item {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    clip: true
                                    SlideView {
                                        slide: modelData
                                        scale: parent.width / 1280
                                        transformOrigin: Item.TopLeft
                                    }
                                }
                            }
                            Text {
                                x: 5
                                y: 108
                                width: parent.width - 10
                                elide: Text.ElideRight
                                text: (index + 1).toString().padStart(2, "0") + "   " + modelData.name
                                color: deck.slideIndex === index ? "#cceee9" : "#7f98a9"
                                font.pixelSize: 10
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function (mouse) {
                                    if (mouse.button === Qt.RightButton)
                                        app.showSlideMenu(slideThumbnail, index, mouse.x, mouse.y);
                                    else {
                                        app.stopPreview();
                                        deck.slideIndex = index;
                                        editor.forceActiveFocus();
                                    }
                                }
                            }
                        }
                    }
                    Tool {
                        text: "+ New slide ▾"
                        Layout.fillWidth: true
                        accent: true
                        onClicked: layoutMenu.popup()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Tool {
                            text: "↑"
                            hint: "Move slide earlier"
                            onClicked: deck.moveSlide(-1)
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "↓"
                            hint: "Move slide later"
                            onClicked: deck.moveSlide(1)
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "⧉"
                            hint: "Duplicate slide · Ctrl+Shift+D"
                            onClicked: deck.duplicateSlide()
                            Layout.fillWidth: true
                        }
                        Tool {
                            text: "×"
                            hint: "Delete slide"
                            onClicked: deleteSlideDialog.open()
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: "#2b3c47"
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 22
                    Layout.rightMargin: 22
                    Layout.topMargin: 13
                    Text {
                        text: app.previewing ? "MOTION PREVIEW" : "CANVAS"
                        font.pixelSize: 10
                        font.letterSpacing: 1.5
                        color: app.previewing ? "#8edbd3" : "#718c9f"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Tool {
                        text: "Notes"
                        hint: "Edit speaker notes"
                        onClicked: app.editNotes()
                    }
                    Text {
                        text: Math.round(editor.canvasScale * 100) + "%"
                        color: "#819aaa"
                        font.pixelSize: 11
                    }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    EditCanvas {
                        id: editor
                        anchors.fill: parent
                        visible: !app.previewing
                        onEditContent: app.editObject()
                        onRemoveRequested: deck.deleteObject()
                        onObjectContextRequested: function (x, y) {
                            objectContextMenu.popup(editor, x, y);
                        }
                        onCanvasContextRequested: function (x, y, slideX, slideY) {
                            canvasContextMenu.slideX = slideX;
                            canvasContextMenu.slideY = slideY;
                            canvasContextMenu.popup(editor, x, y);
                        }
                    }
                    Item {
                        anchors.fill: parent
                        visible: app.previewing
                        MotionStage {
                            x: (parent.width - 1280 * editor.canvasScale) / 2
                            y: (parent.height - 720 * editor.canvasScale) / 2
                            scale: editor.canvasScale
                            transformOrigin: Item.TopLeft
                            tracks: app.previewTracks
                            progress: app.previewProgress
                            builds: app.builds
                            totalDuration: app.previewSeconds
                            fromBackground: app.builds || deck.slideIndex === 0 ? deck.currentSlide.background : deck.slides[deck.slideIndex - 1].background
                            toBackground: deck.currentSlide.background
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 126
                    color: "#14222d"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text {
                                text: "MOTION STUDIO"
                                color: "#8ca6b5"
                                font.pixelSize: 10
                                font.letterSpacing: 1.2
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Tool {
                                text: "Slide transition"
                                checked: !app.builds
                                onClicked: {
                                    app.builds = false;
                                    app.stopPreview();
                                }
                            }
                            Tool {
                                text: "Object entrances"
                                checked: app.builds
                                onClicked: {
                                    app.builds = true;
                                    app.stopPreview();
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            Tool {
                                text: player.running ? "Ⅱ" : "▶"
                                accent: true
                                hint: "Preview motion · Ctrl+Enter"
                                onClicked: {
                                    if (player.running)
                                        player.stop();
                                    else
                                        app.startPreview(true);
                                }
                            }
                            Slider {
                                id: scrubber
                                objectName: "motionScrubber"
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                value: app.previewProgress
                                onMoved: {
                                    var desiredProgress = value;
                                    if (!app.previewing)
                                        app.startPreview(false);
                                    player.stop();
                                    app.previewProgress = desiredProgress;
                                }
                            }
                            Text {
                                text: (app.previewProgress * app.previewSeconds).toFixed(2) + " s"
                                color: "#b1c7d3"
                                font.family: "monospace"
                                font.pixelSize: 12
                                Layout.preferredWidth: 65
                            }
                            Tool {
                                text: app.previewing ? "Back to edit" : "Preview"
                                onClicked: {
                                    if (app.previewing)
                                        app.stopPreview();
                                    else
                                        app.startPreview(true);
                                }
                            }
                        }
                    }
                }
            }
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: "#2b3c47"
            }
            Inspector {
                id: inspector
                objectName: "inspector"
                Layout.preferredWidth: 278
                Layout.fillHeight: true
                onEditContent: app.editObject()
                onEditNotes: app.editNotes()
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 30
            color: "#101b25"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                Rectangle {
                    width: 5
                    height: 5
                    radius: 3
                    color: "#8edbd3"
                }
                Text {
                    text: deck.status
                    color: "#849eaf"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: "LOCAL  /  OFFLINE"
                    color: "#607e90"
                    font.pixelSize: 9
                    font.letterSpacing: 1.3
                }
            }
        }
    }

    Action {
        id: cutObjectAction
        text: "Cut    Ctrl+X"
        enabled: !!deck.selectedId
        onTriggered: deck.cutObject()
    }
    Action {
        id: copyObjectAction
        text: "Copy    Ctrl+C"
        enabled: !!deck.selectedId
        onTriggered: deck.copyObject()
    }
    Action {
        id: duplicateObjectAction
        text: deck.selectionCount > 1 ? "Duplicate selection    Ctrl+D" : "Duplicate    Ctrl+D"
        enabled: deck.selectionCount > 0 && deck.objects.length + deck.selectionCount <= 250
        onTriggered: deck.duplicateObject()
    }
    Action {
        id: deleteObjectAction
        text: deck.selectionCount > 1 ? "Delete selection    Del" : "Delete    Del"
        enabled: !!deck.selectedId
        onTriggered: deck.deleteObject()
    }
    Menu {
        id: objectContextMenu
        objectName: "objectContextMenu"
        width: 250
        property int selectedLayer: {
            for (var i = 0; i < deck.objects.length; ++i)
                if (deck.objects[i].id === deck.selectedId)
                    return i;
            return -1;
        }
        MenuItem {
            text: ["chart", "table"].indexOf(deck.selected.kind) >= 0 ? "Edit data…" : "Edit content…"
            enabled: deck.selectionCount === 1 && ["text", "code", "equation", "chart", "table"].indexOf(deck.selected.kind) >= 0
            onTriggered: app.editObject()
        }
        MenuSeparator {}
        MenuItem {
            action: cutObjectAction
        }
        MenuItem {
            objectName: "contextCopyObject"
            action: copyObjectAction
        }
        MenuItem {
            text: "Paste    Ctrl+V"
            enabled: deck.canPaste && deck.objects.length < 250
            onTriggered: deck.pasteObject()
        }
        MenuItem {
            objectName: "contextDuplicateObject"
            action: duplicateObjectAction
        }
        MenuSeparator {}
        Menu {
            title: "Arrange"
            enabled: deck.selectionCount === 1
            MenuItem {
                text: "Bring to front"
                enabled: objectContextMenu.selectedLayer < deck.objects.length - 1
                onTriggered: deck.layerObject(250)
            }
            MenuItem {
                text: "Bring forward"
                enabled: objectContextMenu.selectedLayer < deck.objects.length - 1
                onTriggered: deck.layerObject(1)
            }
            MenuItem {
                text: "Send backward"
                enabled: objectContextMenu.selectedLayer > 0
                onTriggered: deck.layerObject(-1)
            }
            MenuItem {
                text: "Send to back"
                enabled: objectContextMenu.selectedLayer > 0
                onTriggered: deck.layerObject(-250)
            }
        }
        Menu {
            title: deck.selectionCount > 1 ? "Align selection to slide" : "Align to slide"
            MenuItem {
                text: "Left margin"
                onTriggered: deck.alignObject("left")
            }
            MenuItem {
                text: "Horizontal center"
                onTriggered: deck.alignObject("center")
            }
            MenuItem {
                text: "Right margin"
                onTriggered: deck.alignObject("right")
            }
            MenuItem {
                text: "Vertical center"
                onTriggered: deck.alignObject("middle")
            }
        }
        MenuItem {
            text: "Format object"
            enabled: deck.selectionCount === 1
            onTriggered: inspector.tab = 0
        }
        MenuItem {
            text: "Motion settings"
            enabled: deck.selectionCount === 1
            onTriggered: inspector.tab = 1
        }
        MenuItem {
            text: "Break Morph match"
            enabled: inspector.matched
            onTriggered: deck.unlinkObject()
        }
        MenuSeparator {}
        MenuItem {
            action: deleteObjectAction
        }
    }
    Menu {
        id: canvasContextMenu
        objectName: "canvasContextMenu"
        width: 240
        property real slideX: 160
        property real slideY: 180
        function insert(kind) {
            deck.addObjectAt(kind, slideX, slideY);
        }
        MenuItem {
            objectName: "contextPasteHere"
            text: "Paste here"
            enabled: deck.canPaste && deck.objects.length < 250
            onTriggered: deck.pasteObjectAt(canvasContextMenu.slideX, canvasContextMenu.slideY)
        }
        Menu {
            title: "Insert"
            enabled: deck.objects.length < 250
            MenuItem {
                text: "Text"
                onTriggered: canvasContextMenu.insert("text")
            }
            MenuItem {
                text: "Rectangle"
                onTriggered: canvasContextMenu.insert("rect")
            }
            MenuItem {
                text: "Circle / ellipse"
                onTriggered: canvasContextMenu.insert("ellipse")
            }
            MenuItem {
                text: "Arrow"
                onTriggered: canvasContextMenu.insert("arrow")
            }
            MenuSeparator {}
            MenuItem {
                text: "Code block"
                onTriggered: canvasContextMenu.insert("code")
            }
            MenuItem {
                text: "Equation"
                onTriggered: canvasContextMenu.insert("equation")
            }
            MenuItem {
                text: "Chart"
                onTriggered: canvasContextMenu.insert("chart")
            }
            MenuItem {
                text: "Table"
                onTriggered: canvasContextMenu.insert("table")
            }
            MenuItem {
                text: "Image…"
                onTriggered: imageDialog.open()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Undo    Ctrl+Z"
            enabled: deck.canUndo
            onTriggered: deck.undo()
        }
        MenuItem {
            text: "Redo    Ctrl+Shift+Z"
            enabled: deck.canRedo
            onTriggered: deck.redo()
        }
        MenuSeparator {}
        MenuItem {
            text: "Show grid"
            checkable: true
            checked: editor.showGrid
            onTriggered: editor.showGrid = !editor.showGrid
        }
        MenuItem {
            text: "Snap to grid"
            checkable: true
            checked: editor.snap
            onTriggered: editor.snap = !editor.snap
        }
        MenuSeparator {}
        MenuItem {
            text: "Slide settings"
            onTriggered: inspector.tab = 0
        }
        MenuItem {
            text: "Presenter notes…"
            onTriggered: app.editNotes()
        }
    }
    Menu {
        id: slideContextMenu
        objectName: "slideContextMenu"
        width: 260
        MenuItem {
            text: "New blank slide"
            enabled: deck.slides.length < 200
            onTriggered: deck.addSlide("blank")
        }
        MenuItem {
            objectName: "contextDuplicateSlide"
            text: "Duplicate slide    Ctrl+Shift+D"
            enabled: deck.slides.length < 200
            onTriggered: deck.duplicateSlide()
        }
        MenuItem {
            text: "Rename slide…"
            onTriggered: app.renameSlide()
        }
        MenuSeparator {}
        MenuItem {
            text: "Move earlier"
            enabled: deck.slideIndex > 0
            onTriggered: deck.moveSlide(-1)
        }
        MenuItem {
            text: "Move later"
            enabled: deck.slideIndex < deck.slides.length - 1
            onTriggered: deck.moveSlide(1)
        }
        MenuSeparator {}
        MenuItem {
            text: "Presenter notes…"
            onTriggered: app.editNotes()
        }
        MenuItem {
            text: "Present from here    Shift+F5"
            onTriggered: app.present(true)
        }
        MenuSeparator {}
        MenuItem {
            text: "Delete slide…"
            enabled: deck.slides.length > 1
            onTriggered: deleteSlideDialog.open()
        }
    }
    Menu {
        id: fileMenu
        MenuItem {
            text: "New presentation    Ctrl+N"
            onTriggered: app.requestAction("new")
        }
        MenuItem {
            text: "Open…    Ctrl+O"
            onTriggered: app.requestAction("open")
        }
        MenuItem {
            text: "Save    Ctrl+S"
            onTriggered: app.saveProject()
        }
        MenuItem {
            text: "Save as…    Ctrl+Shift+S"
            onTriggered: saveDialog.open()
        }
        MenuSeparator {}
        MenuItem {
            text: "Export PDF…"
            onTriggered: pdfDialog.open()
        }
        MenuItem {
            text: "Show presenter window"
            enabled: audience.visible
            onTriggered: presenter.start()
        }
        MenuItem {
            text: "Load example deck"
            onTriggered: app.requestAction("demo")
        }
        MenuSeparator {}
        MenuItem {
            text: "Quit"
            onTriggered: app.requestAction("close")
        }
    }
    Menu {
        id: shapeMenu
        MenuItem {
            text: "Rectangle / diagram node"
            onTriggered: deck.addObject("rect")
        }
        MenuItem {
            text: "Circle / ellipse"
            onTriggered: deck.addObject("ellipse")
        }
        MenuItem {
            text: "Arrow / connector"
            onTriggered: deck.addObject("arrow")
        }
    }
    Menu {
        id: layoutMenu
        MenuItem {
            text: "Blank canvas"
            onTriggered: deck.addSlide("blank")
        }
        MenuItem {
            text: "Title slide"
            onTriggered: deck.addSlide("title")
        }
        MenuItem {
            text: "Two columns"
            onTriggered: deck.addSlide("two-column")
        }
        MenuItem {
            text: "Code + explanation"
            onTriggered: deck.addSlide("technical")
        }
        MenuItem {
            text: "Data story"
            onTriggered: deck.addSlide("data")
        }
        MenuItem {
            text: "Section divider"
            onTriggered: deck.addSlide("section")
        }
    }
    FileDialog {
        id: openDialog
        title: "Open Flux presentation"
        nameFilters: ["Flux presentations (*.flux)"]
        onAccepted: deck.open(selectedFile)
    }
    FileDialog {
        id: saveDialog
        title: "Save Flux presentation"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Flux presentations (*.flux)"]
        defaultSuffix: "flux"
        onAccepted: {
            if (deck.save(selectedFile) && app.pending)
                app.runPending();
        }
        onRejected: app.pending = ""
    }
    FileDialog {
        id: pdfDialog
        title: "Export PDF"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PDF documents (*.pdf)"]
        defaultSuffix: "pdf"
        onAccepted: deck.exportPdf(selectedFile)
    }
    FileDialog {
        id: imageDialog
        title: "Insert local image"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: deck.importImage(selectedFile)
    }
    Dialog {
        id: contentDialog
        objectName: "contentDialog"
        property string objectId: ""
        property string kind: "text"
        title: kind === "chart" || kind === "table" ? "Edit data" : "Edit " + kind
        anchors.centerIn: parent
        width: Math.min(760, app.width - 100)
        height: 500
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent
            spacing: 14
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#a4bac8"
                font.pixelSize: 12
                text: contentDialog.kind === "chart" ? "One label,value pair per line. Up to 32 data points. Negative values are supported." : contentDialog.kind === "table" ? "Separate columns with tabs and rows with newlines. You can paste cells from a spreadsheet. First row is the header." : contentDialog.kind === "equation" ? "Offline TeX subset: \\frac{a}{b}, \\sqrt{x}, x^{2}, a_{i}, Greek symbols, \\sum, \\int. No dollar signs needed." : contentDialog.kind === "code" ? "Editable code with lightweight syntax highlighting. Source is displayed, never executed." : "Write your text. Use the inspector to change size, alignment, weight, and color."
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: contentEdit
                    objectName: "contentEditor"
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    font.family: contentDialog.kind === "text" ? "sans-serif" : "monospace"
                    font.pixelSize: 18
                }
            }
        }
        onAccepted: deck.updateObject(objectId, {
            text: contentEdit.text
        })
    }
    Dialog {
        id: notesDialog
        objectName: "notesDialog"
        title: "Presenter notes · slide " + (deck.slideIndex + 1)
        anchors.centerIn: parent
        width: 650
        height: 430
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ScrollView {
            anchors.fill: parent
            TextArea {
                id: notesEdit
                objectName: "notesEditor"
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                font.pixelSize: 18
                placeholderText: "What do you want to say? These notes stay off the audience screen."
            }
        }
        onAccepted: deck.updateSlide({
            notes: notesEdit.text
        })
    }
    Dialog {
        id: unsaved
        title: "Save your changes?"
        anchors.centerIn: parent
        width: 440
        modal: true
        ColumnLayout {
            width: parent.width
            spacing: 20
            Text {
                text: "Save this presentation before continuing."
                color: "#c9d9e2"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Tool {
                    text: "Cancel"
                    onClicked: {
                        app.pending = "";
                        unsaved.close();
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Tool {
                    text: "Discard"
                    onClicked: {
                        unsaved.close();
                        app.runPending();
                    }
                }
                Tool {
                    text: "Save"
                    accent: true
                    onClicked: {
                        unsaved.close();
                        app.saveProject();
                    }
                }
            }
        }
    }
    Dialog {
        id: renameSlideDialog
        title: "Rename slide"
        anchors.centerIn: parent
        width: 380
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField {
            id: slideNameEdit
            width: parent.width
            maximumLength: 160
            placeholderText: "Slide title"
            onAccepted: renameSlideDialog.accept()
        }
        onAccepted: {
            if (slideNameEdit.text.trim().length)
                deck.updateSlide({
                    name: slideNameEdit.text.trim()
                });
        }
    }
    Dialog {
        id: deleteSlideDialog
        title: "Delete this slide?"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            text: "You can undo this change."
        }
        onAccepted: deck.deleteSlide()
    }
    Dialog {
        id: helpDialog
        title: "Make your ideas move."
        anchors.centerIn: parent
        width: 610
        height: 460
        modal: true
        standardButtons: Dialog.Close
        ScrollView {
            anchors.fill: parent
            TextArea {
                readOnly: true
                wrapMode: TextEdit.Wrap
                font.pixelSize: 15
                color: "#ccdde5"
                background: null
                text: "START WITH MORPH\nDuplicate a slide (Ctrl+Shift+D), move or resize an object, and press Ctrl+Enter. The scrubber lets you inspect every moment.\n\nEDIT\nDrag on empty canvas to select enclosed objects. Shift/Ctrl-click toggles objects; Ctrl+A selects all. Drag a selected object to move the whole selection. Drag the lower-right handle to resize; hold Shift to preserve proportions. Arrow keys nudge; Shift moves 10 pixels. Double-click to edit content. Right-click objects, slides, or empty canvas for actions. Shift+F10 also opens a menu. Ctrl+X/C/V cuts, copies, and pastes objects while the canvas has focus.\n\nMOTION\nThe Motion inspector sets paths, easing, entrances, delay, and duration. Bézier control points use slide coordinates.\n\nPRESENT\nF5 starts at slide 1; Shift+F5 starts here. Space / Right advances; Left / Page Up or right-click reverses the transition; B blacks out; Escape ends. A second monitor receives the audience view automatically. On one monitor, use File → Show presenter window if needed.\n\nSAVE\nCtrl+S saves a portable .flux project, including images. PDF export contains static slides. Recovery drafts are stored locally.\n\nFlux 0.1.2 · Native C++ and Qt Quick · Fully offline"
            }
        }
    }
    Presentation {
        id: audience
        objectName: "audience"
        onEnded: {
            presenter.stop();
            app.requestActivate();
            editor.forceActiveFocus();
        }
    }
    Presenter {
        id: presenter
        presentation: audience
    }
}
