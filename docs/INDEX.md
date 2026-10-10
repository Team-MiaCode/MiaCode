# 文档索引

由 `python3 scripts/governance/docs_index.py --sync` 从文档 frontmatter 生成；请勿手工编辑。

入口与维护规则见 [README](README.md)；可执行规格见 [Spec 目录](tests/SPEC_CATALOG.md)。

## stable-current（7）

| 文档 | Canonical ID |
| --- | --- |
| [模块分层（当前）](specs/architecture/MODULE_LAYERING_CURRENT_ZH.md) | architecture.module-layering |
| [延迟 Slide、头材质与无头 Slide 规则](specs/chart/SLIDE_DELAY_AND_HEAD_MATERIAL_SPEC.md) | chart.slide-head-material |
| [无理检测规则与行为规格](specs/muri/MURI_DETECTION_SPEC.md) | muri.detection |
| [当前预览与导出渲染契约](specs/preview/CURRENT_RENDER_EXPORT_CONTRACT_ZH.md) | preview.render-export |
| [Timeline 坐标与聚焦规格](specs/timeline/TIMELINE_COORDINATE_FOCUS_SPEC.md) | timeline.coordinate-focus |
| [当前应用架构与所有权](specs/ui/CURRENT_ARCHITECTURE_ZH.md) | ui.runtime-ownership |
| [QML 到 Session 的直接访问边界](specs/ui/UI_BACKEND_SURFACE_ZH.md) | ui.backend-surface |

## reusable-verification（2）

| 文档 | Canonical ID |
| --- | --- |
| [无理检测测试清单](tests/MURI_DETECTION_TEST_CHECKLIST.md) | verify.muri |
| [Timeline 坐标与聚焦测试清单](tests/TIMELINE_COORDINATE_FOCUS_TEST_CHECKLIST.md) | verify.timeline-focus |

## working（5）

| 文档 | Canonical ID |
| --- | --- |
| [模块分层阶段一交付报告（方案 A + 偏好注入）](audit/MODULE_LAYERING_A_DELIVERY_ZH.md) | — |
| [模块分层阶段一复审](audit/MODULE_LAYERING_A_REVIEW_ZH.md) | — |
| [Windows 首次播放无响应：BASS DEV_DEFAULT 时序根因与修复](audit/PREVIEW_FIRST_PLAY_DEV_DEFAULT_ROOT_CAUSE_AND_FIX_ZH.md) | — |
| [Windows 预览 Slide 轨道访问异常调查与修复](audit/PREVIEW_SLIDE_TRACK_ACCESS_VIOLATION_ZH.md) | — |
| [模块分层与解耦方向](specs/architecture/MODULE_LAYERING_ZH.md) | — |
