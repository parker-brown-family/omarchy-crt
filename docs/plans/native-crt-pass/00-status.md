# Status: native CRT pass

**Difficulty: 8/10** — code inside the compositor can take the whole desktop down, and every Hyprland update can break it → four gates, collapsed to one plan on 2026-11-04.
**Turned out to be:** 8/10, the score held. What moved it was the seat rather than the code: core dumps from sealed runs raised real crash toasts on the live desktop, and the unload crash and the 40 px lock-screen clicks were both faults that only a compositor-side pass can have.

The gates collapsed because the two approvals so far were "Keep going! <3" and "keep going, losing the scanlines on the bar is fine": the sealed proofs answered the product and architecture questions before a gate doc existed, so four rounds would buy nothing. One approval of `plan.md` stands in for all four. The rule is in the `software-factory` skill.

- Plan: APPROVED 2026-11-04 ("Ggogogo")
- Nothing runs on the live desktop until the plan is approved and slice 5 is reached.

## Slices
- [x] Proof: toasts flat over a curved tile
- [x] Proof: clicks land on what a curved tile shows
- [x] Proof: plugin unloads cleanly
- [x] Slice 1 — the whole CRT look, with knobs that change live
- [x] Slice 2 — windows own the right pixels
- [x] Slice 3 — one owner, an off switch and a way back
- [ ] Slice 4 — everything that can go wrong (done: drag, scroll, lock, idle, GPU cost, workspace slide, blur, the float behind a fullscreen window, the border corner in the seat. Never run: X11, recording, hotplug, special workspaces, groups, pins. Pointer lock has a guard and no real game behind it. The corner still waits on Parker's eye, in issue 9. An adversarial review before the 0.2.0 marketplace update found that a second load of one build killed Hyprland; that and nine others were fixed, with no second review round. A plugin that fails halfway through its hooks at load is still untested)
- [x] Slice 5 — your desktop, opt-in: running since 2026-11-04, Parker: "really nice"

## Notes for a fresh session
Parker ruled on 2026-11-04 that losing scanlines on the bar is fine. The same is assumed for toasts and menus, because they are drawn after the CRT too. He has not said so about those two.

The code is on `main`, under `native/`. `./test/all` is what to run before a push, and `native/rig/matrix` is the GPU proof CI cannot run. The rig's traps are in the project memory `sealed-hyprland-test-seat`.
