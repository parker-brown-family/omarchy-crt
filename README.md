# Delight-O-Matic

Curved glass, scanlines and a phosphor glow over the whole Omarchy desktop —
on **any** theme, with four knobs on the bar.

![the front panel](docs/tray.png)

A CRT filter is a **lens**, and a lens is not a palette. This one used to ship
inside the `terminal-delight` theme, which meant you could own a curved monitor
only if you agreed to wear green: catppuccin, gruvbox, tokyo-night, nord and
every other theme were shut out of it for no technical reason anybody could
name. Twenty-two of the shader's twenty-four constants are optics — curvature,
aberration, scanline pitch, bloom, vignette, tracking, flicker, gamma — and
optics have no opinion about what colour your desktop is.

The two that *are* colours, `PHOSPHOR` and `GLARE_TINT`, now come from whatever
theme is staged. So gruvbox gets an amber tracking band and catppuccin a mauve
one, and both look right, because tinting the phosphor to the picture is what
phosphor tint is for.

## Install

```bash
omarchy plugin add https://github.com/parker-brown-family/omarchy-crt
```

Add **Delight-O-Matic** to your bar, then click the monitor icon. It wires
itself into Hyprland on first open — one file (`~/.config/hypr/crt.lua`) and one
`require()` line — or run `crt/crt install` yourself if you would rather watch.

Nothing else is needed. No packages, no compositor patches. If you want the
knob turns validated before they reach the screen, install `glslangValidator`
(`pacman -S glslang`); without it the renders go out unchecked and say so.

## The front panel

| Knob | What it turns |
|---|---|
| **BARREL** | how far the glass bows — `TUBE_K1`, `TUBE_K2` |
| **FADE** | the phosphor wash — `BLOOM`, `GLOW`, `VIGN` |
| **TRACKING** | the band that rolls down the picture |
| **CHANNEL** | which phosphor: `AUTO`, green (P1), amber (P3), white (P4) |

Grab a knob and turn it — it accumulates the angle you sweep, from wherever the
knob already is, the way a knob does. The wheel is the fine adjustment. Arrow
keys work when the tray has focus: left and right pick a knob, up and down turn
it.

Behind **SERVICE PANEL** are six more dials — scanline depth, convergence,
flicker, glare, room light, jiggle. The rest of the tube has ranges a dial
would lie about (scanline pitch in pixels, band height, sweep seconds, the
colour grade), so those live on the command line.

## The command line

```bash
crt
```
```bash
crt set BARREL 0.7
```
```bash
crt set SCAN 0.35 SCAN_STEP 3
```
```bash
crt channel amber
```
```bash
crt off
```

A knob you set by hand outranks the macro that would otherwise cover it, until
you turn that macro again. `crt` on its own prints both tiers and where every
file lives.

## How it works, and the two things worth knowing

**Hyprland screen shaders take no uniforms**, and there is no IPC to set one. So
changing a setting means rewriting the shader's `const` block and recompiling —
a few milliseconds, no relogin. The mistake worth not repeating is doing that
rewrite *in place*: the file then carries both the defaults and your settings
and the two can no longer be told apart. Here the template under `crt/` is
read-only, your settings live in `$XDG_STATE_HOME/omarchy/crt/knobs.json`, and
the shader at `~/.config/hypr/shaders/crt-glass.frag` is a build output you can
delete at any time.

That is also why there is no rule about not committing your window layout. The
tube data is runtime state, it lives in state, and there is no path from a
window being open to a line in a commit.

**Motion costs damage tracking.** Hyprland treats any screen shader that
*declares* `uniform float time` as animated — the warning keys off the
declaration, not the use — and switches damage tracking off for it, so the whole
screen redraws every frame instead of just what changed. The still glass
therefore compiles no clock at all. `crt` raises the clock when TRACKING,
FLICKER or JIGGLE go above zero and drops it when all three are back down, and
the tray says so while it is running.

## Where the pieces are

| | |
|---|---|
| `Panel.qml` | the bar icon and the tray |
| `Knob.qml` | one dial — canvas face, angular drag, wheel |
| `crt/crt-glass.frag` | the template, never written to |
| `crt/crt` | render, knobs, channels, install |
| `test/run` | 45 assertions in a sandbox, with `hyprctl` stubbed |

## What this does not do yet

The **per-window warp** — the per-tile tubes that bow each window separately and
stay click-correct — still lives in `omarchy-terminal-delight-theme`'s
`install-curve.sh`, and Terminal Paint detects it as `crtAvailable`. This plugin
owns the monitor pass, which is the whole-desktop glass; moving the tubes across
is the next piece, and `crt render` already splices a tube block from
`$XDG_STATE_HOME/omarchy/crt/tubes.glsl` when one is there.

The optics are a port of [terminal-delight](https://github.com/parker-brown-family/terminal-delight)'s
own display stack, dial for dial.

MIT.
