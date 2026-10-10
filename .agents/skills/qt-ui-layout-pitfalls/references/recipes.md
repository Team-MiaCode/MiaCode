# QML 布局处理

## 尺寸与滚动

- 查看控件的 `implicitWidth/implicitHeight`、父容器可用尺寸以及 `Layout.minimum/preferred/maximum` 约束。
- Layout 管理的子项通过 Layout 属性表达尺寸；普通 Item 通过 anchors 或显式几何表达尺寸。
- 对话框从 `src/app/ui/components/AppDialog.qml` 的 preferredWidth/preferredHeight、窗口边距和 body 尺寸关系查起。
- ScrollView 的 contentWidth、contentHeight 与子内容实际尺寸保持一致；检查 clip、滚动条和窗口缩小时的可用高度。
- 使用共享组件的 padding 和 Theme 参数，按内容关系解决溢出。

## 弹层

- 对话框、列表和粘性菜单分别从 AppDialog、AppDropdownPanel、AppStickyPopup 查起。
- 核对 parent、Overlay、anchorItem 和局部坐标转换；弹层尺寸包含内容与 padding。
- 空白下拉列表检查派生控件的 contentItem 安装时机与隐式尺寸。基类生命周期辅助对象通过显式属性持有，防止默认 contentData 提前访问延迟创建的内容项。
- 关闭动画期间的 active/closing 状态由共享基类管理；核对模态、Esc、外部点击和焦点恢复。
- 关闭确认通过后处理子弹层与资源释放，取消确认时保持编辑状态。

## 窗口与材质

- 分离预览的父项切换由 MainSplitView 管理；预览移动期间通过 `surfaceActive` 暂停场景订阅。
- 设置弹层归属由 MainSplitView 的 `settingsDialogParent` 选择，窗口 Overlay 与采样源应属于同一窗口。
- FloatingCard 读取 `Window.window.backdropSource`；BackdropBlur 负责采样坐标和几何变化。排查实色弹层时检查采样源、主题材质开关和弹层可见性。
- WindowChrome 的 `materialRegions` 表达原生材质区域，WindowTitleBar 和 CornerMask 使用对应的 Theme 材质颜色。窗口尺寸、全屏与侧栏状态变化时核对区域和裁切边界。

## 像素与素材

- 接缝两侧使用同一坐标和设备像素比；描边与裁剪从同一矩形派生。
- 缩放描边时检查逻辑宽度到设备像素的换算，结合实际渲染判断抗锯齿。
- 素材对齐依据 alpha 可见边界，区分文件画布、裁剪区域与视觉内容。
- 素材拼接检查共享边缘的颜色和 alpha；预览与导出比较使用相同帧时间和缩放条件。

## 文本

- 谱面编辑器入口为 SourceEditor.qml，文本与行号由 ScintillaEditor 管理；字体和几何从编辑器接口取得，补全行的 `labelFont` 使用 `editor.effectiveFont`。
- 普通界面文本使用 Theme.uiFont；谱面正文使用 Theme.codeFont。预览与导出 HUD 的各类字体由独立偏好控制，定位时区分 HUD 与 QML 播放进度、NoteStatistics。
- 自绘 Canvas 文本依据同一字体的 FontMetrics 定位基线；文本测量与绘制采用一致的字体、缩放和设备像素条件。
- 固定宽度槽位按内容选择换行、缩放或省略；保留完整信息的可访问入口。
- 导出可见滚动文字由帧时间驱动，静态输出使用适用的溢出策略。

## 输入与层叠

- 检查 MouseArea、PointerHandler 的接受按钮、手势与事件传播；滚轮交给所属滚动容器。
- 文本输入获得焦点时保留编辑快捷键；鼠标形状与交互内容一致。
- 绘制与命中使用同一几何函数，并核对局部、窗口和场景坐标。
- 图层按声明和绘制顺序排列；颜色从 Theme 绑定，检查父子背景的 alpha 叠加。

对应实现与契约从 `src/app/ui/components/`、`src/app/ui/theme/`、`docs/specs/ui/` 查找。
