---
lifecycle: working
---

# Windows 预览 Slide 轨道访问异常调查与修复

- 日期：2026-10-10
- 调查基线：`dev`，HEAD `663d14d34150`，含本地修改
- 环境：Qt 6.10.3、MSVC 14.36.32532、Release

## 故障位置与证据

Windows 应用程序事件和启动 beacon 记录 PID 39380 在 12:16:02 退出，异常代码为
`0xC0000005`，故障模块为 `MiaCode.exe`，模块内偏移为 `0x6BA165`。
异常访问类型为读取，地址为 `0x3000000010`。最后一条 GUI 播放记录是
`playback_start_prepare`；异常线程为 18676，GUI 记录线程为 27684。

`preview_realtime_object_hot_path_spec` 的 `verifyOpeningHeadlessSlideTracks`
在同一 Release 代码上复现访问异常。规格使用 `QCoreApplication` 和场景计算函数，
覆盖开头连段 Slide、偏移、往返时间采样与三种渲染模式。

GDB 调用栈与规格符号映射把异常定位到 `buildPreviewTrackLayerState`，规格故障偏移为
`0x3CFF5`。故障指令通过栈槽 `[rbp+0x178]` 读取区域容器指针，再读取其箭头数量。
调试时该地址指向带有 `0xABABABAB`、`0xFEEEFEEE` 堆填充值的失效区域；
输入 marker 的 `slideTrackAreaPoints` 则保有有效的两段轨道数组。

反汇编显示，该栈槽此前用作排序的临时存储，而裁剪循环读取它作为轨道区域来源。
源代码中的区域引用来自输入 marker，Release 优化结果取用了错误的存储位置。
证据支持将故障归因到此函数的优化代码中的失效指针读取；MSVC 优化器缺陷归因属于
基于指令与输入数据对照的推断。

## 修复

轨道裁剪位置计算提取到 `PreviewTrackShared` 的 `findPreviewSlideTrackTrimStart`。
该函数逐段、逐区域扣除箭头数量，返回首个可见箭头的段、区域及区域内裁剪数量。
`segmentIndex == segments.size()` 表示全部箭头裁去。

场景状态构建复用该结果，预览与视频导出共享这一实现。裁剪计算由原来的两层扫描
收敛为一次区域扫描，轨道区域定位与大型图层构建函数的排序临时区分离。

## 验证

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| `preview_realtime_object_hot_path_spec` | SegFault，GDB 定位到轨道裁剪循环 | 通过 |
| `preview_slide_erase_by_area_spec` | 通过 | 通过，增加跨区域、跨段、空区域、空段及全部裁去边界 |
| 默认 `MiaCode` Release 构建 | — | 通过 |
| `module_layering.py` | — | 通过 |

验证命令：

```powershell
cmake --build build-devtools/desktop-qt610 --config Release --target MiaCode preview_realtime_object_hot_path_spec preview_slide_erase_by_area_spec --parallel 4
ctest --test-dir build-devtools/desktop-qt610 -C Release -R '^(preview_realtime_object_hot_path_spec|preview_slide_erase_by_area_spec)$' --output-on-failure
python scripts/governance/module_layering.py
```

本记录的产品证据来自用户启动进程的崩溃日志；修复验证使用命令行场景规格。
