# Status: native CRT pass

**Difficulty: 8/10** — code inside the compositor can take the whole desktop down, and every Hyprland update can break it → four gates, collapsed to one plan on 2026-11-04.
**Turned out to be:** not known yet.

The gates collapsed because the two approvals so far were "Keep going! <3" and "keep going, losing the scanlines on the bar is fine": the sealed proofs answered the product and architecture questions before a gate doc existed, so four rounds would buy nothing. One approval of `plan.md` stands in for all four. The rule is in the `software-factory` skill.

- Plan: APPROVED 2026-11-04 ("Ggogogo")
- Nothing runs on the live desktop until the plan is approved and slice 5 is reached.

## Slices
- [x] Proof: toasts flat over a curved tile
- [x] Proof: clicks land on what a curved tile shows
- [x] Proof: plugin unloads cleanly
- [ ] Slice 1 — the whole CRT look, with knobs that change live
- [ ] Slice 2 — windows own the right pixels
- [ ] Slice 3 — one owner, an off switch and a way back
- [ ] Slice 4 — everything that can go wrong, in the sealed seat
- [ ] Slice 5 — your desktop, opt-in

## Notes for a fresh session
Parker ruled on 2026-11-04 that losing scanlines on the bar is fine. The same is assumed for toasts and menus, because they are drawn after the CRT too. He has not said so about those two.

The code is on branch `native-pass-proof`, under `native/`. The rig's traps are in the project memory `sealed-hyprland-test-seat`.
