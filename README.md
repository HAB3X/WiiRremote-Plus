# WiiRremote Plus

A Wii homebrew app that turns the Wii sensor bar into a TV remote, with a guided CRT calibration suite.

Made by Habex. Based on [WiiRremote](https://github.com/Abdelali221/WiiRremote) by Abdelali221. Licensed under GPL-3.0.

## Features

- **TV remote:** saved remotes, on-screen remote, button finder, code scanner (address first, group testing), macros, favourites, sleep timer, notes.
- **Smart match:** checks the buttons you mapped, identifies unnamed ones, finds the right code for wrong ones, then looks for more. Library of 929 TV code sets (irdb, Flipper-IRDB, LIRC, JP1 research).
- **Calibration Suite:** colour, grey levels, geometry, sharpness, and stability and signal. Guided, Intense (question-based diagnosis) and Ultra handhold (uses your service menu numbers) modes. 40 test pictures.
- Wii Remote, Nunchuk, Classic, Wii U Pro, GameCube, Guitar, Balance Board, USB keyboard and mouse.

> Service menus can damage a TV. Write every number down before changing it.

## Install

Copy `apps/WiiRremotePlus/` to the `apps` folder on your SD card and launch it from the Homebrew Channel. Needs the wired sensor bar.

## Build

Needs [devkitPro](https://devkitpro.org/) with devkitPPC and libogc (`wii-dev`).

```
make
```

This produces `WiiRremotePlus.dol`. Rename it to `boot.dol` and put it in `apps/WiiRremotePlus/`.

### PC preview and tests

- `web/build_local.sh` builds a browser preview (needs clang with a wasm32-wasi sysroot).
- `host/` has test drivers that run the app on a PC with scripted input (`host/rt.sh <test>`).
- `tools/gen_presets.py` rebuilds the TV code library.

## Credits

See [apps/WiiRremotePlus/CREDITS.txt](apps/WiiRremotePlus/CREDITS.txt).

## Releases

Each GitHub release has `WiiRremotePlus-<version>.zip`, with a `WiiRremotePlus/` folder holding `boot.dol`, `meta.xml`, `icon.png` and `CREDITS.txt`. `osc/wiirremoteplus.oscmeta` is the Open Shop Channel manifest that points at these releases.
