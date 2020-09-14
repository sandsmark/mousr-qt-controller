import QtQuick 2.0

Rectangle {
    property string text
    property bool active: true

    color: "transparent"

    signal clicked()

    width: 250
    height: 60

    BorderImage {
        source: "qrc:/images/end.svg"
        anchors.fill: parent
        opacity: matchMouse.containsMouse ? 1 : 0.5
        Behavior on opacity { NumberAnimation { duration: 200 } }
        border { left: 25; top: 25; right: 25; bottom: 25 }
    }

    MouseArea {
        id: matchMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            parent.clicked()
        }
    }

    Text {
        id: title
        anchors.fill: parent
        anchors.leftMargin: 20
        verticalAlignment: Text.AlignVCenter
        text: parent.text
        color: active ? "white" : "gray"
    }
}
