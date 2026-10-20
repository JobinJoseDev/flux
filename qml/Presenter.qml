import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: panel
    property var presentation
    property int elapsed: 0
    title: "Flux · presenter view"
    width: 1000
    height: 710
    minimumWidth: 760
    minimumHeight: 560
    color: "#101a24"
    function start() {
        elapsed = 0;
        timer.start();
        showNormal();
        requestActivate();
    }
    function stop() {
        timer.stop();
        hide();
    }
    Timer {
        id: timer
        interval: 1000
        repeat: true
        onTriggered: panel.elapsed++
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "PRESENTER VIEW"
                color: "#8edbd3"
                font.pixelSize: 12
                font.letterSpacing: 2
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                text: Math.floor(panel.elapsed / 60).toString().padStart(2, "0") + ":" + (panel.elapsed % 60).toString().padStart(2, "0")
                color: "#edf3f4"
                font.pixelSize: 28
                font.family: "monospace"
            }
            Tool {
                text: "End show"
                onClicked: panel.presentation.end()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            spacing: 24
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                SlideView {
                    slide: deck.slides[panel.presentation.current] || {}
                    scale: Math.min(parent.width / 1280, parent.height / 720)
                    transformOrigin: Item.TopLeft
                }
            }
            ColumnLayout {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                Text {
                    text: "UP NEXT"
                    color: "#8fa9b9"
                    font.pixelSize: 11
                    font.letterSpacing: 1
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    SlideView {
                        visible: panel.presentation.current + 1 < deck.slides.length
                        slide: deck.slides[panel.presentation.current + 1] || {}
                        scale: parent.width / 1280
                        transformOrigin: Item.TopLeft
                    }
                    Text {
                        visible: panel.presentation.current + 1 >= deck.slides.length
                        text: "End of presentation"
                        color: "#9aafbe"
                        anchors.centerIn: parent
                    }
                }
            }
        }
        Text {
            text: "SPEAKER NOTES"
            color: "#8fa9b9"
            font.pixelSize: 11
            font.letterSpacing: 1
        }
        ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            TextArea {
                readOnly: true
                wrapMode: TextEdit.Wrap
                text: (deck.slides[panel.presentation.current] || {}).notes || "No notes for this slide."
                color: "#e6eef1"
                font.pixelSize: 22
                background: Rectangle {
                    color: "#172733"
                    radius: 10
                }
                padding: 18
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Tool {
                text: "← Previous"
                enabled: panel.presentation.current > 0
                onClicked: panel.presentation.go(-1)
            }
            Tool {
                text: panel.presentation.blackout ? "Show screen" : "Blackout"
                onClicked: panel.presentation.blackout = !panel.presentation.blackout
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                text: (panel.presentation.current + 1) + " / " + deck.slides.length
                color: "#a9bfcd"
            }
            Tool {
                text: "Next →"
                accent: true
                enabled: panel.presentation.current < deck.slides.length - 1
                onClicked: panel.presentation.go(1)
            }
        }
    }
    onClosing: function (close) {
        close.accepted = false;
        stop();
    }
}
