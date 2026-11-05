import QtQuick
import Quickshell

// A small window to drag from, like a drag helper showing one file. Small matters: its
// lens is sized to it, so a press lock here is at its worst over a far tile.
ShellRoot {
  FloatingWindow {
    title: "dragsrc"
    implicitWidth: 300
    implicitHeight: 160
    color: "#3a1f2a"
    Rectangle {
      id: thing
      width: 300
      height: 160
      color: "#ffd27f"
      Drag.active: area.drag.active
      Drag.dragType: Drag.Automatic
      Drag.supportedActions: Qt.CopyAction
      Drag.mimeData: { "text/plain": "crt" }
      MouseArea {
        id: area
        anchors.fill: parent
        drag.target: thing
        onPressed: console.log("PRESS src")
      }
    }
  }
}
