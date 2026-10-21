import QtQuick
import Flux.Native 1.0

Rectangle {
    id: stage
    property var tracks: []
    property real progress: 0
    property bool builds: false
    property real totalDuration: 1
    property color fromBackground: "#101e2c"
    property color toBackground: "#101e2c"
    width: 1280
    height: 720
    clip: true
    color: Qt.rgba(fromBackground.r + (toBackground.r - fromBackground.r) * progress, fromBackground.g + (toBackground.g - fromBackground.g) * progress, fromBackground.b + (toBackground.b - fromBackground.b) * progress, 1)
    Repeater {
        model: stage.tracks
        delegate: Item {
            id: object
            required property var modelData
            property bool spatialText: !stage.builds && !!modelData.spatialText
            MotionPose {
                id: pose
                from: object.modelData.from
                to: object.modelData.to
                progress: stage.builds ? Math.max(0, Math.min(1, (stage.progress * stage.totalDuration - object.modelData.delay) / Math.max(0.001, object.modelData.duration))) : stage.progress
            }
            x: pose.position.x
            y: pose.position.y
            width: pose.extent.width
            height: pose.extent.height
            rotation: pose.angle
            opacity: pose.alpha
            TextMorphItem {
                anchors.fill: parent
                visible: object.spatialText
                from: object.modelData.from
                to: object.modelData.to
                progress: pose.easedProgress
                inkColor: pose.ink
            }
            NodeVisual {
                visible: !object.spatialText
                node: object.modelData.to
                fillColor: pose.fill
                inkColor: pose.ink
                opacity: object.modelData.crossfade ? pose.progress : 1
                transform: Scale {
                    xScale: object.width / Math.max(1, object.modelData.to.w)
                    yScale: object.height / Math.max(1, object.modelData.to.h)
                }
            }
            NodeVisual {
                visible: !object.spatialText && object.modelData.crossfade
                node: object.modelData.from
                fillColor: pose.fill
                inkColor: pose.ink
                opacity: 1 - pose.progress
                transform: Scale {
                    xScale: object.width / Math.max(1, object.modelData.from.w)
                    yScale: object.height / Math.max(1, object.modelData.from.h)
                }
            }
        }
    }
}
