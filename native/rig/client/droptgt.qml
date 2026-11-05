import QtQuick
import Quickshell

// A tile that is one big drop target, and reports where a drag reaches it.
ShellRoot {
  FloatingWindow {
    title: "grid"
    implicitWidth: 1500
    implicitHeight: 800
    color: "#14202a"
    DropArea {
      anchors.fill: parent
      onEntered: function(d) { console.log("ENTER tgt", d.x, d.y) }
      onPositionChanged: function(d) { console.log("OVER tgt", d.x, d.y) }
      onDropped: function(d) { d.accept(Qt.CopyAction); console.log("DROP tgt", d.x, d.y) }
    }
  }
}
