# Native CRT pass

An optional plugin that runs the CRT inside Hyprland after windows and before top
and overlay layers, so toasts land flat on a curved tile. Nothing here is installed
by `crt install`; `native/build` and `crt native on` are the way in.

- `crt-pass.cpp` is the plugin, built against the installed headers by `build`.
- `sandbox` is a seat that cannot reach the live desktop: no display device, no
  input, no login bus, no live Wayland socket. Hyprland 0.56.2 has no headless-only
  switch, so this is what keeps a second compositor off the real screen.
- `rig/` starts Hyprland in that seat, builds the scene and checks it.

```bash
native/rig/scene /tmp/crt-seat
```
```bash
native/rig/prove /tmp/crt-seat /tmp/crt-seat/out
```
```bash
native/rig/prove-pointer /tmp/crt-seat /tmp/crt-seat/out
```

```bash
native/rig/matrix /tmp/crt-seat
```

`matrix` runs every proof on every monitor shape. Seat programs are non-dumpable, so
a crash in the seat can't reach the live desktop's crash watcher.
