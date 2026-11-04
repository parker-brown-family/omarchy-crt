-- The test seat's whole config. One output, no animation, nothing of Omarchy's.
hl.monitor({ output = "HEADLESS-1", mode = "1600x900@60", position = "0x0", scale = 1 })
hl.config({
  animations = { enabled = false },
  misc = { disable_hyprland_logo = true, disable_splash_rendering = true, force_default_wallpaper = 0, background_color = 0xff101820 },
  general = { gaps_in = 0, gaps_out = 20, border_size = 0 },
  decoration = { rounding = 0, shadow = { enabled = false }, blur = { enabled = false } },
})
-- A monitor with no screen has nothing asking it for frames, and a capture
-- waits for one, so the test seat redraws continuously.
hl.config({ debug = { damage_tracking = 0, disable_logs = false } })
