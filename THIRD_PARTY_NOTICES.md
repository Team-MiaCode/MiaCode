# Third-Party Notices

This file inventories libraries, tools, fonts, media assets, and reference projects used by MiaCode.

MiaCode project-owned source code and build/maintenance scripts use [GNU GPLv3](LICENSE). Third-party components and assets retain their respective licenses.

## Direct Dependencies And Bundled Code

| Component | Location / Use | License / Notes |
| --- | --- | --- |
| Qt 6.10 | Application framework, widgets, Qt Quick, multimedia, deployment tools. The top-level build requires Qt 6.10. | Qt is available under commercial and open-source licenses. Confirm the intended distribution model and comply with Qt redistribution obligations. |
| FFmpeg | Export executable and preview decode SDK | Preview uses LGPL shared libraries. Export runs a separate GPL executable carrying libx264. Build sources, flags, and distribution layout are documented in [third_party/ffmpeg/README.md](third_party/ffmpeg/README.md). |
| QtAVPlayer | Vendored preview video decode backend under [third_party/QtAVPlayer](third_party/QtAVPlayer) | MIT License, see [third_party/QtAVPlayer/LICENSE](third_party/QtAVPlayer/LICENSE). |
| ScintillaQuick | Pinned Git submodule at [third_party/ScintillaQuick](third_party/ScintillaQuick), used by the chart editor. The pin and checkout command are in [scripts/README.md](scripts/README.md). | BSD 2-Clause License, see [third_party/ScintillaQuick/LICENSE](third_party/ScintillaQuick/LICENSE). Bundled Scintilla keeps its own license at [third_party/ScintillaQuick/third_party/scintilla/LICENSE](third_party/ScintillaQuick/third_party/scintilla/LICENSE). |
| SoundTouch | Audio processing under [third_party/soundtouch](third_party/soundtouch) | LGPL 2.1, see [third_party/soundtouch/COPYING.TXT](third_party/soundtouch/COPYING.TXT). Static linking and redistribution obligations need review before public release. |
| miniz | ZIP packaging under [third_party/miniz](third_party/miniz) | MIT License, see [third_party/miniz/LICENSE](third_party/miniz/LICENSE). |
| BASS, BASSmix, BASS_FX, BASS_AAC, BASSOPUS, BASSFLAC | Native audio runtimes under [third_party/bass](third_party/bass). Windows x64 ships `bass`, `bassmix`, `bass_fx`, `bassopus`, `bassflac`, and `bass_aac`. Windows arm64 and macOS ship `bass`, `bassmix`, `bass_fx`, `bassopus`, and `bassflac`. Linux x86_64 and aarch64 ship `bass`, `bassmix`, `bass_fx`, and `bassflac`. | Kept for non-commercial MiaCode builds and releases. BASS is not a general open-source dependency. Commercial redistribution of these bundled files needs a separate BASS license. |
| Fluent System Icons | Utility UI SVGs under `src/app/ui/resources/icons/`, including regular and selected filled variants, and the checkbox mark at [resources/icons/checkmark.svg](resources/icons/checkmark.svg) | MIT License. See [licenses/Fluent-MIT.txt](licenses/Fluent-MIT.txt) and the upstream [Fluent repository](https://github.com/microsoft/fluentui-system-icons). |
| Phosphor Icons | Latency editor metronome icons at `src/app/ui/resources/icons/metronome.svg` and `metronome-fill.svg`, with contours thickened to match the UI icon weight | MIT License. See [licenses/Phosphor-MIT.txt](licenses/Phosphor-MIT.txt) and the upstream [Phosphor repository](https://github.com/phosphor-icons/core). |
| Tabler Icons | Range and repeat playback icons at `src/app/ui/resources/icons/range-play.svg`, `range-play-active.svg`, `repeat.svg`, and `repeat-active.svg`, with adapted stroke weights | MIT License. See [licenses/Tabler-MIT.txt](licenses/Tabler-MIT.txt) and the upstream [Tabler repository](https://github.com/tabler/tabler-icons). |

## Fonts

| Asset | Location / Use | License / Notes |
| --- | --- | --- |
| Resource Han Rounded CN | Intro, cover, and video-export fonts at [src/intro/assets/fonts/ResourceHanRoundedCN-Bold.ttf](src/intro/assets/fonts/ResourceHanRoundedCN-Bold.ttf) and [src/intro/assets/fonts/ResourceHanRoundedCN-Heavy.ttf](src/intro/assets/fonts/ResourceHanRoundedCN-Heavy.ttf) | SIL Open Font License 1.1. The license text is in [licenses/Resource-Han-Rounded-OFL.txt](licenses/Resource-Han-Rounded-OFL.txt). |
| Xiaolai Mono subset | HUD font at [assets/fonts/XiaolaiMono-Regular.subset.ttf](assets/fonts/XiaolaiMono-Regular.subset.ttf), packaged as `:/fonts/xiaolai_mono.ttf` | SIL Open Font License 1.1, recorded in the font name table. Copyright 2020-2024 LXGW and 2014 Nozomi Seto. |
| Maple Mono Normal NL CN | HUD font at [assets/fonts/MapleMonoNormalNL-CN-Regular.ttf](assets/fonts/MapleMonoNormalNL-CN-Regular.ttf), packaged as `:/fonts/maple_mono_cn.ttf` | SIL Open Font License 1.1. The license text is in [assets/fonts/MapleMono-OFL.txt](assets/fonts/MapleMono-OFL.txt). Copyright 2022 The Maple Mono Project Authors. |

## Media And Runtime Assets

| Asset Group | Location / Use | Status |
| --- | --- | --- |
| Skin textures | [assets/skin](assets/skin) | Distributed with MiaCode's non-commercial repository and release package. |
| SFX clips | [assets/SFX](assets/SFX) | Distributed with MiaCode's non-commercial repository and release package. |
| Background outlines and noteguide images | [assets/background](assets/background), [assets/noteguide](assets/noteguide) | Distributed with MiaCode's non-commercial repository and release package. |
| Intro templates and generated visual assets | [src/intro](src/intro) | Distributed with MiaCode's non-commercial repository and release package. Intro transition behavior references gfdfdxc/maimai-transition. |

## Reference Implementations

These projects are acknowledged as behavioral or engineering references. They are not vendored code. This repository does not contain their source or assets.

| Project | How It Was Used | Status |
| --- | --- | --- |
| Minepig/MaiMuriDX | Muri detection reference | Behavioral reference only. |
| gfdfdxc/maimai-transition | Intro transition behavior and visual reference | Behavioral and visual reference for the recreated intro transition. |
| MajdataPlay | simai parsing and touch-area behavior in `src/core/chart` | Behavioral reference only. |
