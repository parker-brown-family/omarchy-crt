# HANDOFF — ignition / on-off animation groundwork

> **2026-09-08 — the per-window half of this was built, shipped, and then
> removed.** Parker ruled the open/close window animation a no-fly, and the
> whole ignition feature (shader arc, fx state, watch wiring, toggle, probes)
> came out in the commit that carries this note. Do not rebuild it without a
> new decision. The measurements below remain valid and worth keeping — the
> per-apply clock reset, the latched banner, the damage-tier costs — and the
> GLASS's own on/off transition described here was never built and never
> vetoed.

For the agent building the power-on animation. Everything below was derived or
measured in the session that built this plugin (2026-09-04/05); take it rather
than re-digging.

## The clock, and how to stamp a birth time

`time` in a Hyprland screen shader is seconds since the compositor started.
There is no uniform for "when was this block baked", so a per-tube birth time
must be baked as a constant IN THE SHADER'S OWN TIMEBASE. The epoch is
recoverable: `/proc/$(pidof Hyprland)/stat` field 22 is start time in jiffies
(divide by `getconf CLK_TCK`), against `/proc/uptime`. Wall-to-shader time is
then `now_uptime - hyprland_start_uptime`, good to ~10 ms. Bake
`TUBE_BORN[i]` a beat in the future (+0.1 s) so the whole ramp plays instead
of starting mid-way.

## What is possible, and the half that is not

- A tube APPEARING (openwindow / openlayer / workspace switch): fully
  animatable — the content exists, the watcher knows the moment, the shader
  can ramp the tube from a scanline. This is most of the delight.
- The GLASS's own on/off: fully animatable both directions (the windows are
  not going anywhere, only the effect). Prefer a parameter relax — curve,
  scanlines, vignette easing to zero — over a collapse: a collapse that ends
  with the content restored flat reads wrong.
- A window CLOSING: the pixels leave the framebuffer with it; you cannot
  animate what is no longer drawn. The one slim maybe is riding Hyprland's own
  windowsOut close animation, which keeps a snapshot on screen briefly —
  unproven here, and the two animations would stack.

## Costs, already wired — do not rewire

- Any animation needs the clock: bake `ANIMATED 1` for the transition, and the
  existing `preconditions()` moves `debug:damage_tracking` to tier 0 and back
  automatically. Tier logic lives in ONE place; extend, do not fork.
- Hyprland's red time-uniform banner LATCHES; `apply()` already presses
  `hyprctl seterror disable` after clock-on swaps. A transient clock will
  raise-and-clear; that is expected.
- The transition should be: bake start-time + ANIMATED once, ONE swap, let the
  shader animate itself, then one final still bake after the duration (a
  watcher timer). Never re-bake per frame — a swap per frame is the flash bug
  this repo already buried once.

## Invariants that the suite pins (keep it green: ./test/run)

- `crt/crt-glass.frag` is a TEMPLATE and is never written to; render() owns
  the output in `$XDG_STATE_HOME/omarchy/crt/`.
- Tube rects are spliced only while a live watcher pidfile vouches for them.
- New per-tube arrays (e.g. TUBE_BORN) need: the zeros-fallback in render()
  for blocks that predate them, inclusion in `_tube_rows`/`block_key` ONLY if
  a change should trigger a rebuild (a birth time changes every bake — keep it
  OUT of the key or every event recompiles), and a `TUBE_COUNT 0` declaration
  in the template's `#if` block.
- `swap_in()` alternates two slot files so there is never a flat frame.
- Every apply logs `crt: apply HH:MM:SS animated=… on=… tubes=…` — keep the
  line; it is the only forensic record the red banner has.

## Repo state when this was written

Public-ready pass done, everything committed on `main`, push withheld while
your work lands. `reports/` is untracked as of this commit — it was being
swept into unrelated commits by `git add -A`; your specs stay on disk, out of
the public artifact.
