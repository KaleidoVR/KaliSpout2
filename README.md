KaliSpout2
=========

**KaliSpout2** is the [KaleidoVR](https://github.com/KaleidoVR) fork of the OBS Spout2 plugin.

It enables import and export of shared GPU textures with [SPOUT2](https://github.com/leadedge/Spout2) compatible apps, with first-class support for **OBS Studio 32.2.2+** multi-canvas setups used by Aitum Stream Suite, Vertical, and Multistream.

Upstream project: [Off-World-Live/obs-spout2-plugin](https://github.com/Off-World-Live/obs-spout2-plugin).

## KaleidoVR fork highlights

- Per-canvas Spout senders (bind each output to that canvas’s video mix)
- Fixes extra-canvas blackout when Spout output is enabled
- **KaleidoVR → KaliSpout2 Output Settings** menu (shares the menu with [Kaleido Launcher](https://github.com/KaleidoVR/Kaleido-Launcher))
- Auto-start is opt-in, cleared on upgrade, and retries when canvases appear after load
- Load log prints **Version** and this repo URL for easy install checks

## OBS 32.2+ canvases (Aitum Vertical / Stream Suite / Multistream)

Open **KaleidoVR → KaliSpout2 Output Settings**. Each row can target a frontend canvas (including Aitum extra canvases) as its own Spout sender. Outputs use `obs_output_set_media` on that canvas mix so BGRA conversion does not black out other canvases.

## Installation

- Download the latest Windows installer from the [Releases page](https://github.com/KaleidoVR/KaliSpout2/releases)
- Run `KaliSpout2_Install_v*.exe`
- Default install path: `C:\ProgramData\obs-studio\plugins\win-spout`

## Building

```
git clone --recursive https://github.com/KaleidoVR/KaliSpout2.git
```

- Install [CMake 3.28+](https://cmake.org/download/)
- Configure for `windows-x64`, then build
- CI packages the NSIS installer via `.github/scripts/BuildInstaller.ps1`

## Acknowledgements

Thanks to Off World Live / Campbell Morgan for the original plugin, the OBS team, Spout authors, and contributors including [@shugen002](https://github.com/shugen002), [@mzlt](https://github.com/mzlt), and [@terids](https://github.com/terids).

## License

GPL v2 — see [LICENSE](./LICENSE). Original plugin Copyright Off World Live Ltd, 2019-2021.
