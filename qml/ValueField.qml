import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string label: ""
    property real value: 0
    property real minimum: -20000
    property real maximum: 20000
    signal edited(real number)
    spacing: 5
    Text {
        text: root.label
        color: "#8fa3b4"
        font.pixelSize: 11
    }
    TextField {
        Layout.fillWidth: true
        implicitHeight: 34
        text: Number(root.value.toFixed(2)).toString()
        selectByMouse: true
        validator: DoubleValidator {
            bottom: root.minimum
            top: root.maximum
            decimals: 2
            locale: "C"
        }
        onEditingFinished: {
            if (acceptableInput)
                root.edited(Number(text));
            else
                text = root.value.toString();
        }
    }
}
