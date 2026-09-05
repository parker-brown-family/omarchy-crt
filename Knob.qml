import QtQuick

// A knob you turn. Not a slider wearing a circle.
//
// The distinction is the whole point of this file. A slider maps a straight
// drag onto a value and gets drawn round; grab it anywhere and it jumps to
// where your finger is. A knob has no position to jump to — it has a shaft,
// and turning it moves the value by however far you turned, from wherever it
// already was. So this accumulates the ANGLE SWEPT between mouse samples
// rather than reading the absolute angle under the cursor, which also disposes
// of the discontinuity every absolute-angle dial has at the top of its travel.
//
// 270 degrees of sweep, -135 to +135, because that is what the sweep on a real
// front panel is: enough travel to be precise, with a flat at the bottom where
// the pointer never goes so you can read "off" at a glance.
Item {
  id: root

  property real value: 0                 // 0..1, always
  property string label: ""
  property string readout: ""            // when set, shown instead of a percentage
  property bool showReadout: true
  property color tint: "#67F454"         // the phosphor — face and pointer glow
  property color foreground: "#B5BEAC"
  property color dim: Qt.darker(foreground, 1.55)
  property string fontFamily: "monospace"
  property bool interactive: true
  property bool focused: false           // keyboard cursor is on this knob

  // Turning is continuous; committing is not. Every frame of a drag would be a
  // shader recompile, so the dial moves under your hand and the picture catches
  // up when you let go.
  signal committed(real v)

  readonly property real dialSize: Math.min(width, height - 30)

  implicitWidth: 84
  implicitHeight: 108

  function nudge(delta) {
    if (!root.interactive) return
    root.value = Math.max(0, Math.min(1, root.value + delta))
    root.committed(root.value)
  }

  Canvas {
    id: face
    width: root.dialSize
    height: root.dialSize
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.top: parent.top

    onPaint: {
      var ctx = getContext("2d")
      var w = width, h = height
      ctx.reset()
      ctx.clearRect(0, 0, w, h)

      var cx = w / 2, cy = h / 2
      var r = Math.min(w, h) / 2 - 2
      var a0 = -225 * Math.PI / 180          // value 0
      var sweep = 270 * Math.PI / 180
      var ang = a0 + root.value * sweep

      var lit = root.interactive ? 1.0 : 0.35

      // The travel arc, drawn behind everything: unlit for the whole sweep,
      // lit up to where the pointer is. This is the part you read from across
      // the room, before you can resolve the pointer at all.
      ctx.lineWidth = Math.max(2, r * 0.11)
      ctx.lineCap = "round"
      ctx.strokeStyle = Qt.rgba(root.dim.r, root.dim.g, root.dim.b, 0.5 * lit)
      ctx.beginPath()
      ctx.arc(cx, cy, r - ctx.lineWidth / 2, a0, a0 + sweep, false)
      ctx.stroke()

      if (root.value > 0.001) {
        ctx.strokeStyle = Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.9 * lit)
        ctx.beginPath()
        ctx.arc(cx, cy, r - ctx.lineWidth / 2, a0, ang, false)
        ctx.stroke()
      }

      // Knurling — the milled grip on the rim of the real thing. Ticks, not a
      // texture: at 60 logical pixels a texture is mush and ticks still read.
      var rk = r - ctx.lineWidth - 2
      ctx.strokeStyle = Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.22 * lit)
      ctx.lineWidth = 1
      for (var i = 0; i < 36; i++) {
        var ka = (i / 36) * Math.PI * 2
        ctx.beginPath()
        ctx.moveTo(cx + Math.cos(ka) * (rk - 3), cy + Math.sin(ka) * (rk - 3))
        ctx.lineTo(cx + Math.cos(ka) * rk, cy + Math.sin(ka) * rk)
        ctx.stroke()
      }

      // The cap. A vertical gradient reads as a cylinder catching the room
      // light from above, which is the only cue that says "this sticks out".
      var rc = rk - 4
      var g = ctx.createLinearGradient(0, cy - rc, 0, cy + rc)
      g.addColorStop(0, Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.30 * lit))
      g.addColorStop(0.52, Qt.rgba(root.tint.r * 0.30, root.tint.g * 0.30, root.tint.b * 0.30, 0.90 * lit))
      g.addColorStop(1, Qt.rgba(0, 0, 0, 0.80 * lit))
      ctx.fillStyle = g
      ctx.beginPath()
      ctx.arc(cx, cy, rc, 0, Math.PI * 2)
      ctx.fill()

      ctx.strokeStyle = Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.35 * lit)
      ctx.lineWidth = 1
      ctx.beginPath()
      ctx.arc(cx, cy, rc, 0, Math.PI * 2)
      ctx.stroke()

      // The pointer.
      ctx.strokeStyle = Qt.rgba(root.tint.r, root.tint.g, root.tint.b, lit)
      ctx.lineWidth = Math.max(2, r * 0.09)
      ctx.lineCap = "round"
      ctx.beginPath()
      ctx.moveTo(cx + Math.cos(ang) * rc * 0.18, cy + Math.sin(ang) * rc * 0.18)
      ctx.lineTo(cx + Math.cos(ang) * rc * 0.82, cy + Math.sin(ang) * rc * 0.82)
      ctx.stroke()

      if (root.focused) {
        ctx.strokeStyle = Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.55)
        ctx.lineWidth = 1
        ctx.beginPath()
        ctx.arc(cx, cy, r + 1, 0, Math.PI * 2)
        ctx.stroke()
      }
    }
  }

  onValueChanged: face.requestPaint()
  onTintChanged: face.requestPaint()
  onFocusedChanged: face.requestPaint()
  onForegroundChanged: face.requestPaint()
  onInteractiveChanged: face.requestPaint()

  MouseArea {
    anchors.fill: face
    enabled: root.interactive
    hoverEnabled: true
    cursorShape: root.interactive ? Qt.SizeVerCursor : Qt.ArrowCursor
    acceptedButtons: Qt.LeftButton

    property bool turning: false
    property real lastAngle: 0

    function angleAt(x, y) {
      return Math.atan2(y - face.height / 2, x - face.width / 2) * 180 / Math.PI
    }

    onPressed: function (mouse) {
      turning = true
      lastAngle = angleAt(mouse.x, mouse.y)
      root.focused = true
    }

    onPositionChanged: function (mouse) {
      if (!turning) return
      var a = angleAt(mouse.x, mouse.y)
      var d = a - lastAngle
      // Shortest way round, so crossing the top of the travel does not spin
      // the value a full turn the wrong way.
      while (d > 180) d -= 360
      while (d < -180) d += 360
      lastAngle = a
      root.value = Math.max(0, Math.min(1, root.value + d / 270))
    }

    onReleased: {
      if (!turning) return
      turning = false
      root.committed(root.value)
    }

    onCanceled: turning = false

    // A wheel over a knob is the fine adjustment, and it commits each detent —
    // there is no "release" to wait for.
    onWheel: function (wheel) {
      root.nudge((wheel.angleDelta.y > 0 ? 1 : -1) * 0.02)
    }
  }

  Text {
    id: labelText
    anchors.top: face.bottom
    anchors.topMargin: 4
    anchors.horizontalCenter: parent.horizontalCenter
    text: root.label
    color: root.focused ? root.foreground : root.dim
    font.family: root.fontFamily
    font.pixelSize: 10
    font.letterSpacing: 1.4
  }

  Text {
    anchors.top: labelText.bottom
    anchors.horizontalCenter: parent.horizontalCenter
    visible: root.showReadout
    text: root.readout !== "" ? root.readout : Math.round(root.value * 100) + "%"
    color: root.tint
    font.family: root.fontFamily
    font.pixelSize: 10
    opacity: root.interactive ? 0.85 : 0.4
  }
}
