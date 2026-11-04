import QtQuick
import Quickshell
import Quickshell.Wayland

// Translucent rounded cards on the overlay layer, under the name Omarchy's toasts
// use: the case a flat patch cannot do, because the tile has to show through.
ShellRoot {
  PanelWindow {
    anchors { top: true; bottom: true; left: true; right: true }
    color: "transparent"
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "omarchy-notifications"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    mask: Region { item: cards }
    Column {
      id: cards
      anchors { top: parent.top; right: parent.right; topMargin: 40; rightMargin: 40 }
      spacing: 10
      Repeater {
        model: 3
        Rectangle {
          required property int index
          width: 420; height: 78; radius: 14
          color: Qt.rgba(0.05, 0.05, 0.07, 0.62)
          border.color: "#f0e6c8"; border.width: 2
          Text { x: 18; y: 14; color: "#ffffff"; font.pixelSize: 19; font.bold: true; text: "Screenshot saved to clipboard" }
          Text { x: 18; y: 44; color: "#d8d8d8"; font.pixelSize: 15; text: "Flat card, translucent, over a curved tile" }
        }
      }
    }
  }
}
