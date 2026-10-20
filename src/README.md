# Source layout

Shared development rules live in [AGENTS.md](../AGENTS.md).
Task guides are maintained in `.agents/skills/`.
Current ownership contracts live in `docs/specs/ui/CURRENT_ARCHITECTURE_ZH.md`; the library
layering lives in `docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md`.

| Directory | Library | Responsibility |
| --- | --- | --- |
| `app/ui/` | `MiaCode` | Product frontend: QML, models, theme, locale and native window |
| `app/services/` | `MiaCode` | Workspace, shared application services, typed ports and preference persistence |
| `app/runtime/` | `MiaCode` | Session assembly, domain runtime hosts and the latency sandbox |
| `app/platform/` | `MiaCode` | GPU binding and process/window diagnostics shared below the entry points |
| `core/chart/` | `miacode_chart` | Document, parser, transforms and the note model |
| `core/analysis/` | `miacode_analysis` | Muri analysis and the analysis pipeline |
| `core/scene/`, `core/video/` | `miacode_scene` | GPU-independent frame/layer math and preview configuration |
| `editor/` | `miacode_editor_core` | Text policy, completion and bookmark syntax |
| `audio/` | `miacode_audio` | Audio backend interfaces, worker, SFX runtime and waveform cache |
| `audio/bass/` | `miacode_audio_bass` | BASS preview backend and offline decoder |
| `preview/quick_scene/`, `preview/runtime/` | `miacode_preview_quick` | Shared Qt Quick/QSG chart rendering and the preview runtime |
| `preview/stage_media/` | `miacode_stage_media` | PV/BG stage media host over QtAVPlayer |
| `timeline/` | `miacode_timeline` | Timeline model |
| `timeline/quick/` | `miacode_timeline_quick` | Timeline Quick surface |
| `export/` | `miacode_export` | Video and cover export, QSG/D3D11 export sessions |
| `media_tools/` | `miacode_media_tools` | PV compression, ZIP packaging and network client |
| `common/` | `miacode_base` | Logging, diagnostics, cancellation and the preference port |
| `tools/` | — | Specs only |
| `devtools/` | — | Command-line diagnostics: chart dump, muri dump, audio probe and offset batch |
| `extensions/` | — | Retained manifest/schema contract; no active extension host |
| `intro/` | — | Intro overlay assets, packaged by `miacode_export` |
| `wrapper/` | — | Windows launcher |

Keep sources with their domain owner and include them by their `src`-rooted path. A library
only includes the layers it may depend on.
Reuse existing QML controls and application services; do not create parallel document, playback
or resource-lookup authorities.

Bootstrap is the GUI entry; Session assembles runtime hosts.
QSG export supports D3D11/QRhi and OpenGL sessions and an export worker;
see `docs/specs/preview/CURRENT_RENDER_EXPORT_CONTRACT_ZH.md`.

Executable contracts stay under `tools/` and link the libraries they cover; registration lives in
`cmake/devtools/`. Test targets are listed in `cmake/devtools/specs/`.
