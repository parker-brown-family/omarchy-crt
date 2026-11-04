import QtQuick

// The power switch. A cap you push, standing proud of its well on a dark skirt,
// because the row this replaced was a label on a grey strip and nobody took it
// for a control. The cap travels down onto the skirt under your finger, and the
// lamp and the ring say which way the set is switched from across the room.
//
// Sized and themed from outside like Knob, and it imports nothing of the shell's,
// so a test can draw it on its own. It reports a click and owns no state: the
// switch is whatever live.json says the glass is.
Item {
  id: root

  property bool on: true
  property color tint: "#67F454"         // the phosphor: lamp, ring and legend when on
  property color foreground: "#B5BEAC"
  property color dim: Qt.darker(foreground, 1.55)
  property string fontFamily: "monospace"
  property bool held: mouse.pressed      // a test draws the pressed cap by setting this

  readonly property alias containsMouse: mouse.containsMouse
  readonly property color lit: on ? tint : dim

  signal clicked()

  implicitWidth: 116
  implicitHeight: 50

  readonly property real travel: 3
  readonly property real capHeight: height - 8 - travel

  function fg(a) { return Qt.rgba(foreground.r, foreground.g, foreground.b, a) }

  // The well the button sits in.
  Rectangle {
    anchors.fill: parent
    radius: 11
    color: Qt.rgba(0, 0, 0, 0.55)
    border.width: 1
    border.color: root.fg(0.16)
  }

  // The skirt: the side of the cap, showing only while it stands proud.
  Rectangle {
    x: 4
    y: 4 + root.travel
    width: root.width - 8
    height: root.capHeight
    radius: 8
    color: Qt.darker(root.fg(1), 4.2)
  }

  // The cap.
  Rectangle {
    id: cap
    x: 4
    y: root.held ? 4 + root.travel : 4
    width: root.width - 8
    height: root.capHeight
    radius: 8
    border.width: 1
    border.color: Qt.rgba(root.lit.r, root.lit.g, root.lit.b,
                          root.containsMouse ? 0.95 : (root.on ? 0.6 : 0.3))
    gradient: Gradient {
      GradientStop { position: 0; color: Qt.darker(root.fg(1), root.held ? 3.6 : (root.containsMouse ? 2.0 : 2.4)) }
      GradientStop { position: 1; color: Qt.darker(root.fg(1), root.held ? 3.0 : 3.6) }
    }

    Behavior on y { NumberAnimation { duration: 50 } }

    // The light catching the top edge. A pushed cap has none.
    Rectangle {
      x: parent.radius
      y: 1
      width: parent.width - parent.radius * 2
      height: 1
      color: root.fg(root.held ? 0 : 0.4)
    }

    Row {
      anchors.centerIn: parent
      spacing: 9

      // The lamp: a halo and a bead, dark when the set is off.
      Item {
        width: 14
        height: 14
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
          anchors.fill: parent
          radius: width / 2
          color: root.tint
          opacity: root.on ? 0.28 : 0
        }

        Rectangle {
          anchors.centerIn: parent
          width: 8
          height: 8
          radius: 4
          color: root.on ? root.tint : Qt.darker(root.foreground, 3.4)
          border.width: 1
          border.color: root.on ? Qt.lighter(root.tint, 1.4) : root.fg(0.25)
        }
      }

      // The IEC power symbol, drawn so it does not depend on a font having it.
      Canvas {
        id: glyph
        width: 18
        height: 18
        anchors.verticalCenter: parent.verticalCenter

        readonly property color ink: root.lit
        onInkChanged: requestPaint()

        onPaint: {
          var ctx = getContext("2d")
          ctx.reset()
          ctx.strokeStyle = ink
          ctx.lineWidth = 2
          ctx.lineCap = "round"
          ctx.beginPath()
          ctx.arc(9, 10, 6.5, -Math.PI / 2 + 0.75, -Math.PI / 2 - 0.75 + Math.PI * 2)
          ctx.stroke()
          ctx.beginPath()
          ctx.moveTo(9, 2)
          ctx.lineTo(9, 9)
          ctx.stroke()
        }
      }

      Text {
        anchors.verticalCenter: parent.verticalCenter
        text: root.on ? "ON" : "OFF"
        color: root.lit
        font.family: root.fontFamily
        font.pixelSize: 15
        font.bold: true
        font.letterSpacing: 1.5
      }
    }
  }

  MouseArea {
    id: mouse
    anchors.fill: parent
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    onClicked: root.clicked()
  }
}
