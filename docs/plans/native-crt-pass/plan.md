# Plan: native CRT pass

## What it is for
Toasts and menus render flat and sharp above tiles that stay curved. Moving a tile stops freezing the desktop, the five-a-second cap goes, and Esc stops flashing.

## The shape
A Hyprland plugin runs the CRT after windows and before top and overlay layers, with tile rects and knobs as uniforms read each frame. Three hooks correct hit-testing so clicks land on what a curved tile shows. The screen shader stays as the fallback and the default: the plugin is optional, loaded by hand, and never written into the Hyprland config until Parker says so. `crt` stays the one owner and the tray stays the control surface.

## Least confident decisions
- Three function hooks are tied to Hyprland's internals. An update can rename them, so the plugin must refuse to load on any Hyprland but the one it was built for.
- A plugin can't go through Omarchy's plugin marketplace. It stays a separate, opt-in part of this repo, and the marketplace plugin keeps working without it.
- The optics currently read a frame copy. Bloom and aberration gather across tile edges, so the look may differ slightly from the screen shader's.
- Whether a still desktop can still draw no frames with this pass in the chain is unmeasured.

- Hyprland keeps an unloaded plugin mapped, so a rebuilt plugin only takes effect in a new session.

## Slices
1. **The whole look.** Port crt-glass.frag with knobs as uniforms: glow, bloom, tracking, flicker, scanlines, convergence, phosphor from the theme. Done when a sealed frame matches the screen shader's on the same scene, and turning a knob compiles nothing.
2. **The right pixels.** One stacking rule shared by the shader and the pointer: floating over tiled, fullscreen, Terminal Delight's own curve untouched, a window opted out. Done when each case has a pixel check and a click check.
3. **One owner and a way back.** `crt` chooses the plugin or the shader, never both; off, the tray button and the chords all work on either; a version mismatch falls back to the shader with a message. Done when an unload, a refused load and a crash each leave a working desktop in the seat.
4. **What can go wrong.** Drag across a toast's edge, scrolling, pointer lock, X11 apps, blur, lock screen, screenshots, recording, fullscreen video, hotplug, idle redraw, GPU cost against today's numbers. Each gets a sealed check or an honest "not covered".
5. **Your desktop, opt-in.** `crt native on` loads it into the running session, nothing is written to config, and a reboot or crash comes back on the shader. Parker tries it, and only then does it become a choice that persists.
