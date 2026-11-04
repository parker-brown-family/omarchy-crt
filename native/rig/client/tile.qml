import QtQuick
import Quickshell

// One tile of ruled grid, so any bend shows.
ShellRoot {
  FloatingWindow {
    title: "grid"
    implicitWidth: 1500
    implicitHeight: 800
    color: "#14202a"
    Canvas {
      anchors.fill: parent
      onPaint: {
        var c = getContext("2d")
        c.fillStyle = "#14202a"; c.fillRect(0, 0, width, height)
        c.strokeStyle = "#7fd0ff"; c.lineWidth = 2
        for (var x = 0; x <= width; x += 50) { c.beginPath(); c.moveTo(x, 0); c.lineTo(x, height); c.stroke() }
        for (var y = 0; y <= height; y += 50) { c.beginPath(); c.moveTo(0, y); c.lineTo(width, y); c.stroke() }
      }
    }
  }
}
