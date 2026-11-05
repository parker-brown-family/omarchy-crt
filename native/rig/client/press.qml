import QtQuick
import Quickshell

// A small window that reports where a held button goes, past its own edges too.
ShellRoot {
  FloatingWindow {
    title: "dragsrc"
    implicitWidth: 300
    implicitHeight: 160
    color: "#3a1f2a"
    MouseArea {
      anchors.fill: parent
      onPositionChanged: function(m) { console.log("HELD press", m.x, m.y) }
    }
  }
}
