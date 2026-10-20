import QtQuick

Rectangle {
    id: view
    property var slide: ({})
    width: 1280
    height: 720
    color: slide.background || "#101e2c"
    clip: true
    Repeater {
        model: view.slide.objects || []
        delegate: NodeVisual {
            required property var modelData
            node: modelData
            x: modelData.x
            y: modelData.y
            rotation: modelData.rotation || 0
            opacity: modelData.opacity === undefined ? 1 : modelData.opacity
        }
    }
}
