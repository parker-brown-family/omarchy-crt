# Native CRT pass: proof of concept

Not shipped, and not for the live desktop. This shows that the CRT can run inside
Hyprland after windows and before top and overlay layers, so toasts land flat on a
curved tile. Nothing here is installed by `crt install`.

- `crt-pass.cpp` is a Hyprland plugin, built against the installed headers.
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

Unloading the plugin from a running Hyprland crashes it (seen 2026-11-04, cause not
diagnosed), so restart the seat rather than reloading.

Run on an NVIDIA box. `rig/headless-gpu.cpp` hardcodes the render node path.
