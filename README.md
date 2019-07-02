# RetroArch Lite

Based on RetroArch 1.2, with updates for compatibility.
RetroArch Lite is a simpler, easier fork of RetroArch with touchscreen improvements.

Android and Win64 binaries are [here](https://drive.google.com/open?id=1QjhAOmM9OOP0JX0Me5I1eEbpbFsSZk9I).

### Notable Updates
* Scoped Settings: Individually override certain settings for a core/ROM/directory.
  * E.g. aspect ratio, input overlay, shader preset, etc.
* Overlay Features
  * Dpad and ABXY areas with 8-way direction input
  * Lightgun support (use a SNES_x.cfg overlay)
* Android Features
  * Combined 32/64-bit core lists if both apps are installed
  * Core backup & restore
  * Contact-area based input
  * Haptic feedback

### Control Notes
* Menu
  * In Directory Settings, "Use Loaded ROM's Dir" will quickset a core-specific browser directory.
  * 'Start' deletes a Core, Options file or Shader Preset, and deletes a Remap if "Save..." highlighted.
  * 'L'/'R' sets min/max value; in a few cases, jumps left/right by 10.
* Touchscreen: On most overlays, tapping the screen center toggles the menu.