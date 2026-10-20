---
name: qt-ui-layout-pitfalls
description: 定位 MiaCode QML 对话框、弹层、滚动、边框、层叠、文本与命中区域的布局问题。
---

# QML 布局诊断

共享规则见 [AGENTS.md](../../../AGENTS.md)。从症状选择诊断范围，按实际几何和组件实现定位：

| 症状 | 检查范围 | 参考 |
| --- | --- | --- |
| 控件重叠、内容裁剪、窗口缩小后溢出 | implicit size、Layout 约束、anchors、父容器尺寸 | [尺寸与滚动](references/recipes.md#尺寸与滚动) |
| 下拉列表空白或弹层位置异常 | contentItem、内容尺寸、Overlay 坐标、开关生命周期 | [弹层](references/recipes.md#弹层) |
| 1px 接缝、描边模糊、素材露边 | DPR、缩放、裁剪与描边坐标、素材 alpha 边界 | [像素与素材](references/recipes.md#像素与素材) |
| 文本溢出、行号错位 | 字体、基线、行高、宽度及溢出策略 | [文本](references/recipes.md#文本) |
| 分离窗口弹层呈实色、材质区域异常 | 所属窗口、backdropSource、materialRegions、弹层父项 | [窗口与材质](references/recipes.md#窗口与材质) |
| 滚动受阻、快捷键或点击区域异常 | 指针事件接收、焦点、坐标映射、命中函数 | [输入与层叠](references/recipes.md#输入与层叠) |
| 图层顺序或主题明暗异常 | 声明顺序、父子 alpha、Theme 绑定 | [输入与层叠](references/recipes.md#输入与层叠) |

复用现有控件，修复尺寸或坐标来源。界面审查结合相关窗口尺寸、主题和缩放下的渲染资料；测试与工具执行遵循当前任务授权。
