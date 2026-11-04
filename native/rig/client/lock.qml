import QtQuick
import Quickshell
import Quickshell.Wayland

// A real session lock with a click target, the way Omarchy's lock is built.
ShellRoot {
  WlSessionLock {
    locked: true
    WlSessionLockSurface {
      color: "#203040"
      MouseArea {
        anchors.fill: parent
        onClicked: function(m) { console.log("CLICK lock", m.x, m.y) }
      }
    }
  }
}
