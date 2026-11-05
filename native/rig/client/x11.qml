import QtQuick
import QtQuick.Window

// target.qml's shape, as a plain window, so it can run over XWayland.
Window {
  title: "x11grid"
  width: 900
  height: 600
  visible: true
  color: "#14202a"
  MouseArea {
    anchors.fill: parent
    onClicked: function(m) { console.log("CLICK x11", m.x, m.y) }
  }
}
