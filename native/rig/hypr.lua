-- The test seat's whole config. One output, no animation, nothing of Omarchy's.
hl.config({
  animations = { enabled = false },
  misc = { disable_hyprland_logo = true, disable_splash_rendering = true, force_default_wallpaper = 0, background_color = 0xff101820 },
  general = { gaps_in = 0, gaps_out = 20, border_size = 2 },
  decoration = { rounding = 0, shadow = { enabled = false }, blur = { enabled = false } },
})
-- A monitor with no screen has nothing asking it for frames, and a capture
-- waits for one, so the test seat redraws continuously.
hl.config({ debug = { damage_tracking = 0, disable_logs = false } })
-- The scene's floating window, by title.
hl.window_rule({ name = "crt-float", match = { title = "float" }, float = true, size = { 700, 420 }, move = { 300, 200 } })
