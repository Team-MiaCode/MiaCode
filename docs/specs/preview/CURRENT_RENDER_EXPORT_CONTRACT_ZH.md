# 当前预览与导出渲染契约

## 当前路径

- 谱面场景数学与 layer/frame state 位于 `src/core/scene/`，不依赖 GPU 实现。
- 实时预览通过 `PreviewRuntime` 与 `src/preview/quick_scene/` 的 Qt Quick/QSG 场景绘制；
  两者同属 `miacode_preview_quick`，并以 QML 模块 `MiaCode.Preview 1.0` 提供 `PreviewQuickSceneRoot` 和 `PreviewQuickHudLayer`。
  PV/BG 视频由 `src/preview/stage_media/` 的 `PreviewStageMediaHost`（`miacode_stage_media`，QtAVPlayer）解码，
  帧以不透明句柄 `PreviewVideoFrameHandle` 进入场景状态，场景数学不依赖 QtMultimedia。
- 导出复用 QSG 场景、资源解析和渲染语义，以显式帧时间推进。
- Windows headless 导出保留 D3D11/QRhi 会话 `PreviewQuickD3D11ExportSession`，以及
  `PreviewQuickExportSession` 的 OpenGL 路线（均在 `src/export/session/`）；后端选择见 `src/app/main.cpp` 与导出 backend。

## 共享契约

| 行为 | 单一复用入口与同步面 |
| --- | --- |
| 场景层级 | `PreviewLayerOrder.h`、PreviewQuickSceneRoot；渲染 slot 顺序与 layer mask 对应 |
| 同时刻物件排序 | PreviewMarkerDrawOrder 与 prepared scene cache；preview、timeline、export 核对相同语义 |
| timing 与动画 | core/scene 的纯时间函数；离线帧不依赖事件循环 Timer |
| 皮肤与素材 | scene selectors、AssetPaths、ChartAssetPaths、scene asset loader；预览/导出解析一致 |
| 烟花范围与遮挡 | 烟花覆盖完整圆形画布，半径为逻辑画布宽度的一半；烟花层位于皮肤层下方，皮肤按素材透明度遮挡；预览与导出共用范围和层级 |
| SFX | PreviewSfxTimeline / PreviewSfxTiming；运行时音频与 VideoExportAudioRenderPlan 使用相同事件语义 |
| 导出配置 | QML session / preferences → runtime snapshot → VideoExportSnapshot JSON → worker task |
| 封面合成 | QmlCoverExportWindow/Session 与 `src/export/cover_export/`；窗口拥有自己的 engine 和捕获资源 |
| HUD 字体与导出设置 | `PreviewHudState` 与导出页经偏好端口 `miacode::preferences` 读写 `app.video_export`；GUI、CLI 导出和导出 worker 在入口安装 `PreferenceDocumentProvider` |

封面布局与通用外观复用全局偏好，输出文件及背景素材设置归当前谱面项目的 `coverExport` 字段。
切谱前写回旧项目，随后读取新项目；缺少项目背景时只复用不带自定义素材路径的外观。
谱面帧渲染器在同一封面会话中保持单调内容版本，切谱时取消旧 PV 解码并清除旧帧；
文件选择结果按发起时的来源代次接收。画布透明与帧内图像背景独立。

谱面信息、物量统计和调试 HUD 共用按字号计算的 1.25 倍基线步进；字形轮廓、
栅格覆盖余量、阴影和至少 0.12 倍字号的留白共同兜底，不依赖字体内置行高。
物量统计保留标题/数值及段落间距，紧凑双列布局使用相同规则；空间不足时仍缩字或紧凑显示。
HUD（含中央显示）在短边 1080、96 DPI 的统一逻辑画布上完成字号、行距和自适应布局，
再按实际画布等比缩放绘制。同一宽高比下，预览面板尺寸、屏幕 DPI 和输出分辨率只影响
栅格清晰度，不改变布局比例；预览窗口实际宽高比改变时才重新适配几何。
预览与导出始终使用该布局。
