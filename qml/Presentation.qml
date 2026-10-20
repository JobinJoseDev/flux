import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: show
    title: "Flux · presentation"
    color: "black"
    width: 1280
    height: 720
    visible: false
    property int current: 0
    property bool playing: false
    property bool building: false
    property bool blackout: false
    property real progress: 1
    property real seconds: 1
    property var tracks: []
    property color before: "#101e2c"
    property color after: "#101e2c"
    signal ended
    function start(index) {
        current = index;
        blackout = false;
        if (Qt.application.screens.length > 1)
            screen = Qt.application.screens[1];
        showFullScreen();
        input.forceActiveFocus();
        build();
    }
    function end() {
        clock.stop();
        playing = false;
        hide();
        ended();
    }
    function build() {
        building = true;
        tracks = deck.entranceTracks(current);
        seconds = deck.buildDuration(current);
        before = deck.slides[current].background;
        after = before;
        progress = 0;
        playing = true;
        clock.duration = seconds * 1000;
        clock.restart();
    }
    function go(delta) {
        if (current + delta < 0 || current + delta >= deck.slides.length)
            return;
        clock.stop();
        current += delta;
        blackout = false;
        if (delta < 0 || deck.slides[current].transition === "cut") {
            build();
            return;
        }
        var next = JSON.parse(JSON.stringify(deck.morphTracks(current)));
        for (var i = 0; i < next.length; i++)
            if (next[i].to.animation && next[i].to.animation !== "none")
                next[i].to.opacity = 0;
        tracks = next;
        seconds = deck.slides[current].duration;
        before = deck.slides[current - 1].background;
        after = deck.slides[current].background;
        building = false;
        progress = 0;
        playing = true;
        clock.duration = seconds * 1000;
        clock.restart();
    }
    NumberAnimation {
        id: clock
        target: show
        property: "progress"
        from: 0
        to: 1
        duration: 1100
        onFinished: {
            if (!show.building)
                show.build();
            else
                show.playing = false;
        }
    }
    Item {
        id: input
        anchors.fill: parent
        focus: true
        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_Escape) {
                show.end();
                event.accepted = true;
            } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Space || event.key === Qt.Key_PageDown) {
                show.go(1);
                event.accepted = true;
            } else if (event.key === Qt.Key_Left || event.key === Qt.Key_PageUp) {
                show.go(-1);
                event.accepted = true;
            } else if (event.key === Qt.Key_B) {
                show.blackout = !show.blackout;
                event.accepted = true;
            }
        }
        Item {
            width: 1280
            height: 720
            scale: Math.min(show.width / 1280, show.height / 720)
            anchors.centerIn: parent
            SlideView {
                slide: deck.slides[show.current] || {}
                visible: !show.playing
            }
            MotionStage {
                visible: show.playing
                tracks: show.tracks
                progress: show.progress
                builds: show.building
                totalDuration: show.seconds
                fromBackground: show.before
                toBackground: show.after
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: show.go(1)
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: function (mouse) {
                if (mouse.button === Qt.RightButton) {
                    show.go(-1);
                    mouse.accepted = false;
                }
            }
        }
        Rectangle {
            anchors.fill: parent
            color: "black"
            visible: show.blackout
        }
    }
    onClosing: function (close) {
        close.accepted = false;
        end();
    }
}
