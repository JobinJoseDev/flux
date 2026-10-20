import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    property string hint: ""
    implicitHeight: 34
    implicitWidth: Math.max(34, label.implicitWidth + 22)
    hoverEnabled: true
    contentItem: Text {
        id: label
        text: control.text
        color: control.enabled ? (control.accent ? "#17232c" : "#d4e0e7") : "#596976"
        font.pixelSize: 12
        font.weight: control.accent ? Font.DemiBold : Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 7
        color: control.accent ? (control.hovered ? "#ffcba7" : "#ffb48d") : control.checked ? "#324650" : control.hovered ? "#293944" : "transparent"
        border.color: control.checked ? "#71a9ad" : "transparent"
    }
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.text: hint
    ToolTip.delay: 500
    Accessible.name: hint.length ? hint : text
}
