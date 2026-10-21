import QtQuick
import Flux.Native 1.0

Item {
    id: visual
    property var node: ({})
    property color fillColor: node.fill || "#ffb48d"
    property color inkColor: node.color || "#edf3f2"
    width: node.w || 100
    height: node.h || 100
    Loader {
        anchors.fill: parent
        sourceComponent: visual.node.kind === "text" ? textItem : (visual.node.kind === "rect" || visual.node.kind === "ellipse") ? shapeItem : visual.node.kind === "image" ? imageItem : technicalItem
    }
    Component {
        id: textItem
        TextItem {
            node: visual.node
            inkColor: visual.inkColor
        }
    }
    Component {
        id: shapeItem
        Item {
            Rectangle {
                anchors.fill: parent
                visible: visual.node.kind === "rect"
                color: visual.fillColor
                radius: visual.node.radius || 0
                border.color: visual.node.stroke || "transparent"
                border.width: visual.node.strokeWidth || 0
            }
            Rectangle {
                visible: visual.node.kind === "ellipse"
                width: parent.width
                height: parent.width
                radius: width / 2
                color: visual.fillColor
                border.color: visual.node.stroke || "transparent"
                border.width: visual.node.strokeWidth || 0
                transform: Scale {
                    yScale: visual.height / Math.max(1, visual.width)
                }
            }
        }
    }

    Component {
        id: imageItem
        Image {
            source: visual.node.source || ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
        }
    }
    Component {
        id: technicalItem
        ContentItem {
            node: visual.node
        }
    }
}
