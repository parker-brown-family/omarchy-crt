import QtQuick
import QtQuick.Controls
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

// The linter cannot see Quickshell's C++ type registration nor the dynamic
// members of the theme singletons (Style.font.*, Color.* read as missing).
// Same blind spot the first-party panels hit; every other check still runs.
// qmllint disable uncreatable-type missing-property unqualified

// Delight-O-Matic — the front panel of the desktop's cathode ray tube.
//
// One bar icon and one tray. The icon is the set; the tray is the front of it,
// with the four knobs a real one had. BARREL bends the glass, FADE is the
// phosphor wash, TRACKING is the band that rolls down the picture, and CHANNEL
// picks which phosphor you are tuned to — including AUTO, which takes its hue
// from whatever Omarchy theme is staged. That last one is why this is a plugin
// and not a theme: the optics belong to the set, the colour belongs to the
// picture, and they were welded together for no reason anyone could name.
//
// This file is a pure display and a command line. It never writes the shader.
// `crt` renders that from its own template plus knobs.json, and publishes what
// it actually compiled into live.json — which is the ONLY thing read here, so
// the dials show the state of the glass rather than the state of a wish.
Panel {
  id: root
  moduleName: "brownfamilysports.crt"
  ipcTarget: "brownfamilysports.crt"
  manageIpc: false

  readonly property color foreground: bar ? bar.foreground : Color.foreground
  readonly property color dim: Qt.darker(foreground, 1.55)
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family

  readonly property string home: Quickshell.env("HOME") || ""
  readonly property string stateDir:
    (Quickshell.env("XDG_STATE_HOME") || (home + "/.local/state")) + "/omarchy/crt"

  readonly property bool showPercent: setting("showPercent", true)
  readonly property bool autoInstall: setting("autoInstall", true)

  // Where this plugin was installed. `omarchy plugin add` clones the whole
  // repository, so the CLI half sits under crt/ right here — no second path to
  // configure, and no guess at the install directory's name.
  readonly property string pluginDir: {
    var u = Qt.resolvedUrl(".").toString()
    return u.replace(/^file:\/\//, "").replace(/\/$/, "")
  }

  // ------------------------------------------------------------------ model

  property var live: ({})
  property bool installed: false
  property bool triedInstall: false

  readonly property var panelKnobs: (live && live.panel) ? live.panel : ({})
  readonly property var consts: (live && live.consts) ? live.consts : ({})
  readonly property string channel: (live && live.channel) ? live.channel : "auto"
  readonly property bool glassOn: !live || live.on !== false
  readonly property bool animated: !!(live && live.animated)

  // The phosphor the shader was actually compiled with — not the theme's
  // accent, not a guess. When AUTO resolved against a staged theme these are
  // the same colour; when no theme was staged they are deliberately not, and
  // the tray should show what is on the screen.
  readonly property color phosphor:
    (live && live.phosphor) ? live.phosphor : foreground

  readonly property var channels: ["auto", "green", "amber", "white"]
  readonly property int channelIndex: Math.max(0, channels.indexOf(channel))

  // The six service knobs that are natural 0..1 dials. The rest of the tube —
  // SCAN_STEP, BAND_H, TRACK_PERIOD, the colour grade — has ranges a knob would
  // lie about, so those stay on the command line rather than being squeezed
  // onto a dial that reads 40% and means four pixels.
  readonly property var serviceKnobs: [
    { key: "SCAN",     label: "SCANLINE" },
    { key: "ABERR",    label: "CONVERGE" },
    { key: "FLICKER",  label: "FLICKER" },
    { key: "GLARE",    label: "GLARE" },
    { key: "SPECULAR", label: "ROOM LIGHT" },
    { key: "JIGGLE",   label: "JIGGLE" }
  ]

  function panelValue(key, fallback) {
    var v = panelKnobs[key]
    return (typeof v === "number") ? v : fallback
  }

  function constValue(key) {
    var v = consts[key]
    return (typeof v === "number") ? Math.max(0, Math.min(1, v)) : 0
  }

  readonly property string heroMeta: {
    if (!installed) return "not wired into Hyprland yet"
    var parts = [glassOn ? "glass on" : "glass lifted", "channel " + channel]
    if (animated) parts.push("clock running")
    return parts.join("  ·  ")
  }

  // ----------------------------------------------------------------- action

  // Values reach a command line. They are generated here rather than read from
  // anywhere, but the shell string is built by concatenation, so they are
  // checked on the way out instead of trusted for looking right.
  function safeNumber(v) {
    return typeof v === "number" && isFinite(v)
  }

  function crt(args) {
    if (!root.bar) return
    root.bar.run("'" + pluginDir + "/crt/crt' " + args)
  }

  function setKnob(name, value) {
    if (!safeNumber(value) || !/^[A-Z_0-9]+$/.test(name)) return
    crt("set " + name + " " + value.toFixed(3))
  }

  function setChannel(index) {
    var i = Math.max(0, Math.min(channels.length - 1, Math.round(index)))
    crt("channel " + channels[i])
  }

  function toggleGlass() {
    crt(glassOn ? "off" : "on")
  }

  // ------------------------------------------------------------ keyboard nav

  property int cursor: 0
  readonly property int knobCount: 4 + (serviceOpen ? serviceKnobs.length : 0)
  property bool serviceOpen: false

  function moveCursor(delta) {
    cursor = Math.max(0, Math.min(knobCount - 1, cursor + delta))
  }

  function turnCursor(delta) {
    if (cursor === 3) { setChannel(channelIndex + (delta > 0 ? 1 : -1)); return }
    if (cursor < 3) {
      var names = ["BARREL", "FADE", "TRACKING"]
      var n = names[cursor]
      setKnob(n, Math.max(0, Math.min(1, panelValue(n, 0.5) + delta * 0.05)))
      return
    }
    var s = serviceKnobs[cursor - 4]
    if (s) setKnob(s.key, Math.max(0, Math.min(1, constValue(s.key) + delta * 0.05)))
  }

  // ------------------------------------------------------------------- bar

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  onOpenedChanged: if (opened) cursor = 0

  // ------------------------------------------------------------------ state

  FileView {
    id: liveFile
    path: root.stateDir + "/live.json"
    watchChanges: true
    printErrors: false
    onFileChanged: reload()
    onLoaded: root.parse(text())
    onLoadFailed: {
      root.lastText = ""
      root.installed = false
      root.maybeInstall()
    }
  }

  property string lastText: ""

  function parse(content) {
    var raw = String(content || "")
    if (raw === root.lastText) return
    root.lastText = raw
    try {
      root.live = JSON.parse(raw) || {}
      root.installed = true
    } catch (e) {
      console.warn("crt", "ignoring unreadable live state", e)
      root.live = {}
      root.installed = false
    }
  }

  // A first run has no live.json because nothing has rendered yet. Wiring the
  // shader in is a one-time act and it is what the user asked for by adding the
  // widget, so it happens once, on its own, and never again in this session
  // whatever the outcome — a failed install that retried every three seconds
  // would be a loop nobody can see and nobody asked for.
  function maybeInstall() {
    if (!autoInstall || triedInstall || !root.bar) return
    triedInstall = true
    crt("install")
  }

  // A FileView whose parent directory does not exist when it is created never
  // sees the file appear: watchChanges copes with the file being absent, and
  // with a later atomic replace, but has nothing to attach to when the
  // directory is missing too, and it does not retry. That is the normal first
  // install, not an edge case — so the tick re-reads, and parse() returns early
  // when nothing changed.
  Timer {
    interval: 3000
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: liveFile.reload()
  }

  IpcHandler {
    target: "brownfamilysports.crt"

    function refresh(): void { liveFile.reload() }
    function open(): void { root.open() }
    function close(): void { root.close() }
    function toggle(): void { root.toggle() }
    function glass(): void { root.toggleGlass() }
  }

  // ------------------------------------------------------------- bar button

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    // md-desktop_classic (U+F07C0) — a CRT monitor on a tower, which is the
    // object this plugin is about. Verified present in the bar's Nerd Font
    // (fc-list ':charset=f07c0'); an absent glyph leaves a blank slot on the
    // bar with no error anywhere to explain it. md-television_classic
    // (U+F07DD) is the other honest choice if you want the TV instead.
    text: "󰟀"
    active: root.installed && root.glassOn
    tooltipText: root.installed
      ? ("CRT — " + root.heroMeta)
      : "CRT — click to wire the glass into Hyprland"
    onPressed: function (buttonCode) {
      // Right-click is the one thing worth doing without reading the tray:
      // take the glass off, look at the flat picture, put it back.
      if (buttonCode === Qt.RightButton && root.installed) root.toggleGlass()
      else root.toggle()
    }
  }

  // ------------------------------------------------------------------- tray

  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    focusTarget: keyCatcher
    contentWidth: panel.fittedContentWidth(Style.space(400))
    contentHeight: panel.fittedContentHeight(column.implicitHeight, Style.space(620))

    PanelKeyCatcher {
      id: keyCatcher
      anchors.fill: parent

      onMoveRequested: function (dx, dy) {
        if (dx !== 0) root.moveCursor(dx)
        if (dy !== 0) root.turnCursor(-dy)     // up turns clockwise
      }
      onActivateRequested: root.toggleGlass()
      onReturnRequested: root.toggleGlass()
      onCloseRequested: root.close()
      onTabRequested: function (direction) { root.switchPanel(direction) }

      Flickable {
        id: panelFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        interactive: contentHeight > height
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
          id: column
          width: panelFlick.width
          spacing: Style.space(10)

          PanelHero {
            width: parent.width
            title: "Delight-O-Matic"
            meta: root.heroMeta
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          PanelSeparator {
            width: parent.width
            foreground: root.foreground
          }

          // ------------------------------------------------------ front panel

          Row {
            width: parent.width
            spacing: 0
            visible: root.installed

            Repeater {
              model: [
                { key: "BARREL",   label: "BARREL",   fallback: 0.55 },
                { key: "FADE",     label: "FADE",     fallback: 0.64 },
                { key: "TRACKING", label: "TRACKING", fallback: 0.60 }
              ]

              Knob {
                required property var modelData
                required property int index

                width: column.width / 4
                label: modelData.label
                tint: root.phosphor
                foreground: root.foreground
                fontFamily: root.fontFamily
                showReadout: root.showPercent
                focused: root.cursor === index
                value: root.panelValue(modelData.key, modelData.fallback)
                onCommitted: function (v) { root.setKnob(modelData.key, v) }
              }
            }

            // CHANNEL is detented, because a phosphor is a choice and not a
            // quantity: it snaps to a position and reads out its name. AUTO
            // sits at the bottom of the travel, where the set was tuned to
            // whatever the aerial was pointing at.
            Knob {
              id: channelKnob
              width: column.width / 4
              label: "CHANNEL"
              tint: root.phosphor
              foreground: root.foreground
              fontFamily: root.fontFamily
              readout: root.channel.toUpperCase()
              focused: root.cursor === 3
              value: root.channelIndex / (root.channels.length - 1)
              onCommitted: function (v) {
                root.setChannel(v * (root.channels.length - 1))
                // Snap back to the detent immediately; the file watch will
                // confirm it a moment later.
                value = Qt.binding(function () {
                  return root.channelIndex / (root.channels.length - 1)
                })
              }
            }
          }

          // --------------------------------------------------------- power

          Rectangle {
            width: parent.width
            height: Style.space(40)
            radius: Style.space(6)
            visible: root.installed
            color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.06)

            Text {
              anchors.left: parent.left
              anchors.leftMargin: Style.space(12)
              anchors.verticalCenter: parent.verticalCenter
              text: root.glassOn ? "GLASS ON" : "GLASS LIFTED"
              color: root.glassOn ? root.phosphor : root.dim
              font.family: root.fontFamily
              font.pixelSize: Style.font.body
              font.letterSpacing: 1.2
            }

            Text {
              anchors.right: parent.right
              anchors.rightMargin: Style.space(12)
              anchors.verticalCenter: parent.verticalCenter
              text: root.glassOn ? "click to lift" : "click to lower"
              color: root.dim
              font.family: root.fontFamily
              font.pixelSize: Style.font.caption
            }

            MouseArea {
              anchors.fill: parent
              cursorShape: Qt.PointingHandCursor
              onClicked: root.toggleGlass()
            }
          }

          // The one cost worth stating in the UI rather than in a README:
          // Hyprland turns damage tracking off for any shader that declares a
          // time uniform, so motion means the whole screen redraws every frame.
          // TRACKING at zero is what turns the clock back off.
          Text {
            width: parent.width
            visible: root.installed && root.animated
            text: "The clock is running, so Hyprland redraws the whole screen every "
                + "frame instead of just what changed. TRACKING, FLICKER and JIGGLE "
                + "at zero stop it."
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            wrapMode: Text.WordWrap
          }

          // ------------------------------------------------------- service

          PanelSeparator {
            width: parent.width
            foreground: root.foreground
            visible: root.installed
          }

          Item {
            width: parent.width
            height: Style.space(22)
            visible: root.installed

            Text {
              anchors.left: parent.left
              anchors.verticalCenter: parent.verticalCenter
              text: (root.serviceOpen ? "▾  " : "▸  ") + "SERVICE PANEL"
              color: root.dim
              font.family: root.fontFamily
              font.pixelSize: Style.font.caption
              font.letterSpacing: 1.2
            }

            MouseArea {
              anchors.fill: parent
              cursorShape: Qt.PointingHandCursor
              onClicked: root.serviceOpen = !root.serviceOpen
            }
          }

          Grid {
            width: parent.width
            visible: root.installed && root.serviceOpen
            columns: 4
            spacing: 0

            Repeater {
              model: root.serviceKnobs

              Knob {
                required property var modelData
                required property int index

                width: column.width / 4
                label: modelData.label
                tint: root.phosphor
                foreground: root.foreground
                fontFamily: root.fontFamily
                showReadout: root.showPercent
                focused: root.cursor === 4 + index
                value: root.constValue(modelData.key)
                onCommitted: function (v) { root.setKnob(modelData.key, v) }
              }
            }
          }

          Text {
            width: parent.width
            visible: root.installed && root.serviceOpen
            text: "The rest of the tube has ranges a dial would lie about — scanline "
                + "pitch in pixels, band height, sweep seconds, the colour grade. "
                + "Those are `crt set SCAN_STEP 3`, and `crt` on its own lists them."
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            wrapMode: Text.WordWrap
          }

          // ------------------------------------------------------ first run

          Text {
            width: parent.width
            visible: !root.installed
            text: root.autoInstall
              ? "Wiring the glass into Hyprland — this writes ~/.config/hypr/crt.lua "
                + "and one require() line, then renders the shader. A moment."
              : "Not wired in yet. Run `crt install` from the plugin directory, or "
                + "turn on \"Wire the shader into Hyprland\" in this widget's settings."
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            wrapMode: Text.WordWrap
          }
        }
      }
    }
  }
}
