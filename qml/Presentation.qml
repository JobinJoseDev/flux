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
    property bool reversing: false
    property bool runBuildsAfterTransition: false
    property int transitionEdge: 0
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
        reversing = false;
        tracks = deck.entranceTracks(current);
        seconds = deck.buildDuration(current);
        before = deck.slides[current].background;
        after = before;
        progress = 0;
        playing = true;
        clock.from = 0;
        clock.to = 1;
        clock.duration = seconds * 1000;
        clock.restart();
    }
    function go(delta) {
        if ((delta !== 1 && delta !== -1) || current + delta < 0 || current + delta >= deck.slides.length)
            return;
        var target = current + delta;
        // Turn around on the current path without jumping to an endpoint.
        if (playing && !building && target === (reversing ? transitionEdge : transitionEdge - 1)) {
            clock.stop();
            current = target;
            reversing = !reversing;
            blackout = false;
            clock.from = progress;
            clock.to = reversing ? 0 : 1;
            clock.duration = Math.max(1, seconds * 1000 * Math.abs(clock.to - progress));
            clock.restart();
            return;
        }
        clock.stop();
        transitionEdge = delta > 0 ? target : current;
        current = target;
        reversing = delta < 0;
        blackout = false;
        if (deck.slides[transitionEdge].transition === "cut") {
            if (reversing) {
                playing = false;
                building = false;
            } else
                build();
            return;
        }
        var next = JSON.parse(JSON.stringify(deck.morphTracks(transitionEdge)));
        if (!reversing)
            for (var i = 0; i < next.length; i++)
                if (next[i].to.animation && next[i].to.animation !== "none")
                    next[i].to.opacity = 0;
        tracks = next;
        seconds = deck.slides[transitionEdge].duration;
        before = deck.slides[transitionEdge - 1].background;
        after = deck.slides[transitionEdge].background;
        building = false;
        runBuildsAfterTransition = !reversing;
        progress = reversing ? 1 : 0;
        playing = true;
        clock.from = progress;
        clock.to = reversing ? 0 : 1;
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
            if (!show.building && !show.reversing && show.runBuildsAfterTransition)
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
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: function (mouse) {
                show.go(mouse.button === Qt.RightButton ? -1 : 1);
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
