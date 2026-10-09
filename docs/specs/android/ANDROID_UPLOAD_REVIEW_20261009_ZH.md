# Android 工作台与离线导出提交审阅

日期：2026-10-09。目标分支：`Miacode_Mobile`。本次差异的基线为 `3cd1060b382356bd6b2c7defa4acf72073d6ad76`。本次提交保存安卓工作台、播放设置和本机导出的当前集成成果，P5 状态仍为未验收。

## 审阅范围与结论

审阅 Android 文档和会话、批量视频队列、封面及预设队列、ZIP、SAF 发布、后台服务、音效和校准、波形及时间轴 QSG、共享设置/主题、图片纹理 provider、CMake/QRC/翻译、诊断和 Spec 的变更。检查任务快照、取消与资源释放、异步结果代次/路径校验、跨难度隔离、输出文件保护、设置迁移及启动参数归属。发现 Miniz 原始源码中的 5 处行尾空白并修正，保留其许可证。

当前审阅未发现阻止保存开发成果的新增问题。已知实现缺口和验证限制保留在 [当前功能状态](ANDROID_CURRENT_STATUS_ZH.md)，不能由本次构建与测试通过推断为 P5 已通过。原生 GUI、真机或后台/锁屏没有在本次提交检查中重新操作。

提交范围包含产品源码、共享控件、测试、构建/验证脚本、第三方 Miniz 源码及许可证和公开状态记录。暂存源码的 UTF-8、Python 语法、XML、70 个 QRC 资源引用、体积、签名材料及定向凭据检查通过；Git 暂存差异格式检查通过。Qt/SDK、签名密钥、APK、测试媒体、私有会话和 `build-devtools` 中的证据不进入源码提交。

## 最终验证

全部构建使用已有 `build-devtools` 配置、Release、并发上限 4，一次一个构建。

| 检查 | 结果 | 本地记录 |
| --- | --- | --- |
| 宿主完整目标构建 | 通过 | `build-devtools/upload-review-host-final-20261009.log` |
| CTest | 22/22 通过，6.65 秒 | `build-devtools/upload-review-ctest-final-20261009.log` |
| Android Release 与测试签名打包 | 通过 | `build-devtools/upload-review-android-20261009.log` |
| ARM64 ELF LOAD / ZIP 16 KB 对齐 / 签名 | 92 个原生库及 ZIP、签名检查通过 | `build-devtools/upload-review-20261009/elf.json`、`apk-audit.json` |
| 暂存源码、Python/XML 与 QRC 资源闭包 | 通过 | `build-devtools/upload-source-audit-20261009.json` |
| 暂存格式 | 通过 | `git diff --cached --check` |

该独立 Android 工程以旧迁移参考的 CMake 目标图组织，未包含 `scripts/governance/module_layering.py`；上述构建/link 检查不能替代新 v2 分层工具的治理检查。后续同步新版 v2 架构时需配套适用的治理脚本。

最终测试 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,927,400 字节，SHA256 `7c4c6bc9f1d6e0e43507b156575c8850f5f04ff9b272d71083a2ed11adbb32cc`。Manifest 额外启动参数为空。本次重新打包结果与 2026-10-05 最后一轮普通 APK 逐字节一致；对应导出页 91 分和原工程恢复证明继续保持原范围，并未增加新的设备验收结论。

本次为源码分支提交及推送。APK 发布包、正式签名及 P5 用户验收另行完成。
