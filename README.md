# KaliSpout2

<p align="center">
  <img src="logo/Kali_Logo.png" alt="KaleidoVR" width="300">
</p>

**KaliSpout2** is the [KaleidoVR](https://github.com/KaleidoVR) OBS Studio plugin for Spout2 shared-texture input and output.

Share GPU textures with [Spout2](https://github.com/leadedge/Spout2)-compatible apps, with first-class support for **OBS Studio 32.2.2+** multi-canvas setups used by Aitum Stream Suite, Vertical, and Multistream.

## Features

- Per-canvas Spout senders (each output binds to that canvas’s video mix)
- Fixes extra-canvas blackout when Spout output is enabled
- **KaleidoVR → KaliSpout2 Output Settings** (same menu as [Kaleido Launcher](https://github.com/KaleidoVR/Kaleido-Launcher))
- Auto-start is opt-in, cleared on upgrade, and retries when canvases appear after load
- Load log prints **Version** and this repo URL for easy install checks

## OBS 32.2+ canvases

Open **KaleidoVR → KaliSpout2 Output Settings**. Each row can target a frontend canvas (including Aitum extra canvases) as its own Spout sender. Outputs use `obs_output_set_media` on that canvas mix so BGRA conversion does not black out other canvases.

## Install

1. Download the latest Windows installer from [Releases](https://github.com/KaleidoVR/KaliSpout2/releases).
2. Run `KaliSpout2_Install_v*.exe`.
3. Default install path: `C:\ProgramData\obs-studio\plugins\win-spout`
4. Restart OBS, then open **KaleidoVR → KaliSpout2 Output Settings**.

## Requirements

- Windows x64
- OBS Studio **32.2.2+** (multi-canvas / Aitum workflows)

## Building

```
git clone --recursive https://github.com/KaleidoVR/KaliSpout2.git
```

- Install [CMake 3.28+](https://cmake.org/download/)
- Configure for `windows-x64`, then build
- CI packages the NSIS installer via `.github/scripts/BuildInstaller.ps1`

## Credits

Created and maintained by **KaleidoVR**.

- [kalivr.com](https://kalivr.com)
- [Discord](https://discord.com/invite/cRsufJssTA)
- [Twitch](https://www.twitch.tv/kaleidovr)

## License

GPL-2.0 — see [LICENSE](./LICENSE).

Copyright (C) 2024-2026 KaleidoVR.

Based on the OBS Spout2 plugin, Copyright (C) 2019-2021 Off World Live Ltd.

Spout2 SDK is BSD 2-Clause — Copyright (c) 2020-2024 Lynn Jarvis ([leadedge/Spout2](https://github.com/leadedge/Spout2)).
