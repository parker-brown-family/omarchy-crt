import QtQuick
import Quickshell

// A ruled tile with targets near its corners. It reports where every click lands,
// in its own coordinates, so a test can compare that with what was on screen.
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
        c.strokeStyle = "#ff5a3c"; c.fillStyle = "#ff5a3c"; c.lineWidth = 3
        var T = [[110, 90], [110, 770], [1450, 770], [780, 430]]
        for (var i = 0; i < T.length; i++) {
          c.beginPath(); c.arc(T[i][0], T[i][1], 18, 0, 6.3); c.stroke()
          c.fillRect(T[i][0] - 2, T[i][1] - 2, 4, 4)
        }
      }
    }
    MouseArea {
      anchors.fill: parent
      onClicked: function(m) { console.log("CLICK tile", m.x, m.y) }
    }
  }
}
