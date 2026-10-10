---
name: qt-ui-design
description: 设计或审查 MiaCode 的 Qt/QML 页面、窗口、控件状态、图标、主题与键盘访问。
license: LicenseRef-Qt-Commercial OR BSD-3-Clause
metadata:
  author: qt-ai-skills
  version: "1.0"
  qt-version: "6.x"
  category: conceptual
---

# Qt/QML 界面设计

共享规则见 [AGENTS.md](../../../AGENTS.md)。通用控件位于 `src/app/ui/components/`，窗口装饰位于 `src/app/ui/chrome/`，界面图标位于 `src/app/ui/resources/icons/`。

## 设计

- 根据任务和相邻页面确定操作目标、内容优先级、窗口尺寸、输入方式、语言与主题。涉及产品决策的缺失信息向用户确认。
- 沿用现有页面结构与共享控件。表单用 LabeledCombo/LabeledSlider，对话框用 AppDialog/DialogFooter，菜单用 AppMenu/AppMenuItem。
- 布局表达内容层级、操作分组和窗口缩放关系；尺寸与视觉参数从 Theme 获取。
- 执行类按钮通过点击触发命令；`active/checked` 表达持续状态，菜单按钮的展开状态跟随弹层 `active`。分离、合入等相反动作使用对应图标与提示，动作本身采用普通按钮外观。
- 操作状态覆盖可用性、进行中、完成和错误反馈；耗时任务接入 JobProgressService。
- 检查键盘焦点、快捷键、文本输入、长文案和主题切换；图标与文本共同表达关键动作。
- 布局或裁剪诊断使用 `qt-ui-layout-pitfalls` 的症状表。

## 图标

- 优先采用仓库使用的 Fluent System Icons，按动作含义选取图形；相似轮廓须核对其实际语义。
- 沿用相邻 SVG 的 20×20 画布、颜色和轮廓重量。常规 Fluent 资源使用 `#212121`，按现有素材增加 0.5 的轮廓描边；显示颜色由 IconButton 的 IconImage 和 Theme 提供。
- 状态按钮按现有用法配置常规与填充图标；执行类按钮使用常规图标。SVG 资源在 `CMakeLists.txt` 登记，保留来源与许可说明。
- 自绘图标沿用同组图标的形状、圆角、比例与留白，成对动作保持视觉对应。按按钮实际的 16×16 显示尺寸控制细节密度、边框间距和视觉重量。

## 窗口与材质

- 窗口操作复用 WindowChrome、WindowTitleBar 和 WindowGestureArea。分离预览复用 PreviewPane，系统最大化与全屏由窗口平台接口处理。
- 系统窗口名称和自定义标题分别绑定各自用途；系统名称应能区分编辑窗口与预览窗口。
- 弹层使用所属窗口的 Overlay。FloatingCard 从该窗口的 `backdropSource` 采样；窗口标题栏原生材质通过 WindowChrome 的 `materialRegions` 指定区域。

## 审查

结合源码与可用渲染资料核对布局、操作路径、焦点、文字溢出、主题和命中区域。
报告具体位置、观察证据、用户影响和修改建议；实现推断与渲染观察分别标明。

本仓库版本依据 Qt UI Design 技能裁剪，许可见 [LICENSE.txt](LICENSE.txt)。
