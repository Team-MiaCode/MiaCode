# Android 迁移实现与验收记录

目标目录：`D:\STUDY\Project_Work\MiaCode_dev2\MiaCode_Mobile`。

目标：完整保留 v2 功能，Android 手机和平板横屏使用，本机离线导出，APK 发布；主要界面与 v2 至少 90% 相似。本文记录实际完成范围，不把开发探针算作产品功能。

## 当前功能与提交检查（2026-10-09）

当前已接入的功能、实现缺口及设备验收项统一见 [当前功能状态](ANDROID_CURRENT_STATUS_ZH.md)。用户本日明确要求提交并推送当前成果，本次按开发成果保存，不将该请求解释为 P5 用户验收通过。最终宿主与 Android Release 构建、22/22 项 CTest、APK 审计及暂存源码检查通过，见 [提交审阅](ANDROID_UPLOAD_REVIEW_20261009_ZH.md)。APK 与 2026-10-05 最后一轮普通包哈希相同；本次没有新增真机、全页面或后台/锁屏通过声明。

## P5 后的 v2 更新同步顺序（2026-10-03）

用户要求补全 v2 的最新更新，并明确安排在 P5 验收完成、安卓端提交和推送之后执行。先完成当前 P5 功能、设备与界面验收，再审阅和提交安卓端成果并推送，随后核对 v2 最新差异，逐项移植适用的更新并验证。当前阶段保持已经记录的迁移来源与验收基准，后续同步另留差异及验证记录。

用户再次确认该顺序：P5 用户验收通过 → 审阅差异并完成必要构建与测试 → 提交并推送安卓分支 `Miacode_Mobile` → 补齐 v2 最新更新。安卓工作目录继续为 `MiaCode_Mobile`；补齐更新之前另行记录当时的 v2 来源提交、适用差异及验证结果。

2026-10-09 用户另行要求提交并推送当前开发成果，按顶部记录执行本次保存。最新 v2 同步继续保留 P5 用户验收完成的前提。

## 内容时长、解码长度与导出页重新配对（2026-10-05）

当前普通测试签名 APK 为 `7c4c6bc9f1d6e0e43507b156575c8850f5f04ff9b272d71083a2ed11adbb32cc`，99,927,400 字节。最终完整宿主及 Android Release 构建通过，日志 `decoded-duration-host-build-20261005.log`、`decoded-duration-android-build-20261005.log`；最终 CTest 22/22，7.56 秒，日志 `decoded-duration-ctest-20261005.log`。92 个 ARM64 ELF LOAD 16KB、ZIP 对齐、签名和空启动参数检查通过。以下原生证据位于 `build-devtools/android-ui/content-duration-native-20261005/`。

预览改为消费实际 v2 `TimelineQuickModel` 的完整时间跨度，再应用共享的谱面尾段/音乐时长规则。单曲导出按偏移后的 note marker 的开始、结束、轨迹、可用和分段发射时间计算谱面结束，并应用同一规则，避免复用当前试听难度的预览时长。实际回归在修正前失败：短谱面安卓为 1.75 秒，实际 v2 时间轴加尾段为 7.25 秒；保留 `content-duration-before-verdict-20261005.txt`。修正后的用例覆盖长/短谱面、hold、正负 first 和空结束谱面。

v2 预览的音乐长度来自波形解码结果。Android 的已有波形异步回调现在将解码长度传给预览，单曲导出从预览读取同一长度；解码未完成时保留播放器长度作为临时值。换音轨时清理旧长度，波形请求代次及当前路径共同拒绝旧结果。实际消费者测试覆盖音轨由 11 秒换为 2 秒、旧路径迟到结果、非有限输入及实际 `MobileTimeline` 波形回调；原音轨时长可缩短，不再只增不减。

实际原始 MP3 哈希逐项核对相同。使用既有 v2 参考的 BASS DLL 按原 no-sound 解码参数探测并完整读取：44.1kHz 双声道、4,549,871 帧，103.171678 秒，与 v2 界面 103.172 对应；FFprobe 元数据为 103.074313 秒，解释旧安卓 103.074。Android 既有波形缓存经实际格式头读取为 103.183 秒。修正后的普通 APK 实际导出页也显示 103.183，剩余约 11.3 毫秒为两端解码输出长度差异，尚未消除，不能声称时长完全一致。证据 `audio-duration-comparison.json`、`native-waveform-duration.json` 和两轮 `*-content.png`。没有采用固定秒数补偿。

实际共享 ExportSession 消费者的片头切正文、16:9/4:3/1:1 画布、10–15 秒选区试听、WAV 和设置快照回归，单独运行两次通过：`intro-scene-ui-20261005/run-4em1yaza/verification.json`、`run-fb9csrsv/verification.json`。第一次与 CTest 同时运行，实际在正文零位置出现暂停并以 52 退出，保留 `run-ba7ul4ma/`。代码在应用失去前台时暂停；并发测试窗口影响前台状态是可能原因，未直接证明。两次单独通过不构成该失败原因或长期播放稳定性已解决的证据。已查看最终宿主片头截图，时长为 103.183。

原生普通 APK PID 11777 恢复《白金ディスコ》Master 13，实际进入导出输出页。沿归档桌面偏好、背景、2560×1440、60 FPS、192kbps、快速/标准配置，在 -5.816667 秒暂停。新鲜 XML 确认内容边界 `[0,24][1294,862]`，只裁切成与真实 v2 参考相同的 1294×838，不缩放。已查看 `decoded-export-head-content.png` 和真实参考 `actual-v2/export-output-0.png`；单页结构化人工审阅 91 分、`pass`，记录 `decoded-export-head-verdict.json`。此前中间普通候选 `b157e068...` 的同页审阅也为 91 分，并独立归档其 APK、审计、截图及实际加载库；分数不相互借用，也不是像素一致率。

当前页面主要布局、导出控件及片头图案对应，残余差异包括字体字重、完整私有路径显示、表单约 4–11 像素的纵向偏移、背景透出及约 11 毫秒时长。仅此页面通过人工阈值；封面及其他主要页面仍需按当前候选逐项配对，整体 `pairedV2FidelityVerified=false`、`P5Accepted=false`。本轮没有重新执行原生 MP4 全链、后台/锁屏或真机矩阵；上一轮离线成片证据仍按其原 APK 和范围记录。

确认无活动导出服务后，恢复原五个私有文件和原始屏幕尺寸/density，恢复时逐字节一致。最终普通无额外参数 PID 12114 经实际恢复入口返回 Master 13、零位置、546 对象；`ordinary-restored-complete.png` 已查看。文件、偏好、APK、实际 ARM64 库、正文、旧偏好源、G 盘原谱面及原 v2 干净参考检查通过；有限运行时错误及当前 PID ANR 未匹配，记录 `restoration.json`、`final-verification.json`，不代替长期稳定性结论。首个恢复截图读取早于收集器完成而报告文件不存在，等待同一收集器完成后实际查看，不重启或伪造恢复结果。

未提交或推送，v2 最新更新继续遵循顶部用户指定的后续顺序。

## 片头与封面纹理上传修正（2026-10-05）

当前普通测试签名 APK SHA256 为 `f57361cc1aba06d9822a1e4ee4f0ed6b4274deddaa9e0f1f1cd5527dec2fb3a8`，99,923,304 字节。Android Release 构建、92 个 ARM64 ELF LOAD 16KB、ZIP 对齐、签名和空启动参数审核通过；审核及以下原生观察位于 `build-devtools/android-ui/intro-scene-native-20261005/`。

先在实际挂载的片头场景中读取图片状态、素材哈希、祖先可见性、几何和渲染线程纹理。原生首帧九张过场图片均已解码，纹理存在且启用 mipmap，但日志同时出现九次 `s_glGenerateMipmap` 的 `0x502` 错误，实际画面为平紫色。宿主纹理属性相同、画面正常；宿主观察动画帧不同，不把这一属性核对描述为同帧视觉验收。失败截图、CPU/GPU JSON 和完整日志保留。

新增公共 Qt API 的 RGBA 纹理工厂和本地图片 provider。Android 片头过场、难度卡、曲绘、封面背景和自定义图片走同一入口，实时和离线 QQmlEngine 均注册；保持原素材、透明度、模板几何、动画时间和 mipmap。Qt 默认图片工厂会将 RGBA 再转回 ARGB，因此使用 `QQuickTextureFactory` 保持上传格式。等级图集通过显式裁剪参数选择原 glyph；原 `sourceClipRect` 继续保留。`image://coverchart` 仍由自己的 provider 管理，其既有 Android mipmap 限制不在本次修正范围内。

- 第一轮只修过场纹理，普通包的片头首帧恢复；中段卡片仍缺曲绘和卡框，并新增八次 mipmap 错误。中间候选与失败证据保留，不作为最终通过结果。
- 最终普通无诊断参数 PID 24564，实际恢复《白金ディスコ》Master 13、546 对象，进入导出并在实际滑块上定位约 -2.968 秒。已查看 `export-head-card-rgba-ordinary.png`、`export-middle-card-rgba-ordinary.png`：过场图案、卡框、曲绘、Deluxe 标签、MASTER、Lv13、标题、作者与 BPM 正常显示，采集日志未匹配 mipmap 错误。
- 已查看普通包 `cover-card-rgba-ordinary.png`，实际封面画板的 Standard 卡框、曲绘、MASTER 与等级图集正常显示。它与视频片头的模式及状态不同，不作为跨页面同条件配对。旧桌面封面参考客户端边界未实测，封面局部叠加效果与完整页面相似度继续核对。
- 完整宿主 Release 构建通过。实际共享 ExportSession 消费者回归通过，记录 `intro-scene-ui-20261005/run-xi9erd97/verification.json`；实际封面导出消费者回归通过，包含 JPG、透明 PNG、双谱面帧、原文件保护及文档替换后缓存清理，记录 `intro-card-rgba-cover-ui-20261005/run-o52sqs5s/verification.json`。宿主消费者回归不代表原生编码或真机验收。
- 新增 `MobileIntroTextureSpec` 验证中文、空格、`#`、`%` 文件路径、像素保存、图集裁剪、请求尺寸和实际 QML Image 消费裁剪纹理；完整 CTest 22/22，8.50 秒，日志 `intro-card-rgba-final-ctest-20261005.log`。
- 原生离线成片使用独立私有目录中的五个媒体/谱面副本，哈希逐项核对。测试入口启动参数单独打包，92 个原生库与普通包完全相同；普通 APK 重打包后与保存版本逐字节一致，证据 `card-offline-package-proof.json`。这次任务来自默认构建任务，卡片模式为 Standard；普通界面观察的已保存模式为 Deluxe，二者不作为同状态视觉比较。
- Android PID 25147 的任务最终成功，720×720、30 FPS、385/385 帧、12.833 秒，含 5.816667 秒片头、2 秒预备段及 5 秒正文；输出 `offline-intro.mp4`，4,315,127 字节，SHA256 `0968f616ffbf890cfb907c4097989c47c62500f8e2b6bcb6e2471279f6bc824d`。FFprobe 实际读出 385 个 H.264 视频帧及 AAC 48kHz 双声道音轨；任务日志未匹配 mipmap 错误。报告 `offline-task-report.json`、`offline-result.json`、`offline-ffprobe.json`。
- 已实际查看成片 0、3.2、6.4、9、12 秒五个抽帧：过场图案、完整难度卡、预备段、正文和 PV/音符显示可见。9 秒成片对应约 1.183 秒正文的 PV 为黑色；另外解码原始 PV 的对应时间，确认素材本身也是黑帧，4.183 秒对应画面为漫画，与 12 秒成片背景内容对应。原 PV 与设备副本哈希一致。额外两帧使用 `build-devtools/offline-video-probe/` 中基于现有宿主解码器、仅改采样时间的 QA 收集器，不修改生产解码路径。该本机成片验证不涵盖其他分辨率、全长曲、原生听感或后台/锁屏。

测试期间模拟器一度变为四音符工程，先归档其会话、配置、偏好和谱面于 `intervening-state/`，没有据此判定应用故障。用户明确回复“不是我，可以继续测试”后，确认无活动导出服务，再恢复归档的原工程及匹配配置继续检查。四音符工程保留。

测试任务正常退出后，收集器对 `pidof` 的非零返回曾误报一次；改为允许已退出进程并读取唯一匹配的最终任务报告。首个 FFmpeg 抽图尝试因本地裁剪版没有输出编码器/封装器而失败，转用上述 Qt 收集器；收集器初次独立编译缺少 `QVideoFrame` 头文件，补齐后构建与解码通过。保留各次日志，不把这些 QA 工具问题算作产品导出失败。

确认导出服务结束后重新安装普通 APK，恢复原五个私有文件和屏幕参数，恢复时逐字节核对通过。普通无额外参数 PID 9869 经实际恢复入口返回原 Master 13、零位置、546 对象；`ordinary-restored-complete.png` 已查看。文件、非更新偏好、安装包、实际 ARM64 库、正文、旧偏好源、G 盘原谱面及 v2 干净参考核对通过，有限运行时错误/当前 PID ANR 检查无匹配；证据 `restoration.json`、`final-verification.json`，不构成长期稳定性通过。

本节修正片头和封面本地静态素材的上传，不赋予新的整体相似度分数。整体 `pairedV2FidelityVerified=false`、`P5Accepted=false`；真机、Android 12、长期性能、后台/锁屏及其他开放项继续验收。未提交或推送，最新 v2 同步顺序保持顶部约定。

## 导出实时预览比例与退出恢复（2026-10-05）

MobilePreview 的画布比例由固定 1:1 改为带通知的运行时状态。MobileExportComposition 按实际共享导出任务的宽高在进入试听及修改分辨率时设置比例；退出试听恢复 1:1。非有限和越界比例按实际 v2 的 1–3 规则归一化。现有 PreviewPane 和全屏预览直接消费这一状态，保持场景与媒体在同一实际画布内布局，未调整个别控件像素位置。

- 完整宿主 Release 构建通过。实际 ExportSession 消费者检查验证 16:9、4:3、1:1 的可见 PreviewSurface 宽高，保留片头负时间、正文切换、10–15 秒选区试听、WAV 和编码设置快照回归，报告 `build-devtools/export-aspect-ui-20261005/run-tjxmo9uh/verification.json`。宿主首帧 `intro-head-wide.png` 及 4:3 正文截图已查看。最终宿主 CTest 21/21、6.61 秒，日志 `export-aspect-ctest-20261005.log`。
- 首轮 UI 回归退出 52：新增两次实际截图加载使固定 1500 毫秒检查早于真正播放推进，观察位置仍为 -0.2 秒。改为定时观察实际 transport 越过零位置后继续，保留整体 90 秒失败上限，不修改产品传输行为。失败证据 `export-aspect-ui-20261005/run-kw0c5ovg/` 保留；最终通过来自后续独立目录。
- Android Release 构建通过，当前普通测试签名 APK SHA256 `8fbb8811c05f9ec33dcc6083b050f334c07b9dbf71db6fed8b8e0a437a1914ab`，99,902,824 字节，92 个 ARM64 ELF LOAD 16KB、APK ZIP 对齐、签名和空启动参数审核通过。日志 `export-aspect-android-build-20261005.log`，审核 `build-devtools/android-ui/export-aspect-native-20261005/apk-audit.json`。
- 普通 Android 14 ARM 转译模拟器 PID 19950，以归档桌面背景/偏好和实际 1294×838 内容区进行操作。实际分辨率下拉选择、截图/XML 及原始像素测量确认：2560×1440 和 1280×720 的预览均为 527×296；960×720 为 527×395；1024×1024 为 527×527。保持同 PID，从宽屏导出切回谱面，实际预览恢复 527×527。截图均已查看，证据 `export-aspect-native-20261005/aspect-observations.json`。这是画布比例与退出恢复的验证，不是全页面相似度评分。
- 片头在这三种比例下仍是平紫色，图案问题没有解决。宿主同路径首帧图案正常；原生完整动画、声音及导出成片仍待诊断。本轮未改变 PNG 素材、默认图形后端、解码策略或渲染循环。总时长约 0.098 秒差异及封面局部异常透明效果继续开放。

测试结束确认无导出服务，还原五个私有文件和屏幕尺寸/density。普通启动 PID 20790，经实际恢复入口回到 Master 13、零位置及 546 个对象，`ordinary-restored-complete.png` 已查看。会话、QSettings、谱面、音轨、非更新偏好、安装包及加载 ARM64 库、正文、旧偏好源、G 盘原谱面和原 v2 干净参考核对通过；证据 `export-aspect-native-20261005/restoration.json`、`final-verification.json`。所列有限运行时错误及当前 PID ANR 块未匹配，不构成长期稳定性通过。

本轮不重新赋予视觉分数，不将比例修正视为整体 90% 通过。物理 ARM 手机和平板、Android 12、真实 16KB 设备、后台/锁屏和长任务性能继续验收。未提交或推送，P5 及 v2 最新更新顺序保持顶部约定。

## 导出片头实时试听与当前成果边界（2026-10-05）

当前普通 Release 测试签名 APK 更新为 `4a5a681d8dc9bc586674078e5024ba48c8968bfe3dfadc8e335016e31826170d`，99,902,824 字节。导出试听现使用实际 ExportSession 的全区间/片头/计数拍设置：启用片头时进入约 -5.817 秒的负时间区间，谱面场景固定于零位置；片头按 v2 的 1 倍速作者时间推进，到零后交还谱面、音乐和用户选择的播放速率。片头声音及 clock_count 接入实际 PCM 混音计划，拖动跳过已经发生的声音，退出导出试听清理其专用状态。片头模板和字体沿用共享模型。

自定义片头声音增加异步 QAudioDecoder 解码、过期请求拒绝及超时/格式限制。Android 音乐库路径设为应用私有可写目录，供实际导入、试听与离线导出共同使用；此前 APK 内只读目录不能承担用户导入。原 v2 工作区没有修改。

- 宿主和 Android Release 构建通过，最终宿主 CTest 为 21/21、6.32 秒，日志 `build-devtools/export-intro-final-ctest-20261005.log`。规格检查实际开场/计数拍 PCM、跳转不重放、片头增益、负时间暂停及谱面冻结、自定义声音解码替换。
- 实际共享导出页宿主回归通过负时间定位、片头帧 274、零位置切回谱面及选区 WAV/编码设置保持，证据 `build-devtools/export-intro-ui-20261005/run-kox6xdwk/verification.json`。已查看宿主片头中段图案；该图不能代替 Android 图形验证。
- 原生包的 92 个 ARM64 ELF LOAD 16KB、APK ZIP 对齐、测试签名和空启动参数审核通过，证据 `build-devtools/android-ui/export-intro-native-20261005/apk-audit.json`。默认 host 图形的 Android 14 x86_64 ARM 转译模拟器以普通参数运行；实际 PID 18666 的导出页显示负时间，但片头初始及停止后均为平紫色，图案未正常显示，仍未通过。导出配置 16:9 时实时预览仍为正方形，总时长约 0.098 秒差异也保留。
- 播放中的一次 UIA 观察超时；随后在前一观察尚未终止时误发第二次 UIA，后者失败，均未算作有效截图。保留原失败记录，后续观察等待同一句柄结束再继续。原进程没有据此重启；原始截图 `after-observation-timeout.png` 已查看，显示同 PID 在约 01:11 处正常谱面/PV 播放，证明片头后能够进入正文，不证明片头完整动画、实际听感或持续性能。`intro-stopped-settled.png` 已查看，停止可返回负时间片头入口。
- 自定义声音与计数拍的当前原生实际听感、超过片头长度的自定义声音连续性、大计数拍参数的资源上限仍待检查。当前包未重新跑完整后台/锁屏或全页面视觉矩阵，未赋予新的视觉分数。

中间编译失败为 Qt QList 原始指针追加接口不匹配，修正后构建通过。自定义声音测试首轮把替换前同步报出的旧源错误算入替换后检查，结果 6 通过、1 失败；修正观察窗口后最终 CTest 通过，原失败日志保留。当前结论以最终实际测试为准。

测试后确认导出服务空闲，还原五个私有文件及原屏幕参数。普通启动 PID 19291，通过实际恢复入口回到 Master 13、零位置与 546 个对象；`ordinary-restored-complete.png` 已查看。最终文件/偏好、安装包哈希、加载 ARM64 库、正文、旧偏好源、原始 G 盘谱面及原 v2 干净参考全部核对通过，证据 `export-intro-native-20261005/final-verification.json`。有限错误模式未匹配不等于启动稳定性通过。

当前成果覆盖编辑/分析、时间轴/实时预览、单曲/批量 MP4/WAV、封面/预设、ZIP 与共享设置的主体路径。上一候选的主编辑和信息页 92/92 分只适用于对应两页；导出页上一候选 85 分，当前片头修正尚未重新评分。全页面 90%、完整功能保留、物理 ARM 手机和平板、Android 12、真实 16KB 设备及长任务性能仍待验收。`P5Accepted=false`、`pairedV2FidelityVerified=false`、`startupStabilityPassed=false`。本轮没有提交或推送，最新 v2 同步顺序保持顶部用户约定。

## 系统安全区域、弹窗及导出/封面配对复核（2026-10-05）

实际 Android 冷启动暴露了底部状态栏裁切：`ApplicationWindow` 的内容项已扣除系统安全区域，但工作台和弹窗 Overlay 仍使用整个窗口宽高计算缩放。现在二者统一使用实际内容项的尺寸，Overlay 的位置也随内容项定位。依据为 [Qt ApplicationWindow 安全区域文档](https://doc.qt.io/qt-6/qml-qtquick-controls-applicationwindow.html#safe-areas)。实际绘制与触摸继续共用同一缩放变换。

完整宿主及 Android Release 构建通过，日志分别为 `build-devtools/safe-area-host-build-20261005.log`、`safe-area-android-build-20261005.log`。实际工作台 `1280×720`、`854×393`、`1536×1024` 的六个独立 seed/restart 进程通过；新增非对称安全边距情形验证内容、工作台、状态栏和 Overlay 的实际场景坐标，同时保留真实分隔条拖动、尺寸变化及重启持久化检查。证据 `safe-area-layout-20261005/run-ssza23np/verification.json`；已查看窄屏的 `layout-safe-area-seed.png`。本轮未改变核心逻辑，未重复上一轮已通过的 21 项 CTest。

新普通测试签名 APK `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk` 为 99,886,440 字节，SHA256 `e1d077d57ee6f40dec21f47a5fbbbc94aafbe175155be21fd333cbc3089c8a72`；92 个 ARM64 ELF LOAD 16KB、ZIP 16KB、签名和空 manifest 参数检查通过。Android 14 x86_64 模拟器 ARM 转译环境保持默认 host 图形，普通无额外启动参数 PID `16920`。配对复用归档桌面配置与背景，实际内容边界 `[0,24][1294,862]`，只裁切内容、不缩放图片。主界面和单曲/批量导出页的底部状态栏完整显示；十路音频设置及底部按钮位于实际内容边界内，可以关闭。截图、XML、动作记录和 `observations.json` 位于 `build-devtools/android-ui/safe-area-native-20261005/`。

单曲输出页的结构化人工视觉审阅由修正前 82 分变为 85 分，仍为 `revise`，分数不是像素一致率。实际 v2 在添加片头时显示负时间片头帧；Android 虽然开关为启用，仍显示零位置圆形预览。实时片头及计数拍试听传输待补；总时长还相差约 0.098 秒，需核对媒体时钟。批量页已实际查看，未另行评分。封面稳定截图仍存在卡片局部半透明色块；原因未确认。桌面封面参考包含原生窗口边框和标题栏，客户端边界尚未实测，因此不对封面强行给出相似度分数。证据包括 `export-output-content.png`、`export-output-verdict.json`、`export-batch.png`、`audio-dialog.png`、`cover-board-settled.png`。旧 APK 和配对证据另存于 `paired-export-20261005/`，保留失败历史。

测试后确认无导出服务，还原五个私有文件和原屏幕尺寸/density，恢复时逐字节一致。随后普通启动 PID `17670`，实际点击恢复后回到 Master 13、零位置和 546 个对象；有效已查看截图为 `ordinary-restored-complete.png`。初始 UIA 观察曾无根节点，点击恢复后的 `ordinary-restored-settled.png` 仍为过渡帧，均未据此重启，也不算恢复完成。最终文件、非更新偏好、旧偏好源、屏幕、安装包、实际 ARM64 库、正文、G 盘原谱面和原 v2 干净参考核对通过，所列有限运行时错误及当前 PID ANR 块未匹配；证据 `safe-area-native-20261005/final-verification.json`。

新候选的全页面 90% 相似度、物理 ARM 手机和平板、Android 12、后台/锁屏和长期性能仍待验收。此前两页的 92 分只属于对应旧候选和配对条件，不直接移用于本轮。`P5Accepted=false`、`pairedV2FidelityVerified=false`、`startupStabilityPassed=false`。未提交或推送；v2 最新更新仍按用户要求在 P5 用户验收及安卓提交推送之后补齐。

## 当前普通包的主编辑及谱面信息重新配对（2026-10-05）

使用普通 APK `d8c84ff5077bfd9af7c728dee4e230c762b4aab9b1721d8f801462875b0f4c3e`、无额外启动参数，实际 PID `14488` 对照既有真实 v2 主编辑与信息页截图。复用上一轮归档的桌面配置和背景素材，字节一致核对通过。Android 本轮系统栏及任务栏占用不同，按新鲜 XML 的应用边界调整测试自然尺寸到 `922×1294`、density 160；稳定内容边界 `[0,24][1294,862]`，两页均与参考为 `1294×838`，只裁切内容区，不拉伸或缩放。原始配置和屏幕参数另存并在测试后还原。

实际主编辑正文、安装包、加载的 ARM64 应用库以及两页几何核对通过；主编辑页与谱面信息页的结构化人工视觉审阅均为 92 分、`pass`。分数不是像素一致率，仅表示两页在本次条件下达到人工阈值，不能据此宣布全部主要页面达到 90% 或 P5 完成。细微差异仍包括背景透出、系统字体度量、信息表单行距及预览标题区高度。证据 `build-devtools/android-ui/paired-refresh-20261005/pairing-state.json`，两页 `*-verdict.json` 和已查看裁剪图同目录；完整来源、状态与限制见 `ANDROID_UI_PAIRING_ZH.md`。此前 88/89 分保留为旧候选历史记录。

测试后确认无导出服务，恢复偏好、QSettings 及原始屏幕尺寸/density；五个私有文件在恢复时逐字节一致。普通无额外参数冷启动 PID `15151`，已查看 `ordinary-restored-settled.png`，Master 13、零位置及 546 个对象恢复。最终文件、非更新偏好、旧偏好源、屏幕、安装包及 ARM64 库、正文、G 盘原谱面和原 v2 干净参考核对通过，所列有限运行时错误和当前 PID ANR 块未匹配。记录 `paired-refresh-20261005/final-verification.json`。首轮恢复校验脚本缺少本轮目录中的 APK 审计报告而中断，补齐实际 APK 审计后在同一 PID 复核通过；不据此重启应用或修改产物。

校准、视频与批量导出、封面画板及图层、音频和预览设置继续按相同条件配对；物理 ARM 手机和平板、Android 12、后台/锁屏与长期性能继续进行。整体 `pairedV2FidelityVerified=false`、`P5Accepted=false`、`startupStabilityPassed=false`。本轮没有提交或推送，最新 v2 更新仍遵循用户指定的后续顺序。

## 播放起点、停止定位与拖动暂停复核（2026-10-05）

本轮按既有 v2 的 `TIMELINE_COORDINATE_FOCUS_SPEC.md` 和播放 owner 实现传输行为，不同步最新 v2 更新。Android 预览显式保存本次暂停状态下开始播放的位置，时间轴入口标记使用这一位置；停止回到该起点，暂停和显式定位发出时间轴聚焦请求。播放中拖动进度条先暂停，松手保持暂停，用户再次点击播放才继续。工程替换重置播放起点。时间轴自身导航期间保留其已有视口规则，避免新的聚焦请求覆盖手势状态。

验证与证据：

- 新增 `MobileLatencySpec::stoppingReturnsToPlaybackEntryAndScrubStaysPaused` 使用实际预览、时间轴、编辑器与分析消费者，覆盖从 12 秒播放、45 秒暂停并聚焦、关闭进度跟随后停止仍返回 12 秒、拖动到 34 秒松手保持暂停和原起点不变。旧实现实际失败于时间轴起点为零，失败记录保留为 `transport-return-baseline-test-20261005.txt`；修正后该测试及原两项校准测试全部通过，5 PASS、0 FAIL、0 SKIP，记录 `transport-entry-focus-spec-20261005.txt`。
- 完整宿主 Release 构建通过，记录 `transport-entry-focus-host-build-20261005.log`；21/21 CTest 通过，6.25 秒，记录 `transport-entry-focus-ctest-20261005.log`。真实工作台在 `1280×720`、`854×393`、`1536×1024` 分别独立 seed/restart 通过，记录 `transport-entry-focus-workbench-20261005/run-6x4_wut5/verification.json`。现有音效实际设备 PCM 回归调整为拖动松手后先检查暂停，再显式播放；消费、暂停恢复、TouchHold、难度隔离和倍速检查通过，记录 `transport-sfx-host-20261005/run-n2y2dhgi/verification.json`。这是宿主音频验证，不能代表物理 Android 音画同步。
- Android Release 构建通过，记录 `transport-entry-focus-android-build-20261005.log`。本轮普通测试签名 APK 为 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,886,440 字节，SHA256 `d8c84ff5077bfd9af7c728dee4e230c762b4aab9b1721d8f801462875b0f4c3e`。92 个 ARM64 ELF LOAD 16KB、ZIP 16KB 对齐、签名和空 manifest 参数审计通过。
- Android 14 x86_64 模拟器 ARM 转译环境、默认 host 图形、原始 `1080×2220`、density 440、无 override，普通无诊断参数 PID `9559` 的实际截图已查看。校准试听在进度条可访问性 XML 中的开始位置为 `28.596931` 秒，播放至 `91.454` 秒后暂停，停止返回 `28.596931` 秒且时间轴聚焦起点。再播放并实际滑动，松手后两次独立观察均为 `58.916889` 秒且暂停；再次停止仍返回原起点。关闭校准页恢复原难度的 546 个对象，正文归一化 SHA256 与既有实际 v2 相同。记录 `build-devtools/android-ui/transport-native-20261005/transport-proof.json`，保留截图、XML 和日志。`entry-playing-a.png` 是开始播放后的过渡帧，不作为播放推进证据；后续实际播放截图 PID 一致。UIA 观察延迟和设备时钟变化不用于推算响应速度，性能尚未通过。

计数拍语义已根据现有 v2 owner 核对：校准页试听为持续测试 Tap，预备计数拍声音属于导出试听及离线导出链。此前校准记录中的“移动计数拍待补”应按此归属理解；当前移动导出试听的计数拍传输仍待补齐，不能把离线音频渲染已有的计数拍当作实时试听已完成。正常播放期间修改谱面时的冻结预览快照、校准退出时保留当前位置等传输细节继续核对。

确认导出服务已结束后，还原测试前会话、QSettings、谱面、音轨及偏好五个私有文件，恢复时逐字节一致。普通无额外参数冷启动 PID `14101`，实际点击恢复后回到 Master 13、零位置和 546 个对象；已查看有效截图 `ordinary-restored-settled.png`。最终会话、设置、谱面、音轨、非更新偏好、旧偏好源、屏幕参数、安装包、实际 ARM64 应用库及归一化正文核对通过；原 G 盘谱面和原 v2 干净参考提交保持不变，所列有限运行时错误和当前 PID 的 ANR 块未匹配。最终证据为 `build-devtools/android-ui/transport-native-20261005/final-verification.json`，有限检查不等于启动稳定性通过。

本轮功能截图使用用户原始配置，尚未进行新的统一尺寸 v2 成对评分，不能重算此前 88/89 分。物理 ARM 手机和平板、Android 12、当前包后台/锁屏回归、长期性能和主要页面 90% 验收继续进行；`P5Accepted=false`、`pairedV2FidelityVerified=false`、`startupStabilityPassed=false`。本轮未提交或推送。

## 校准页共享模型、实际试听场景与生命周期复核（2026-10-05）

继续在 Mobile 工作区推进既有迁移，原 v2 保持参考提交 `c190bb2c138cf61032dd7ac97ec41027da4bb40d` 且工作区干净，未同步最新更新。工作台直接装配已有 `ChartFieldSidebar`，恢复谱面信息、延迟校准和难度导航，校准页使用既有 QML 和 `LatencyModel`。新增 `LatencyAudition` 平台接口，使共享模型调用 Android 的实际预览与音频传输；`MobileLatency` 读取工程 BPM、首偏移和计数拍，检测或编辑结果写回对应元数据。试听文本使用已有 `LatencyTestChartBuilder`，通过临时预览源驱动对象、波形、播放线及统计，退出恢复实际难度。音效滑块使用既有校准音量换算，退出恢复正常混音设置。

共享检测模型增加非有限数值校验，并以校准上下文及音轨路径、大小、修改时间识别缓存。移动解码器处理 Qt 事件期间可能发生页面或工程切换，旧检测结果在返回后被拒绝。新增 `MobileLatencySpec` 使用可实际解码、能检测到节拍的脉冲 WAV 覆盖解码期间退出，以及真实文档、预览和时间轴的场景替换、元数据写回和恢复。

首个 Android 候选 `853f8aaf…` 虽然构建通过，实际校准页仍显示默认 120/0 和原谱面统计，不能视为功能通过。实际标签变化回调读到了尚未更新的派生状态，已改为监听 `latencyEditorActive` 的变化。候选包及审计另存为 `uncalled-lifecycle.apk` 和 `uncalled-lifecycle-apk-audit.json`。实际工作台回归新增打开校准页、检查试听源、切换信息页并恢复难度的消费者验证，避免仅凭独立模型测试漏掉装配问题。

验证与证据：

- 完整宿主 Release 构建通过；21/21 CTest 通过，6.18 秒，记录 `build-devtools/latency-port-ctest-20261005.log`。随后页面生命周期修正后的完整宿主构建通过，记录 `latency-lifecycle-host-build-20261005.log`；真实工作台在 `1280×720`、`854×393`、`1536×1024` 分别通过独立进程 seed/restart，包含新增校准场景切换检查，记录 `latency-lifecycle-workbench-20261005/run-sp90jynl/verification.json`。测试构造首轮编译失败及修复后的日志分别保留。
- Android Release 构建通过，记录 `build-devtools/latency-lifecycle-android-build-20261005.log`。当前测试签名 APK 为 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,886,440 字节，SHA256 `8d0764f98b56f253998b5c7197ae81c7f3e1e2fa54dbdcff4cc16f27a41565e1`。92 个 ARM64 ELF LOAD 16KB、ZIP 16KB 对齐、签名和空 manifest 参数审计通过；安装包哈希及加载的实际 ARM64 应用库核对通过。
- Android 14 x86_64 模拟器的 ARM 转译环境，默认 host 图形，无应用诊断参数，原始屏幕 `1080×2220`、density 440、无 override。实际 PID `8211` 的截图已查看：进入校准页读取 BPM `117.000`、偏移 `0.300`，1/4 试听为 201 个 Tap；实际滚动后可操作 1/8 和音量滑块，切换为 402 个 Tap、音量 30%。两张播放截图时间为 43 秒和 58 秒，实际预览对象、PV、波形和播放线推进，进程保持不变；停止回到零位置。真实音轨的两个自动检测分别显示 `117.000` 和 `0.300 秒`。关闭校准页恢复原难度及 546 个对象，实际编辑器正文归一化哈希与既有 v2 参考相同。
- 测试后确认无导出服务，还原测试前五个私有文件。普通启动 PID `9000` 的已查看有效截图为 `ordinary-restored-settled.png`；前一张 `ordinary-restored-final.png` 仍显示恢复对话框过渡，不用于恢复完成判定。会话、QSettings、谱面、音轨、非更新偏好、旧偏好源、屏幕参数、安装包及正文核对通过，G 盘原谱面与原 v2 保持不变；有限运行时错误及当前 PID 的 ANR 块未匹配。最终证据 `build-devtools/android-ui/latency-native-20261005/final-verification.json`，完整截图、XML、播放截图时间和各包审计保留在同目录。

实际停止试听后，时间轴视口仍停留在后段，关闭校准页也保留该视口，待按 v2 传输行为修正和复核。计数拍元数据已接入，但实际预备拍声音、音画同步及声道效果尚未验证，移动音效链的计数拍行为仍需补全检查。首次恢复后的中间截图与稳定截图区分，有限日志检查不代表启动稳定性已通过。

本轮没有重新完成同尺寸、同配置的 v2 成对评分，不能由组件复用或这些功能截图宣布达到 90%。物理 ARM 手机和平板、Android 12、当前包后台/锁屏回归及长期性能继续验收。`P5Accepted=false`、`pairedV2FidelityVerified=false`、`startupStabilityPassed=false`；本轮未提交或推送，最新 v2 更新仍按用户指定顺序留到之后。

## 时间轴纵向线、裁剪及播放中分隔条复核（2026-10-05）

本轮改动只在 Mobile 工作区完成，继续使用已经记录的 v2 参考。用户指定的最新 v2 更新补全仍安排在 P5 用户验收、安卓审阅提交并推送之后。

时间轴纵向线在实际 Android 中缺失的直接证据是：绘制状态已经要求显示播放线和光标线，但原 `QSGSimpleRectNode` 槽位的类型转换结果为 null，矩形仍为零尺寸，每次更新都会重新创建槽位。现在三个纵向线槽位由本模块的具体节点类型创建和识别。实际诊断 PID `6375` 的原始节点与转换后的节点地址一致，矩形变为 `2×109`；播放中多条记录保持同一节点且 `slotsRebuilt=false`。跨动态库的类型识别是解释方向，尚未证明全部平台的底层原因。普通无诊断参数 APK 的 PID `6752` 及还原后 PID `7220` 实际截图均已查看，橙色纵向线显示正常。

同时补齐三层场景图的裁剪几何。`setIsRectangular(true)` 只是优化提示，[Qt QSGClipNode 文档](https://doc.qt.io/qt-6/qsgclipnode.html)要求裁剪节点提供几何；祖先变换触发 stencil 路径时原空几何实际导致 D3D11 `updateClipState` 访问异常。共享工厂现在创建并持有四点矩形，尺寸变化时同时更新裁剪矩形、几何与 dirty 标志，尺寸不变时复用。新增像素绘制测试还暴露了软件绘制在调整裁剪高度时丢失祖先位移的问题，三个层的根节点改为恒等 `QSGTransformNode`，使 dirty 子树具有可恢复的变换状态。依据为 [Qt 6.11.1 软件节点更新器源码](https://raw.githubusercontent.com/qt/qtdeclarative/v6.11.1/src/quick/scenegraph/adaptations/software/qsgsoftwarerenderablenodeupdater.cpp)，实际软件及硬件测试均覆盖修正后的行为。

验证与证据：

- 完整宿主 Release 构建通过，`timeline-owned-slots-host-build-20261005.log`；20/20 CTest 通过，`timeline-owned-slots-ctest-20261005.log`，5.34 秒。新 `MobileTimelineOverlaySpec` 通过实际渲染像素检查隐藏后显示、上下裁剪、滚动补偿、裁剪高度变化和再次隐藏；普通、0.73 比例及 17° 旋转三种情形分别在软件绘制和实际 D3D11 下验证。旋转场景覆盖 stencil 路径。之前的空几何硬件异常、软件 resize 位移失败和首轮过严抗锯齿识别阈值记录均保留，最终测试日志与它们区分。
- 实际工作台三种尺寸的消费者 seed/restart 回归通过，记录 `build-devtools/timeline-clip-workbench-20261005/run-7h4jt2dk/verification.json`。最终节点类型修正后又完成完整宿主构建及上述 20 项测试；消费者记录来自同轮较早的裁剪修正版，不作为最终 APK 的原生操作证据。
- Android Release 构建通过，`build-devtools/timeline-owned-slots-android-build-20261005.log`。最终普通测试签名 APK 为 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,845,480 字节，SHA256 `70f13a3f4872fc775663ee1fb21ae64838f04445d8a6a65adb1ef1d49bbef471`。92 个 ARM64 ELF LOAD 16KB、ZIP 16KB 对齐、签名与空 manifest 启动参数检查通过。安装包哈希及实际 ARM64 应用库核对通过。
- 普通 APK 的 PID `6752` 在播放期间进行两次实际触摸拖动，预览边界从原始屏幕 x=1318 移至 1538，再移回 1318。已查看的后续截图显示播放位置继续推进、PV 和谱面对象变化，进程未变；停止按钮可以停止播放。两次操作紧随其后的截图仍是中间状态，稍后才出现最终尺寸，因此不判定即时响应、持续性能或原卡死根因已消除。一次停止操作仍沿用移动前的按钮坐标，实际点击到了编辑器；后续按新鲜截图中的真实按钮停止，没有改写谱面。播放时 UIA dump 曾超时，截图和实际播放继续推进，未据此重启应用。
- 测试后确认没有导出服务，只有预览比例及合法更新状态允许变化；反向拖动已将比例恢复 0.5，再还原测试前偏好字节并普通启动。最终 PID `7220` 的会话、QSettings、谱面、音轨、非更新偏好及旧偏好源文件均核对通过，屏幕为原始 `1080×2220`、density 440，无 override。实际正文归一化 SHA256 仍与 v2 参考一致，G 盘原始谱面未改动，原 v2 工作区干净且 HEAD 未变；有限运行时错误及当前 PID 的 ANR 日志块未匹配。这些有限检查不替代长期稳定性验收。

原生证据目录为 `build-devtools/android-ui/timeline-clip-native-20261005/`，有效最终截图 `ordinary-restored-final.png`，最终检查 `final-verification.json`。槽位诊断使用运行时 `--trace-render-loop`，普通最终启动没有该参数；默认图形环境保持 `host`。首个诊断候选曾因记录 null 节点时解引用而崩溃，已修正为 null 安全访问，候选包、日志和转储分别存档；最终安装包不是该候选。原包、单独裁剪修正版和类型修正前候选均另存，避免跨包混用。

本轮修复了实际纵向线显示问题，未重新进行同尺寸、同配置的 v2 成对截图评分，90% 验收仍未通过。校准页完整迁移仍待完成；实际手机和平板、当前包六项后台回归、锁屏、持续性能和启动稳定性继续验证。当前 `P5Accepted=false`、`pairedV2FidelityVerified=false`、`startupStabilityPassed=false`，本轮未提交或推送。

## 实际同尺寸界面对照与布局修正（2026-10-05）

实际 v2 参考窗口恢复可操作后，新增主编辑与谱面信息页参考，来源仍为 `c190bb2c138cf61032dd7ac97ec41027da4bb40d`，未进行 P5 后安排的最新 v2 同步。Android 临时对齐背景素材与透明度、字号/行距、皮肤、暂停判定线、时间戳、300% 时间轴和代码跟随；实际应用内容边界 `[0,66][1294,904]` 与桌面同为 `1294×838`，只裁掉系统栏，没有拉伸图片。两端真实编辑器可访问性正文归一化 SHA256 均为 `361e4fe874c146b4d3f5a6aba44b9169fb3c88b8cc78dd3aba4a95d28e3b74ca`。

按实际差异完成以下修正：

- `WorkbenchSettings` 保留保存的点字号，另提供 Android 绘制字体，以 v2 工作台逻辑像素换算后再交给已有设备比例变换。正文、行号、补全通过共享 `Theme.codeFont` 使用同一绘制字体；宿主平台继续返回原字体。实际消费者检查区分保存字号与绘制字体，避免以点字号假设 Android 的实际像素尺寸。
- 恢复、保存等短提示放入现有状态栏，五秒后恢复工程路径，移除持续占用高度的额外提示行。预览模式标题复用 v2 的已有翻译。
- 信息页的底部时间轴和工具栏开关遵循 v2 的难度/校准活动标签规则。Android 原有信息页保留时间轴，会挤压 PV 及其他字段；修正后实际截图及可访问性验证确认时间轴隐藏、开关禁用、PV 两项操作可见，切回 Master 后时间轴重新显示。

验证与证据：

- 宿主完整 Release 构建通过，18/18 CTest 通过；实际工作台 `1280×720`、`854×393`、`1536×1024` 的 seed/restart 和小屏六页首选项回归通过。日志 `build-devtools/ui-pair-font-host-build-20261005.log`、`ui-pair-ctest-20261005.log`，实际消费者记录 `ui-pair-workbench-20261005/run-3awkbe7d/verification.json`。信息页布局修正后的完整宿主构建通过，真实分隔条与独立重启在 `1294×838`、`854×393` 回归通过，记录 `ui-pair-layout-20261005/`。
- Android Release 构建、92 个 ARM64 ELF LOAD 16KB 审计、ZIP 16KB 对齐和测试签名检查通过，manifest 启动参数为空。最终普通 APK SHA256 `fbcf4d76f0b00c0b37f36f98b129fb1e0fa36ca86e29c23697b90e347577a1dd`，99,841,384 字节；日志 `build-devtools/ui-pair-metadata-android-build-20261005.log`。实际 PID `696` 的安装包哈希一致、ARM64 应用库已加载，普通 APK 主编辑和信息页截图已查看。
- 证据 `build-devtools/android-ui/paired-native-20261005/final-pairing-state.json`。有效最终截图为 `main-final-settled.png` 与 `metadata-after.png`；此前 `main-final.png` 仍处于恢复对话框过渡，不能当作恢复完成证据。字体修正首个 APK `6ce04f…` 的审计和包另保留，原始对照 APK `d317ca…` 也已存档，避免跨包混用。
- 视觉审阅主界面从 84 至 88，信息页从 78 至 89，均为 `revise`。评分是结构化人工视觉审阅，不是像素一致率；校准导航、时间轴纵向播放线、界面字体度量和背景区域对照仍待修正，不能宣称 90% 达成。
- 配对测试后还原五个私有文件并核对逐字节一致，移除屏幕尺寸/density override。随后普通冷启动 PID `1498` 实际出现 ANR；系统主线程转储显示等待硬件绘制，Qt 启动线程等待剪贴板 UI 注册，同期 Launcher 也有输入超时。原始截图、logcat 和对应转储保留；原因未确认，启动稳定性未通过。确认没有导出服务且会话、QSettings、谱面、音轨、非更新偏好及屏幕参数一致后，重启测试模拟器进行恢复检查，未改用绕过启动或渲染的参数。

本轮为 Android 14 x86_64 模拟器的 ARM 转译观察，尚未代表物理 ARM 设备。最终 APK 的六项后台回归尚未运行，旧包证据不替代新包；`P5Accepted=false`、`pairedV2FidelityVerified=false`，没有提交或推送。最新 v2 更新仍留在用户 P5 验收、安卓提交及推送之后。

启动恢复的后续观察：仅重启 Android 客体后，PID `3832` 的 MiaCode 有限错误模式未匹配，但实际屏幕仍出现 Pixel 启动器无响应，不能证明界面恢复。重启后 adbd 回到非 root 状态，首次私有文件读取因权限不足而失败；恢复原诊断访问方式后核对通过，不是偏好文件丢失。报告另存为 `ordinary-after-guest-reboot-verification.json`。

随后关闭同一 AVD 的主机进程进行冷启动诊断，保留用户数据分区且未编辑 `config.ini`。第一次启动未设置 SDK 根而找不到系统镜像，原始失败日志保留；从原 `hardware-qemu.ini` 找到实际 SDK 根后，临时 `swiftshader_indirect` 渲染环境下 PID `4345` 可以恢复 Master，实际截图却出现图标和画布三角形绘制损坏，因此软件渲染诊断没有通过。恢复默认图形环境，实际生成配置为 `hw.gpu.mode=host`，取消软件 GPU 参数。普通 APK 无额外应用参数启动为 PID `4195`，主编辑器恢复、谱面信息页打开和关闭均响应，元数据页底部开关禁用；最后截图 `ordinary-default-cold-final.png` 已查看，未见软件诊断中的绘制损坏。实际正文归一化 SHA256 仍与 v2 参考一致，会话、QSettings、谱面、音轨、非更新偏好、旧偏好源文件及安装包核对通过；尺寸 `1080×2220`、density `440` 无 override。证据 `ordinary-default-cold-verification.json`。

这次冷启动恢复不证明此前 ANR 的根因已消除，也不证明长期响应、拖动稳定性或真机性能。软件渲染观察与默认图形恢复分别保留，最终继续 `startupStabilityPassed=false`，时间轴纵向播放线及界面相似度待项保持开放。

## 时间轴视图设置恢复与配对状态准备（2026-10-05）

实际 v2 的时间轴缩放、波形亮度、小节线亮度和代码跟随保存在 `app.preview`。此前 Android `MobileTimeline` 只有运行时桥接，未读取和回写这四项；重启会丢失选择，旧设置迁入后也不能影响实际时间轴。本轮在同一桥接层恢复并保存原有键，启动恢复完成后才连接持久化信号，每次保存合并最新文档，保留其他应用设置、移动端帧率文档及未知字段。缩放预设和亮度归一化继续由实际 `TimelineQuickStateBridge` 负责；启动时的视口锁定使用 v2 的固定启用行为。

验证与产物：

- 完整宿主 Release 构建和 18/18 CTest 通过，日志 `build-devtools/host/timeline-canonical-preferences-build-20261005.log`、`timeline-canonical-preferences-ctest-20261005.log`。实际工作台消费者在 `1280x720`、`854x393`、`1536x1024` 三种尺寸 seed/restart 均通过；检查旧文档的 300% 缩放、亮度和跟随恢复，修改后的独立进程恢复，以及 `app.preview`、`app` 和根帧率文档的未知字段保留。记录 `build-devtools/timeline-canonical-workbench-20261005/run-lyhher0n/verification.json`。首轮候选测试仅使用了根 `preview`，虽然通过，却没有验证 v2 原文档位置；最终记录以 canonical 目录为准。
- Android Release 构建通过，日志 `build-devtools/timeline-view-preferences-android-build-20261005.log`。当前普通测试签名 APK 为 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,841,384 字节，SHA256 `d317ca4830044978de9990b01dd12567d91ec8c8008e14ba026fd2a9b91a19c2`。92 个 ARM64 ELF LOAD 16KB、APK ZIP 16KB 对齐、签名与空 manifest 参数检查通过。此前后台导出通过包另存为 `build-devtools/android-ui/cover-window-default-native-20261005/ordinary-window-guard.apk`；其六项后台证据不直接算作新包后台回归。
- Android 14 模拟器实际控件先显示 300% 和勾选的代码跟随。通过真实缩放菜单选择 150%，关闭跟随，并拖动两个亮度滑块到波形 90%、网格线 65%。PID `31256` 未变，偏好差异严格只有 `app.preview` 内上述四个字段。独立冷启动为 PID `31699`，主偏好文件逐字节一致，实际控件恢复 150%、未勾选、90% 和 65%；`restart-brightness-settled.png` 已查看。记录 `build-devtools/android-ui/timeline-view-native-20261005/native-changed.json` 和 `native-restart.json`。
- `initial.png` 及 `restart-brightness.png` 是操作后尚未完成的过渡观察，不能当作恢复完成或亮度弹窗已打开的证据；使用已查看的 `initial-settled.png`、`brightness-changed.png` 和 `restart-brightness-settled.png`。测试完成后恢复原偏好文件，会话、QSettings、谱面、音轨和主偏好五项在恢复时逐字节一致，记录 `restoration.json`。最终普通无额外参数冷启动及已安装 APK/实际 ARM64 库/有限日志模式检查记录为 `final-verification.json`；启动后的合法更新状态与恢复时字节一致分别核对。

为准备成对截图，解析此前实际 v2 主编辑器的可访问性正文和当前 Android 工程的 Master 正文，去除显示用换行和空白后 SHA256 同为 `361e4fe874c146b4d3f5a6aba44b9169fb3c88b8cc78dd3aba4a95d28e3b74ca`。只读核对桌面设置中的 300% 缩放、代码跟随和背景配置，记录 `build-devtools/ui-parity-state-20261005.json`。当前桌面配置不能自动视为旧截图的全部配置证明。Computer Use 对实际 v2 窗口报告最小化，恢复绑定及激活时返回 `failed to activate captured window`；停止使用该窗口的旧坐标。尚未取得本轮相同内容尺寸、背景、字体和暂停显示配置的完整配对，不计算 90% 通过分数。

最终普通进程 PID `31915` 已恢复 `Master 13` 主编辑器，`final-ready.png` 已查看。安装包哈希、真实 ARM64 应用库、会话、QSettings、谱面、音轨、旧偏好源文件及非更新偏好字段检查均通过；有限错误模式未匹配。G 盘原始谱面 SHA256 再次核对为 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`，原 v2 工作区仍干净。新 APK 的六项后台回归尚未重跑，最终报告明确保留该范围。

以上补齐当前参考 v2 已有的时间轴设置链路，未同步最新 v2 更新。P5、90% 成对界面验收、真实 ARM 手机和平板、锁屏和持续性能继续进行；本轮未提交或推送。

## 封面批量后台完成与前台重绘恢复（2026-10-05）

Android 主窗口现增加实际 Activity 生命周期保护。在封面批量任务运行期间，Activity 已暂停时拦截主窗口的正向 Expose 和 UpdateRequest，避免嵌套事件处理进入不可见窗口的渲染等待；真正的窗口失去曝光事件、离屏捕获窗口及正常前台更新继续使用原有生命周期。已拦截状态保持到前台恢复，再请求一次重绘，覆盖队列完成后仍留在后台的情况。保护由普通启动装配，Qt 渲染循环与硬件解码保持产品默认设置。

定位依据包括实际后台阶段日志和 [Qt 6.11.1 Android 窗口源码](https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/plugins/platforms/android/qandroidplatformwindow.cpp)。该源码可能在 surface 变化时发出曝光事件；[QPlatformWindow 源码](https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/gui/kernel/qplatformwindow.cpp) 在分发 UpdateRequest 前清除 pending 标志。生命周期竞争是结合源码与日志的解释；实际通过范围以下述普通 APK 的后台导出和前台恢复证据为准。

本轮验证：

- 完整宿主 Release 构建通过，日志 `build-devtools/host/cover-window-lifecycle-build-20261005.log`。18/18 CTest 在具备 Qt 运行库的环境中通过，日志 `cover-window-lifecycle-runtime-ctest-20261005.log`，总耗时 2.31 秒。首次测试环境缺少 Qt bin 路径，首个测试未进入运行，原日志保留；不能算作产品测试失败或通过。
- Android Release 普通包构建通过，日志 `build-devtools/cover-window-default-android-build-20261005.log`。当前测试签名 APK 位于 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,837,288 字节，SHA256 `62fcf3d66719659a81e19d5cc1a127b699babff178a9b2aed07dfabc16e83b24`。92 个 ARM64 ELF LOAD 16KB、APK ZIP 16KB 对齐、签名检查通过，manifest 启动参数为空。
- 普通 APK 的实际批量对话框选中当前布局、四个内置预设和旧自定义 `CoverQA`。先从真实日志确认六项队列准备完成，再发送 Home；PID `29959` 全程一致。约 95.360 秒时同时观察到六个新增公共 PNG 和服务结束，六次发布均成功；逐张保留真实 PNG、大小和 SHA256，完成全部记录的实际耗时为 98.141 秒。`queue/home.png` 已查看，为 Android Launcher，此时没有返回应用。证据 `build-devtools/android-ui/cover-window-default-native-20261005/queue/verification.json`。
- 返回前台仍为 PID `29959`，实际弹窗显示已处理 6/6。按新鲜弹窗边界关闭批量窗口，再关闭封面工作区；实际视频导出页能够播放。已查看的 `resume-playing-1.png` 与 `resume-playing-2.png` 显示播放位置从 00:02.161 推进至 00:07.490，谱面对象变化且 PV 视频恢复显示；`resume-stopped.png` 显示停止后归零。这是前台重绘和播放基本恢复验证，不是持续帧率、音画同步或播放中拖动验收。播放时一次 UIA dump 超时，未据此重启应用。
- 独立诊断候选也在同一后台进程完成六项，证据 `build-devtools/android-ui/cover-window-lifecycle-native-20261005/continued/verification.json`。诊断日志中队列准备到最后一次发布约 241 秒，性能不能算作通过。普通包结果来自新的独立测试目录，不能用候选记录替代。
- 确认服务已结束后，还原测试前偏好设置；会话、QSettings、谱面、音轨和主偏好文件五项在恢复时逐字节一致。随后普通无额外参数冷启动为 PID `30918`，安装包哈希一致且实际 ARM64 应用库已加载；旧偏好源文件、会话、QSettings、谱面、音轨及非更新偏好内容保持一致，迁移标记保留，所列有限启动错误模式未匹配。证据为同目录的 `restoration.json` 和 `final-verification.json`。首次最终记录脚本用系统 GBK 读取 UTF-8 报告失败；明确编码后在同一次启动上复核通过。

新增 `scripts/build/validate-mobile-cover-background.py` 使用单调时钟记录真实经过时间，并要求同 PID、指定数量新增成品、实际成功发布和权威服务结束同时成立。服务查询的 DUMP TIMEOUT、命令超时及无有效头部均记为未知，不作为服务停止；本次普通运行实际包含文件查询超时及服务查询未知记录。历史脚本的 2 秒轮询计数没有计入 ADB 命令耗时，因此此前记录的 12/24 秒观察不能用作实际耗时结论；历史失败和原始证据保留。

本轮完成 Android 14 x86_64 模拟器 ARM 转译环境的六项后台封面导出和上述前台恢复路径。真实 ARM 手机和平板、锁屏、取消、禁止后台、长任务性能及完整成对 UI 验收继续进行；`P5Accepted=false`、`pairedV2FidelityVerified=false`。原 v2 HEAD 仍为 `c190bb2c138cf61032dd7ac97ec41027da4bb40d`，工作区干净。本轮没有提交或推送，最新 v2 更新仍在用户 P5 验收、安卓提交及推送之后补全。

## Android 旧偏好文件迁移与封面后台复核（2026-10-05）

普通 APK 的实际封面页复核发现，切换到应用私有 `files/preferences.json` 后，原 Qt 配置目录 `files/settings/preferences.json` 内的封面预设、输出目录、素材设置和视频导出设置没有迁入。旧文件仍存在，但新文档的 `app` 为空，实际批量封面对话框因此缺少 `CoverQA` 和用户已选的 SAF 输出目录。这属于移动端既有设置兼容性修复，保持 P5 后再同步 v2 最新更新的顺序。

`PreferenceDocument::migrateFromFile` 现使用原有原子保存入口，递归导入缺失字段；新文档已有的值、数组和显式 null 保持优先。`android_config_preferences_v1` 标记与迁入内容一起保存，后续启动不会重新导入已主动删除的预设。源文件保持不变。Android 普通启动在构造设置消费者之前执行迁移，指定隔离存储根的测试入口保持隔离。损坏的源或目标文档会保留并报告错误，当前启动路径返回错误码 63；面向用户的恢复提示还需完善。

本轮验证：

- 完整宿主 Release 构建及 18/18 CTest 通过，日志 `build-devtools/host/preference-path-migration-build-20261005.log`、`preference-path-migration-ctest-20261005.log`。新增 `MobilePreferenceMigrationSpec` 覆盖缺失源文件、目标已有值优先、嵌套字段导入、未知字段、显式 null、源文件不变、迁移后删除不复活、首次目标创建及损坏文档保护。
- 实际六页首选项和消费者的 `854x393` seed/restart 回归通过，记录 `build-devtools/preference-path-migration-ui-20261005/run-wa7ubhbw/verification.json`；这项是宿主验证，不代替触摸及设备适配验收。
- Android Release 构建通过，日志 `build-devtools/preference-path-migration-android-build-20261005.log`。本轮普通测试签名 APK 位于 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,833,192 字节，SHA256 `99021e5f28d23b1876d451ca69b2a1b11e2a94054499106500b136cd90cd407b`。92 个 ARM64 库 ELF LOAD 16KB、APK ZIP 16KB 对齐和签名审计通过，manifest 启动参数为空。实际 16KB 设备和发布签名继续验收。
- Android 14 模拟器普通 APK 的实际封面批量对话框恢复 `CoverQA` 和 `Download/000MiaCodeCover20261003-b8973975` 输出目录，截图 `restored-batch.png` 已查看。通过新鲜可访问性节点选中当前布局、四个内置布局及旧自定义预设，日志确认六项队列准备完成后才发送 Home。旧脚本 12 次名义 2 秒间隔观察结束时只有 1 张 PNG 落盘，服务仍运行，`completeSixWhileHome=false`；命令耗时未计入，不能称为实际 24 秒。
- 回到前台后，同一 PID `27419` 最终完成六项发布，六次 `publication settled` 均为成功，服务结束。六个输出另存为 `export-0.png` 至 `export-5.png` 并记录 SHA256；已查看旧预设和纯谱面帧产物。这证明旧预设进入实际导出流程，不代表封面整体视觉验收通过。期间 ADB 文件、PID 和截图命令曾超时，模拟器控制台仍报告运行；重连调试传输后恢复观察，没有重启应用或中止导出。原超时记录保留。前台恢复后的延迟也偏长，后台六项完整导出和性能仍未通过。
- 队列完成且服务结束后才还原测试前五个私有文件，恢复时逐字节一致。普通无额外参数冷启动 PID `28409` 后，旧源文件、会话、QSettings、谱面和音轨仍精确一致；新偏好文件按产品迁移规则导入旧封面/视频设置，原界面字段保持优先。安装包 SHA256 与产物一致，实际 ARM64 应用库已加载，有限启动错误模式检查通过。首次验证脚本误把应用库名写成 `MiaCodeMobile`，保留失败报告；改为读取部署 manifest 的真实 `MiaCodeAndroid` 名称，在同一 PID 上复核通过，没有因此重启。

本轮原生证据目录为 `build-devtools/android-ui/preference-path-migration-native-20261005/`，包含 APK 审计、后台与前台日志、实际文件和服务观察、`restoration.json`、`final-verification.json` 及最终恢复主界面截图。后台诊断使用普通 APK 的运行时 `--trace-render-loop` 参数；最终启动不带额外参数。模拟器为 x86_64 环境中的 ARM 转译，不能代表真实 ARM 手机和平板。P5、90% 成对界面验收、持续性能和后台导出仍未完成。本轮没有提交或推送。

## 六页偏好设置、独立画面节奏及实际设置恢复（2026-10-05）

Android 首选项现直接装配 v2 的 `PreferencesDialog`，包含界面、背景、编辑器、性能、快捷键和更新六页。原素材导入入口保留为独立素材窗口；MiaCode 菜单进入首选项，工具入口继续打开素材窗口。界面页加入实际后台导出允许开关，仍使用已有 Android 会话与前台服务策略。本轮原生测试没有改动用户的后台导出授权。

新增 `MobilePreferencesStore`，通过实际 `PreferencesModel` 将字号、行距、输入方式、自动补全、左右面板、三个帧率与解码选择传给工作台、输入控制器及播放器。输入方式的两个标志合并保存，保留第一个不持久化 setter 的运行时值，避免第二个 setter 用旧文件覆盖它。保存继续合并私有 `preferences.json`，保留未识别字段。Android 新安装缺少明确输入法选择时默认允许软键盘，用户已有选择继续保留。左右预览使用实际 `SplitView.moveItem`，左侧分隔条的边界和拖动方向随面板位置计算。

画布、PV 和时间轴各自使用独立画面节奏，传输与音效时钟继续运行。PV 解码写入独立 `QVideoSink`，按设置合并为最新帧，再提交两个实际输出；暂停定位即时更新，同渲染上下文的全屏切换保留暂停帧，释放及清空覆盖两个输出。渲染上下文初始化/失效连接按所用 Qt 6.11.1 的实际实现处理。软件解码策略在首次构造多媒体对象之前应用；改变此项显示明确的重启提示。帧率逻辑及真实 sink 单元检查不等同于原生硬件 PV、锁屏恢复或持续性能验收。

语言接入实际 `LocaleService`，QML 页面和时间轴标签即时重译；语法分析在同一文档版本切换语言时拒绝旧语言结果。实际快捷键注册表和绑定组件已装配，首选项显示时关闭全局绑定，录制时接收 ShortcutOverride。更新页接入原有网络获取、状态存储及 `UpdateService`，平台明确为 `android-arm64`；普通启动遵循用户的自动检查设置。当前版本标识仍来自移动项目已有构建配置，Android 线上发布包、真实外接键盘快捷键录制及全部移动端文本翻译继续验证。

验证与产物：

- 完整宿主 Release 构建通过，17/17 CTest 通过；日志 `build-devtools/host/preferences-port-rhi-build-20261005.log`、`preferences-port-final-ctest-20261005.log`。覆盖真实更新服务/清单/版本、同版本分析语言竞争，以及新增画面节奏和真实 `QVideoSink` 的暂停、帧合并、双输出、切换及释放检查。
- 六页实际 QML 控件及消费者的三比例 seed/restart 通过，尺寸 `1280x720`、`854x393`、`1536x1024`，记录 `build-devtools/preferences-dialog-ui-final-20261005/run-2kwjo_zu/verification.json`。检查中日即时切换、左右面板的实际位置、20pt 字号与实际 8px 附加行距、三种输入模式、自动补全、三种帧率消费者、解码重启提示、快捷键注册表、更新服务注入及未知字段保留。这一宿主路径调用控件处理函数，`nativeTouchVerified=false`，不能替代真实触摸/软键盘验收。部分 Android 外壳文字仍为中文，不能据此宣称整应用多语言完成。
- 当前完整设置装配后的三比例真实指针及独立重启布局回归通过，记录 `build-devtools/layout-preferences-regression-20261005/run-uwzf08vj/verification.json`。G 盘原始谱面 SHA256 保持 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。
- 首轮普通 ARM64 Release 测试签名 APK SHA256 `f85fb93d9fd292109aac2986b1387dc8c553a1122057f72287af176c9c92cab2`，99,833,192 字节，原包另存为 `build-devtools/android-ui/preferences-native-20261005/before-cadence-fix.apk`，对应以下首轮原生记录。构建日志 `build-devtools/preferences-port-android-build-20261005.log`；ELF LOAD 16KB、APK ZIP 16KB 对齐与签名检查均通过，启动参数为空。最终修复包见下文，发布签名未验收。
- 普通 APK 在 Android 14 模拟器通过实际触摸打开六页、切换左侧预览、三种输入模式、自动补全开关及画布 30 FPS。左侧分隔条滑动使比例 `0.5 → 0.44490135703651496`，只改变 `ui.preview_width_ratio`。冷启动 PID `24475 → 25616`，偏好文件一致，恢复原工程后实际编辑器开关和面板位置正确。`restart-start.png` 仍含恢复提示，实际完成截图使用已查看的 `restart-settled.png`；编辑器、性能、快捷键、更新和背景页面截图均已查看。证据位于 `build-devtools/android-ui/preferences-native-20261005/`。
- 测试末尾停止应用后还原原始偏好文件，会话、原有 QSettings、谱面、音轨与偏好设置五个文件逐字节一致，记录 `restoration.json`。普通再次启动可能写入产品规定的 Android 缺省输入法标志和更新状态；必须区分恢复时的精确一致与启动后的合法迁移，不能用启动前的校验宣称启动后偏好字节不变。最终普通运行、已安装包哈希及有限日志检查另记于同目录 `verification.json`。

中间失败证据保留：Windows 更新测试可执行文件未嵌入 manifest 导致 CreateProcess 740，修复测试目标的嵌入及 asInvoker 配置后全部通过；初版 LayoutMirroring 没有让实际面板换位，改用 v2 的 moveItem；字号检查曾把真实 8px 行距误写为 6px，仅修正检查期望；新增翻译 ID 的 TS context 最初不匹配 Qt 的 ID 查询，修复三个目录后截图显示实际译文。对应首轮构建/CTest 日志及 `preferences-dialog-ui-20261004/run-w8sdhxeu`、`preferences-dialog-ui-repaired-20261005/run-z88tkl97` 等记录仍保留，失败报告内的能力字段不能单独算作通过。

首轮原生冷启动截图另发现隐含时间轴帧率的恢复差异：尚未单独设置时间轴帧率时，修改画布为 30 FPS，运行中时间轴保持 60 FPS，重启却按新画布默认值变成 30 FPS。首轮偏好文件一致及显式模式测试没有覆盖这个缺省场景，不能算作完整帧率恢复通过。保存画布模式时现把当前隐含时间轴模式写入同一个合并文档，保证两者独立。增加实际控件回写的序列断言；失败报告的 `controlHandlersVerified` 也改为随真实结果返回。修复后的完整宿主 Release、17/17 CTest 和三比例六页 seed/restart 全部通过，日志 `preferences-default-cadence-build-20261005.log`、`preferences-default-cadence-ctest-20261005.log` 位于 `build-devtools/host/`，UI 证据 `build-devtools/preferences-default-cadence-ui-20261005/run-_8xz7k18/verification.json`。

最终普通测试签名 APK 为 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,833,192 字节，SHA256 `7a744dc311260c8e22497abbdc62622e193c613aef421e796159d420adacd678`。Android Release 构建日志 `build-devtools/preferences-default-cadence-android-build-20261005.log`；92 个 ARM64 库 ELF LOAD 16KB、APK ZIP 16KB 对齐、签名及空启动参数审计通过。独立普通 APK 冷启动回归 PID `26355 → 26674`，实际性能页先显示画布/PV/时间轴 `60/30/60`，只修改画布后显示 `30/30/60`，冷启动仍为 `30/30/60`；最终截图已查看。首次启动后的即时 PID 观察过早而失败，稍后观察同一次启动得到有效 PID，没有因此再次重启。测试末尾五个原始文件已精确还原；证据使用独立目录 `build-devtools/android-ui/preferences-cadence-default-native-20261005/`，保留首轮文件和截图。最终普通进程 PID `26966` 已恢复 `Master 13`，`final-ready.png` 已查看，实际安装包哈希一致且映射 ARM64 应用库。启动后会话、原有 QSettings、谱面、音轨与非更新偏好内容保持一致，所列日志错误模式未匹配；记录 `verification.json`。原 v2 HEAD 和干净工作区也再次核对一致。

本项完成六页装配与上述消费者/恢复路径。软键盘与完整输入操作、全语言文本、原生硬件 PV/全屏/后台生命周期、持续帧率、播放中拖动、音频校准、封面后台批量、物理手机和平板及 v2 同配置成对截图仍须继续。`duringPlaybackDraggingVerified=false`、`sustainedFrameRateVerified=false`、`pairedV2FidelityVerified=false`、`P5Accepted=false`。未提交或推送，v2 最新更新继续按顶部顺序执行。

## 工作台分区比例、触摸拖动与布局恢复（2026-10-04）

Android 主工作区移除临时的侧栏 17%、预览 32%、底部 30% 分配，改为实际 `WorkbenchSettings` 的侧栏宽度、预览/编辑可用区比例和底部高度比例。沿用既有 v2 `MainSplitView` 的边界约束、侧栏收起阈值及完成拖动后保存的语义；侧栏与底部工具栏开关也进入同一个持久化所有者。`Binding` 在拖动结束后重新接入持久比例，缩小窗口触及控件下限后再放大仍恢复用户比例。画面缩放预算覆盖整个侧栏可调范围，不随拖动宽度变化，视觉和触摸坐标共用同一个变换。

原生复核发现预览分隔条在首版普通 APK 的触摸和鼠标操作后没有保存比例，尽管宿主指针测试通过。仅把保存移到 `SplitView.resizing` 结束信号也未解决。最终将预览手势带放到两个面板所有者上方，从实际边界计算位置及拖动距离，使用已有 `Theme.splitHandleHitExtent`，拖动时仍由 `SplitView` 执行尺寸约束，松开后写入比例。Qt 原有分隔条继续承担视觉样式；工作区没有新增固定像素偏移来补偿触摸。首版及中间失败证据分别保留于 `build-devtools/android-ui/layout-native-20261004/` 和 `layout-release-native-20261004/partial-result.json`，不计作通过记录。

实际验证与产物：

- 完整宿主 Release 构建通过，12 项 CTest 通过；日志为 `build-devtools/host/layout-pointer-build-20261004.log`、`layout-pointer-ctest-20261004.log`。编译、打包均按单次构建、最多四任务执行。
- 新增 `--layout-ui-smoke` 和验证脚本的 `--layout` 选项。经真实 `QQuickWindow` 指针事件完成三处分隔条拖动，核对拖动中配置文件字节不变、缩放变换稳定、松开保存、侧栏拖动收起、工具栏折叠底部、缩小与放大恢复，以及独立进程重建后重新展开的尺寸。五组逻辑尺寸 `1280x720`、`960x720`、`1120x480`、`854x393`、`1536x1024` 的 seed/restart 均通过，证据 `build-devtools/host/layout-pointer-ui-20261004/run-vz1m3p76/verification.json`。已查看短屏手机、4:3 及大窗口恢复截图；宿主矩阵不替代物理设备或 Android 触摸验证。
- 编辑器字号/行距、输入标志、主题及预览隐藏状态的三比例独立进程回归通过，证据 `build-devtools/host/layout-workbench-regression-20261004/run-ke2t4npc/verification.json`。G 盘原始谱面哈希保持 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。
- 最终普通测试签名 APK 为 `build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，99,743,080 字节，SHA256 `bc6ce0b36592860bc09b8d31186779e48e916f9f747d6293f2e049ce17556103`。原生 Release 构建与打包通过，92 个原生库 ELF LOAD 16KB、APK ZIP 16KB 对齐和签名验证通过；普通启动参数为空。实际已安装 APK 的设备端 SHA256 与产物一致。
- 普通 APK 在 Android 14 模拟器中从实际可访问节点定位分隔条，执行触摸滑动，侧栏 `190 → 237`、预览比例 `0.5 → 0.42286189985112116`、底部比例 `0.35 → 0.4403018846308072`；每步只改变相应字段。重启 PID `23374 → 23663`，配置完全一致。`restart-main.png` 仍是恢复提示过渡帧，不计作恢复完成截图；重新观察节点后采集的 `restart-ready-main.png` 已查看并显示原工程。记录 `build-devtools/android-ui/layout-pointer-native-20261004/verification.json`。
- 额外播放观察中，实际谱面、PV 画面和时间轴继续更新，截图显示 00:35 与 01:22 的不同状态。播放时无障碍树暂时只返回原生容器，暂停标签断言未通过；额外拖动后的即时文件断言也早于配置更新，后续观察已得到新比例 `0.48047475273624096`。这些观察及日志保留，不作为完整播放中拖动、音频同步或性能验收通过。最终恢复采集时第一次节点断言未通过，第二次对同一 PID 重新观察得到 44 个标签、无恢复提示且有 `Master 13`；没有因观察失败重启该进程。
- 测试结束恢复初始偏好设置和原工程，最终 PID `23950`，`final-main.png` 已查看。会话、原有 QSettings、谱面和音轨逐字节保持一致，工作台偏好也已还原。最终进程日志未匹配记录所列 QML/崩溃模式，该有限检查不替代稳定性验收。模拟器尺寸和密度没有变更。

本项完成上述布局及暂停状态触摸路径。完整六页偏好设置、校准与其余功能迁移、播放/解码与性能矩阵、封面后台批量、物理手机和平板以及相同配置的 v2 成对截图仍须继续完成。`duringPlaybackDraggingVerified=false`、`playbackPerformanceVerified=false`、`pairedV2FidelityVerified=false`、`P5Accepted=false`。未提交或推送；v2 最新更新继续按本文顶部顺序留待用户 P5 验收以及安卓成果提交、推送之后。

## 工作台偏好设置的真实消费者与私有存储（2026-10-04）

Android 主界面已用实际 `WorkbenchSettings` 替换临时 QML 设置对象，`Theme`、`SourceEditor` 和相关视图直接读取该持久化模型。主装配把编辑器输入方式、覆盖模式及自动补全传给实际 `EditorController`，后续设置变化也通过原有信号更新。工程导入字体通过字体族覆盖保留，字号和行距仍由共享偏好设置控制；清空工程字体后恢复内置字体。

`Shared.cpp` 中四个字体/行距辅助函数提取到独立 `common/EditorAppearance.h`，保留现有命名空间。逐项与既有 v2 基准 `c190bb2c138cf61032dd7ac97ec41027da4bb40d` 的函数体核对一致；旧位置的重复定义已移除。证据 `build-devtools/host/workbench-helper-provenance-20261004.json`。内置字体使用原 v2 `fonts.qrc` 资源，已删除主装配中重复的字体注册。`PreferenceDocument` 增加平台文件路径入口，Android 在构造消费者前绑定私有工作目录的 `preferences.json`；保存继续采用原有合并和原子写入，背景/音视频的既有存储边界继续保留。

验证范围与证据：

- 完整宿主 Release 构建通过，12 项 CTest 通过，包括新增的设置与实际输入事务测试。全角键盘输入与粘贴分别核对原有 v2 策略；导入字体保留字号、清除覆盖、设置重建恢复及其它字段保留通过。
- 三种逻辑窗口尺寸 `1280x720`、`960x720`、`1120x480` 各执行独立 seed/restart 进程。核对真实 QML 编辑字号、真实文本块的 `LineDistanceHeight` 行距、输入控制器及视图/主题恢复；原始 G 盘谱面哈希保持一致。记录 `build-devtools/host/workbench-ui-20261004/run-tctxd8d7/verification.json`；已查看初始以及三种比例的恢复截图。初次探针错误地以段落下边距核对行距，失败记录 `run-90thc55b` 保留；确认实际 v2 使用附加行高后修正探针，产品样式逻辑没有为此修改。
- 背景三比例及冷启动回归通过，记录 `background-workbench-regression-20261004/run-ebc_vca7/verification.json`；先前预览/音频的三进程持久化检查通过，记录 `preferences-workbench-regression-20261004`。构建和 CTest 日志均在 `build-devtools/host/workbench-*.log`。
- 第一轮普通 ARM64 Release 测试签名 APK SHA256 `413df273cbe17f464f27a7d25643704442c2a4d68a12ff5c37140e927f49a6c2`，92 个原生库的 ELF LOAD 16KB、APK ZIP 16KB 对齐和签名检查通过。普通启动参数为空。实际 Android 14 模拟器切换浅色、冷启动保持及恢复初始设置通过，证据保留于 `build-devtools/android-ui/workbench-native-20261004/`。其中 `initial-main.png` 仍显示恢复提示、`light-main.png` 仍显示设置弹窗，均不记作关闭后的主界面。
- 审阅补上隐藏 PV 的启动恢复与设置变化信号到实际 `MobilePreview` 的连接；此前只恢复工作台模型值。最终宿主 Release 构建通过，三种比例的独立 seed/restart 再次通过，并核对实际播放器 `hidePv` 与工作台偏好值一致。记录 `build-devtools/host/workbench-pv-build-20261004.log` 和 `workbench-pv-ui-20261004/run-49tfin06/verification.json`。此项属性恢复检查不等同于视频解码验证。
- 最终普通 ARM64 Release 测试签名 APK SHA256 `e891b61386a7e83f13782699f786ffcc2e130fc4e1b7e14ba1f9149a6d09721b`，大小 99,718,504 字节；92 个原生库的 ELF LOAD 16KB、APK ZIP 16KB 对齐与签名验证通过，普通启动参数为空。该包在同一实际模拟器重新验证浅色切换及冷启动，PID `21880` → `22137`，仅 `ui.theme` 改变，其余工作台字段保持一致。恢复初始设置及原工程后最终 PID `22293`，会话、原有 QSettings、谱面和音频逐字节保持一致，新工作台配置也已还原。记录 `build-devtools/android-ui/workbench-pv-native-20261004/verification.json`；`light-settings.png`、`restart-light-main.png` 和 `final-main.png` 已查看，最终节点确认恢复提示消失并显示 `Master 13`。最终 PID 日志未匹配记录所列 QML/崩溃模式，仅覆盖该进程与所列模式，不代替稳定性、真实设备或完整 PV 验收。

本项补齐设置模型到消费者和私有文件的链路。完整六页 `PreferencesDialog`、语言/布局/帧率/解码控制及快捷键/更新入口仍须迁移；安卓字号控件与软键盘/IME 实际操作未完成验证，v2 默认阻止输入法的选择也须在安卓设置中提供明确可操作的入口。完整功能、封面后台批量、物理设备与至少 90% 成对截图验收等既有 P5 待办继续保留。`P5Accepted=false`、`pairedV2FidelityVerified=false`。未提交或推送，v2 最新更新继续遵循本文顶部的验收、提交推送、再同步的顺序。

## 应用背景、难度标签与实际文件选择（2026-10-04）

Android 难度导航及编辑标签补齐既有 v2 的等级投影，当前《白金ディスコ》显示 `Master 13`；居中标题按活动编辑器显示作品和当前难度。应用背景接入实际 `AppBackgroundModel` 与 `AppBackgroundSettings`，通过现有 `UiRequestService` / `AndroidFileRequests` 选择图片并取得私有工作文件，配置保存到应用 `mobile/uiPreferences`。保存时合并已有 JSON；实际安卓检查确认其它普通设置保持不变。

背景页从 Mobile 副本内原有 v2 `PreferencesDialog` 提取为共享 `AppBackgroundPage`，保留启用、选择/清除、图片透明度、面板遮罩、模糊、缩放和位置的原有控件及回写语义；原偏好设置页也引用该组件。Android 画面使用与 v2 `Main.qml` 同样的 Image/MultiEffect 参数，按壁纸、工作区、全屏预览的顺序绘制。面板和壁纸共用 `Theme.backgroundActive`，侧栏使用既有 `Theme.surfaceColor`；浮层从完整场景采样，继续位于独立 Overlay。完整偏好设置的界面、编辑器、性能、快捷键和更新页仍需继续迁移，当前“设置与素材”承载背景页，不计作整个 v2 偏好设置迁移完成。

- 宿主 Release 完整构建通过：`build-devtools/host/background-reviewed-build-20261004.log`。十一项 CTest 通过：`background-ctest-20261004.log`。音频/预览设置 UI、编辑同步、三进程设置恢复及实时音效输出回归通过：`background-regressions-20261004.log`、`background-sfx-playback-20261004.log`。
- `--background-ui-smoke` 运行实际 QML 页面和生产模型，派发真实滑块/下拉控件回写，检查实际 Image Ready、填充和透明度、难度标签和标题、冷进程恢复、关闭/清除及无效路径保留原选择。首轮 seed 通过但 restart 失败，原因是诊断使用 Windows 反斜杠路径与模型规范路径直接比较；已用 `QDir::cleanPath` 修正比较，持久化生产代码没有为该失败改变。首次失败保留在 `build-devtools/host/background-20261004/storage-qzlaab2l-verification.json`，之后三组通过记录及截图各留在独立 storage。G 盘原谱面 SHA256 仍为 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。
- 新增可重复执行的 `scripts/build/validate-mobile-background.py`，用工程文本副本及每次独立目录，按 1280×720、960×720（4:3）、1120×480（21:9）逻辑尺寸启动 seed/restart 两个实际进程，记录截图的实际像素尺寸。脚本的实际三组检查通过，每组四张 UI 截图，记录在 `build-devtools/host/background-script-20261004/run-_9omw84z/verification.json`。它是宿主 UI 与持久化验证，不替代 Android SAF、物理设备或 v2 成对界面验收。
- 普通 APK 在 Android 14 模拟器实际打开背景页，经系统 DocumentsUI 搜索并选择 `miacode-background-20261004.png`。选择结果位于应用私有 `ui-files/imports`，与所选测试 PNG 逐字节相同，SHA256 为 `00f85a46ab117a5420112870387e2c55721515e7734339587ad6496f4dcdc0ed`。实际触摸图片及遮罩滑块，节点从 20%/78% 变为 45%/50%；实际重启、恢复工程后保持这些值及背景。关闭背景后截图恢复原面板填充。证据在 `build-devtools/android-ui/background-native-20261004/`：`picker-nodes.txt`、`search-result-nodes.txt`、`touched-nodes.txt`、`restart-settings-nodes.txt`、`import-verification.json`、`native-background-on.png`、`native-restart-on.png`、`native-background-off.png`，所列截图已实际查看。
- 测试结束恢复普通设置，并再次启动/恢复原工程。`restored.json` 及 `observation-final.json` 核对会话、普通设置、原工作谱面及音乐逐字节保持；`final-main.png` 已查看，标题及侧栏/编辑标签显示 `Master 13`，波形及 394/34/82/0/36/546 统计可见。最终 `verification.json` 汇集 APK 审计、实际导入、触摸及重启节点、文件哈希和实际 ARM64 PID 20507；模拟器仍为原始 1080×2220、440dpi。最终进程日志未匹配 QML ReferenceError/TypeError、绑定循环、属性赋值失败或原生 fatal 模式，这个有限模式检查不充当完整性能或稳定性验收。
- 新普通测试签名 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，SHA256 `92b5456cd2e922bbdb970039ff8321a774a3197c2a527ddcf8a97ea27768cfc4`。原生 Release 编译、APK 打包通过；92 个 ARM64 库 ELF LOAD 16KB、APK ZIP 16KB 对齐和签名分别通过，CMake/清单均无固定诊断参数。证据：`background-build-20261004.log`、上述安卓证据目录的 `apk-audit.json` / `elf-audit.json`。此前音效原生诊断针对此前构建，本次不据此声称新包已重新完成同一原生音效诊断。

这些检查证明所列背景配置、文件选择、恢复及标签投影路径。配对仍需统一 v2 与 Android 内容尺寸、字体、应用背景、暂停判定线和时间轴状态；完整偏好设置、其余功能及物理设备等既有 P5 待办继续保留。`P5Accepted=false`、`pairedV2FidelityVerified=false`，没有提交或推送；v2 最新更新继续按顶部顺序留待用户 P5 验收以及安卓成果提交、推送之后。

## 实时谱面音效输出与实际桌面参考（2026-10-04）

补齐原先只有 BGM 的实时预览音频链路。`MobileSfxMixer` 直接采用既有 v2 的 SFX 时间线、同刻聚合、同类 latest-wins 和 Touch Hold 所有权分段；`MobileSfxOutput` 在后台加载十三种既有 WAV 素材，通过实际 `QAudioSink` 回调输出 PCM。十路音量和尾判音开关使用既有 `PreviewAudioSettings` 路由，播放、暂停、恢复、拖动、难度替换、变速和选区终点进入同一个音效 owner。选区终点在回调内限制输出，不等待下一次 UI 定时器；设备更换暂停预览，素材或输出失败进入实际通知服务。

- 纯混音检查覆盖 PCM16/float32、异常与截断输入、全部十三种实际素材、同刻事件聚合、同类打断、异类叠加、倍速下原采样速度、实时静音、seek 清除旧短音效、嵌套 Touch Hold 恢复、尾判音开关及选区终点。最终宿主 Release 完整构建和十一项 CTest 通过：`build-devtools/host/sfx-loader-reviewed-build-20261004.log`、`sfx-loader-reviewed-ctest-20261004.log`。
- 新增 `--sfx-playback-smoke`，检查实际 v2 预览 pane 与运输控制器绑定，且只在独立存储副本中修改测试谱面。宿主设备验证实际回调产生非零 PCM、暂停后帧数停止增长、恢复保留输出、拖入 Touch Hold 能恢复持续音、替换难度后旧声音消失、2x 输出继续运行。G 盘原谱面和音乐 SHA256 未改变。最终记录：`build-devtools/host/sfx-playback-20261004/verification.json`；设置 UI、编辑同步及三进程设置持久化回归全部通过：`build-devtools/host/sfx-regressions-20261004/verification.json`。
- 首次实际设备检查暴露 Qt 6.11.1 回调路径的 `processedUSecs()` 始终为零，旧同步逻辑因而反复重启并丢失音效。改用设备回调请求的 PCM 帧数记录提交时钟。Windows 实现的回调与环形缓冲处理路径不同，参考 [Qt v6.11.1 实现](https://raw.githubusercontent.com/qt/qtmultimedia/v6.11.1/src/multimedia/windows/qwindowsaudiosink.cpp)；安卓本次实际观察同样保留零处理时长和非零回调帧数。该计数说明设备请求了 PCM，尚不表示测得了扬声器实际输出延迟。早期失败 JSON 留在各独立宿主存储目录中，未计为通过。
- Android NDK 27.2 的标准库未提供此次使用的 `std::jthread`，首次原生构建失败；已改为可停止并 join 的 `std::thread`，保留构建失败日志。音效类型通过独立 owner 隔离，`MobilePreview` 前向声明该类型，避免音效私有实现头文件变化传播到全部 QML 编译单元。
- 同一八阶段检查在 Android 14 模拟器 `emulator-5554` 的实际 ARM64 进程中通过，使用产品默认渲染循环。应用内检查通过后，首轮收集器过早停止，未等到 `main()` 的退出消息；继续观察同一 PID 18730，实际 `main()` 返回 0，保留 `initial-collection-verification.json`、`completion-logcat.txt` 并修正收集器的等待条件，没有为得到通过结果重启测试。最终记录位于 `build-devtools/android-ui/sfx-native-20261004/verification.json`，原会话、普通设置、原谱面和音乐在诊断前后逐字节一致。测试中短谱面替换伴随 `QTextCursor` 越界警告，后续需在实际编辑路径隔离复核，不能只凭本次音效检查排除编辑问题。
- 已移除固定诊断启动参数，重新构建并安装普通测试签名 APK。其 92 个 ARM64 原生库与通过检查的诊断 APK 逐字节一致；ELF LOAD 16KB、APK ZIP 16KB 对齐和签名各自检查通过。普通 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，SHA256 `66608428f1dd173bab141f2d97c634396d629d1de28b5e69f5ce645e4dce6b19`。证据：`ordinary-restored.json`、`ordinary-elf-audit.json`、`final-audit.json`。实际点击恢复原《白金ディスコ》工程后，`ordinary-main-settled.png` 已查看，编辑器、波形、预览和 394/34/82/0/36/546 统计可见。紧随点击的首张截图仍为恢复过渡状态，未作为完成证据。
- 实际 v2 桌面参考已可操作，记录了主编辑、音频设置、预览画面/游戏/皮肤、单曲及批量视频导出、实际封面画板。来源仍为 `c190bb2c138cf61032dd7ac97ec41027da4bb40d`。文件位于 `build-devtools/ui-parity-20261004/actual-v2/`，具体有效参考、配置差异和配对条件见 [界面配对验收记录](ANDROID_UI_PAIRING_ZH.md)。截图包含桌面应用背景及不同时间轴/暂停显示配置；尚未对齐状态和逻辑内容尺寸，没有据此给出 90% 通过分数。

本项验证实时谱面 PCM 输出及所列控制路径。BGM 和 SFX 仍使用两个输出管线；单一主时钟、物理输出延迟、延迟校准、倍速与重叠 Hold 的完整行为、回调竞争和长期稳定性仍需完善与测量。片头/倒计时音频尚未加入该实时程序。封面后台批量、Android 文件目录操作、物理手机/平板、完整功能及主要页面至少 90% 成对视觉验收等 P5 待办继续保留。`P5Accepted`、`pairedV2FidelityVerified` 和物理设备验证均为 `false`。原 v2 工作区仍干净；没有提交或推送，最新更新按顶部顺序留待 P5 验收及安卓成果提交、推送之后。

## 设置持久化与工程音量隔离（2026-10-04）

按既有 v2 语义补齐设置范围：画面、游戏参数及皮肤选择使用应用配置；十路音量、静音前恢复值使用工程目录的 `.miacode/preferences.json` 中 `preview_audio`；Break 星星尾判音静音属于应用配置，加载工程或恢复默认时继续使用应用开关。“设为本地默认”明确保存一套默认音量，切换到没有工程音量的谱面才采用它；已有工程配置保持独立。恢复会通知实际 `AudioSettingsModel`，同时更新预览及后续导出任务。

- `MobileExportComposition` 合并保存自身配置，保留未知应用与工程键；恢复时核对皮肤及自定义判定线是否存在，限制枚举和数值范围。亮度以整数百分比生效，流速按 v2 的 0.25 步长取整。导出页回写共享渲染参数时保存应用配置；临时判定线操作的 `persist=false` 不直接写入配置。
- 移动工程所复用的 `ProjectPreferences` 保存改为 `QSaveFile` 原子替换并禁用直接写入回退；写入失败向实际通知服务报告，当前音量继续在会话内生效。此修改位于 Mobile 副本，原 v2 源码没有改动。
- 宿主 Release 完整构建、十项 CTest 及实际设置 UI 回归通过。回归仍操作原有 v2 对话框、真实滑块、试听和静音/默认按钮；现在额外恢复检查前的整个测试设置组及工程 sidecar。日志：`build-devtools/host/preferences-build-reviewed-20261004.log`、`preferences-ctest-20261004.log`、`settings-persistence-regression-20261004.log`。早期诊断头文件的编译错误及修复日志保留。
- 新增 `scripts/build/validate-mobile-preferences.py`，每次创建独立存储目录，用 seed、restart、invalid 三个实际进程检查共 39 项条件：十路工程音量、静音前恢复值、本地默认更新与显式恢复、应用尾判音覆盖、画面/游戏/皮肤冷启动恢复、预览与导出任务参数一致、未知键保留、损坏工程配置回退、无效保存值和缺失素材归一化，以及旧配置存在时真正失败的原子替换。宿主失败替换使用测试文件的独占句柄。最终通过记录：`build-devtools/host/preferences-reviewed-20261004/verification.json`。宿主取 G 盘谱面文本的副本，原文件 SHA256 保持 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。
- 同一组所有者与模型检查在 Android 14 模拟器 `emulator-5554` 的三个独立原生进程中通过，三个实际 `main()` 退出码均为 0。Android 失败替换通过移除测试工程配置目录写权限来触发；旧文件逐字节保持完整。记录：`build-devtools/android-ui/preferences-native-20261004/verification.json`、`preferences-{seed,restart,invalid}.json` 及对应日志。这组检查通过真实 workspace 切换独立工程，使用私有工程的文本副本，不带音视频素材；它验证保存范围和恢复链路，不覆盖 SAF 选目录、PV 解码切换或界面触摸全过程。
- 清除固定诊断启动参数后重新构建并安装普通 APK，其 92 个 ARM64 原生库与通过检查的诊断 APK 逐字节相同。所有库 ELF LOAD 16KB、APK ZIP 16KB 对齐和签名分别验证通过。最新普通测试签名 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，SHA256 `af27bfa100d2a67397abe24d965cba05043dd0cb98b96a47aa744ed0e858e90a`。证据：`ordinary-restored.json`、`ordinary-elf-audit.json`、`final-audit.json`。
- 普通启动后通过实际恢复按钮恢复 `白金ディスコ`，`final-main.png` 已查看：编辑器、预览、波形与 394/34/82/0/36/546 统计可见。原工程会话和普通应用设置在诊断及恢复安装前后逐字节一致；模拟器保持原始 1080×2220、440dpi。原 v2 工作区仍干净，没有提交和推送。

本项完成所列预览设置、皮肤选择和工程音量的保存范围与恢复检查。音频时钟/延迟校准、实时谱面音效、目录操作的 Android 文件适配、封面后台批量、物理手机/平板、完整功能与桌面 v2 成对界面验收等 P5 待办继续保留。`P5Accepted`、`pairedV2FidelityVerified` 和物理设备验证均为 `false`；v2 最新更新继续按顶部顺序执行。

## v2 音频与预览设置对话框接入（2026-10-04）

工具栏和菜单的音频、预览按钮原先共同打开简易“设置与素材”弹窗，现分别连接实际 `AudioSettingsDialog` / `PreviewSettingsDialog`。侧栏视图设置也连接预览设置。音频页沿用十路通道、静音恢复、主静音联动、本地默认及尾判音开关；预览页沿用画面、游戏、皮肤三页与已有 `PreviewSettingsModel`。简易弹窗中的开发探针按钮已删除。

- 复用实际 `AudioSettingsModel` 的通道和 220ms 试听延后逻辑，为移动端注入 `MobileAudioAudition`。适配器通过本机 WAV 缓存和 `QSoundEffect` 播放设置试听，复用 v2 音效文件解析及音量计算；试听载入完成前更换请求，只播放当前请求。关闭对话框、恢复默认或开始预览会释放试听，音量变动同时更新已载入的声音。这个适配器只处理设置试听，实时谱面音效的调度仍需完成。[Qt QSoundEffect 文档](https://doc.qt.io/qt-6/qsoundeffect.html)说明其 WAV 播放和异步载入状态接口。
- 宿主 Release 完整构建、十项 CTest、实际设置 UI 检查通过。UI 检查经工具栏信号打开两个对话框，操作真实滑块、静音及默认按钮，验证拖动保持期间不试听、释放后的音效类别及增益、默认恢复与关闭释放；亮度、时间戳和流速进入实际预览或导出任务。日志：`build-devtools/host/settings-ui-smoke-visual-20261004.log`、`settings-ctest-20261004.log`。最初测试没有找到 Repeater 动态控件，日志保留；测试改为查找实际视觉树，没有改变产品滑块逻辑来制造通过。
- 同一 UI 检查在 Android 14 模拟器 `emulator-5554` 通过，使用默认渲染循环和默认解码路径。诊断使用单独工程副本；`verification.json` 及 `logcat.txt` 位于 `build-devtools/android-ui/settings-native-20261004/`。首次副本的文件所有者造成恢复写入提示，已修正诊断副本归属并保留首次证据。第二次采集器先于应用完成结束；随后读取同一进程的最终通过日志和退出码 0，重新采集四张最终截图，没有重启这一轮测试来替换结果。Qt 在退出时仍有字体销毁阶段的警告，记录保留。
- 普通 APK 已清除固定诊断参数并恢复安装；其 92 个原生库与通过检查的诊断包逐字节相同。普通启动实际触摸 Tap 滑块，辅助节点及截图记录 30% → 50%；点击“恢复本地默认”后回到 30%。沿标签区域向上滑动，尾判音开关可见高度从部分显示增至 49px，底部操作按钮保持可用。证据：`ordinary-audio-{before-touch,after-touch,restored}-nodes.txt`、`ordinary-audio-bottom.png` 和 `final-audit.json`。
- 普通 APK 在模拟 4:3 平板与 21:9 手机尺寸下，实际打开音频页和预览三页，逐页点击、关闭并截图；八张截图已查看，弹窗及页脚均留在窗口内，音频长内容继续通过原有滚动视口访问。记录：`ordinary-layout-matrix.json` 及 `tablet-4-3-*.png` / `phone-21-9-*.png`。测试后恢复原始 1080×2220、440dpi，无尺寸或密度覆盖。这是模拟器布局检查，不是物理手机、平板或桌面 v2 成对相似度证明。
- 最新普通测试签名 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，SHA256 `851d2b8543393e80be1464b19ac5b62d031ab7fb98f1edb1e8a74c0b9f19991e`。92 个 ARM64 库 ELF LOAD 16KB、APK ZIP 16KB 对齐与签名分别验证通过。主会话快照逐字节保持一致；G 盘原始谱面哈希保持一致；原 v2 工作区保持干净。没有进行提交和推送。

本项补齐两个设置入口与设置试听的实际链路。实时谱面音效、音频时钟/延迟校准、全部设置的重启恢复及皮肤目录操作的 Android 文件适配仍需完善；封面后台批量、真实设备、完整功能和桌面 v2 成对界面验收等既有 P5 待办继续保留。`P5Accepted` 和 `pairedV2FidelityVerified` 均为 `false`，v2 最新更新按本文顶部顺序留到 P5 验收与安卓成果提交推送后执行。

## 编辑器与时间轴同步连接修复（2026-10-04）

实际 QML 属性检查发现 `EditorPane` 的 `editorSync: editorSync` 绑定读取了组件自身的同名属性，未连接到 C++ 共享 `EditorSyncController`。原有输入、难度隔离与撤销测试没有覆盖这一连接。修复前增加的实际 UI 检查以退出码 27 失败，日志：`build-devtools/host/editor-sync-before-20261004.log`。移动端上下文名称改为 `mobileEditorSync`，同时更新编辑器、书签与分析行导航的入口；沿用已有 v2 同步服务。

- 增加 `--editor-sync-ui-smoke`，使用实际 `SourceEditor`、同步控制器与 `MobileTimeline`，逐项检查定位及回执、时间轴定位至编辑器和预览、编辑光标回传时间轴、编辑器位置跳转预览、过期 revision 拒绝、播放跟随范围与编辑光标保持。等待实际信号并设置 15 秒总超时，不直接调用同步服务的回执或就绪状态制造通过。
- 宿主 Release 构建、上述实际 UI 检查、原有输入/难度隔离/撤销检查及 8 项 CTest 通过。宿主同步日志：`build-devtools/host/editor-sync-delivery-20261004.log`；基础回归：`build-devtools/host/editor-sync-final-ctest-20261004.log`。
- 安卓模拟器 `emulator-5554` 的同一实际 UI 检查通过，使用默认渲染循环和默认视频解码路径。证明：`build-devtools/android-ui/editor-sync-native-20261004/verification.json` 和 `logcat.txt`。诊断任务使用独立存储目录；主会话快照前后逐字节一致。
- 首次安卓跟随检查失败，保留 `first-failed-logcat.txt` / `first-failed-verification.json`。检查原先拿尚未投递的最新播放采样与界面比较，已改为与实际 `followChanged` 投递的范围比较；编辑光标保持断言仍然保留。产品同步逻辑没有为此修改。
- 诊断入口已从 CMake 缓存及普通 APK 清单移除，并重新安装普通 APK；92 个原生库与通过检查的诊断 APK 逐字节相同。记录：`build-devtools/android-ui/editor-sync-native-20261004/ordinary-restored.json`。
- 最新普通测试签名 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，SHA256 `c993c303d814317b5b3ff379f664244661fd8dcb3a040c1d21dd3ca2c9998086`。92 个 ARM64 库的 ELF LOAD 16KB 检查、APK 16KB ZIP 对齐与签名验证通过；报告：`build-devtools/android-arm64-6.11.1/editor-sync-apk-audit-20261004.json`。
- 普通启动后已通过实际恢复对话框恢复原来的 `白金ディスコ` 私有工程，截图 `build-devtools/android-ui/editor-sync-ordinary-restored-20261004.png` 已检查，编辑器、波形、预览及 394/34/82/0/36/546 统计正常显示。此过程中模拟器系统界面曾弹出无响应提示，选择“等待”后恢复；未将这一环境状态当作性能验收通过。

本次验证覆盖上述同步路径；真实设备触摸/IME、Android 12、真实 16KB 设备、完整功能及成对界面验收仍需完成。未进行提交和推送；v2 最新更新继续遵循本文顶部的 P5 后同步顺序。

## 实际 ZIP 入口与离线打包（2026-10-04）

将导出侧栏的“打包 ZIP”从提示入口接入 `MobileZipExport`。沿用 v2 的 `ChartZipPackager` 与 `ChartAssetPaths`：完整文档快照写为 `maidata.txt`，附带解析到的音轨、静态背景和同目录 PV，保留原有备份排除及外部视频路径规则。打包前提交编辑字段，在系统文件选择期间捕获的快照保持独立，之后的编辑不会被导出覆盖。

压缩使用独立线程，沿用 v2 文件请求、进度、取消及结果对话框。先写唯一暂存包，再用 `QSaveFile` 原子提交；随后通过既有 Android SAF 发布器复制并回读校验，发布失败时保留完整私有文件。后台允许设置接入既有前台服务；不允许后台时应用状态变化会取消任务。此设置的完整 ZIP 后台/锁屏矩阵尚未测试，不能由视频或封面测试替代。

独立工程补齐 `third_party/miniz` 的源码、头文件、说明和许可证，并把许可证嵌入 APK 资源。四份文件逐项与基线 `c190bb2c138cf61032dd7ac97ec41027da4bb40d` 的 Git 内容核对，仅忽略行尾差异；来源记录见 `build-devtools/android-arm64-6.11.1/zip-export-final-audit-20261004.json`。这不是 P5 后的最新版本同步，v2 工作区保持干净。

- 宿主 Release 全量构建及 10 项 CTest 通过。新增 `MobileZipExportSpec` 核查输入字段提交、选择文件期间的快照隔离、重复选择请求、扩展名、取消保留已有输出、写入失败及重试；同时接入已有 `ChartZipPackagerSpec` 检查真实压缩内容和素材排除规则。日志 `build-devtools/host/zip-export-final-build-20261004.log`、`zip-export-ctest-20261004.log`。
- 编辑器与时间轴的实际 UI 回归仍通过，日志 `build-devtools/host/zip-sync-regression-20261004.log`。
- 普通 Release APK 在 Android 14 模拟器完成实际操作：导出侧栏 → 打包 ZIP → 系统创建文件选择器 → Download 目录 → 保存 → v2 结果对话框。此次未使用固定验收参数或诊断 APK。活动栏图标补充了基于现有提示文字的无障碍名称，外观保持原组件；实际安卓无障碍树可以识别“谱面”“导出”等入口。
- 执行导出时飞行模式开启、Wi-Fi 与移动数据关闭；测试后恢复原始设置。状态记录为 `build-devtools/android-ui/zip-export-native-20261004/network-{before,offline,restored}.json`。
- 最终公共文件 `Download/白金ディスコ.zip` 已取回，ZIP CRC 校验通过，`maidata.txt` 的 2275 字节与当前会话快照完全相同；`track.mp3`、`bg.jpg`、`pv.mp4` 均按未压缩方式存储，逐项 SHA256 与私有工程媒体一致。整个 ZIP SHA256 为 `cf652b50215b6e0647c48186572201e1441be5b9d116ebe1bc3e33bffaac4603`。证据 `build-devtools/android-ui/zip-export-native-20261004/verification.json` 与 `actual-ui-export.zip`，实际结果截图 `actual-ui-success.png` 已检查。
- 主会话八项字段保持一致，G 盘原始谱面哈希仍为 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。测试后返回谱面页，`final-main.png` 已检查；`final-services.txt` 确认没有导出服务运行。
- 当前普通测试签名 APK SHA256 为 `a2ff0cc29a6b43aaec7ec5f929877c815b691ed8a0313c158068d2b699bbdb45`。92 个 ARM64 ELF LOAD 16KB、APK ZIP 对齐及测试签名验证通过，普通清单和 CMake 缓存无诊断启动参数；审核为 `zip-export-final-audit-20261004.json`。

仍需完成全部功能、真实设备、性能、正式签名及 90% 成对界面验收。界面核查还确认，当前工具栏音频/预览设置仍打开通用“设置与素材”弹窗，需接入实际 v2 `AudioSettingsDialog`、`PreviewSettingsDialog` 及对应模型；这一差异继续留在待制作项中。本轮未提交或推送，后续 v2 更新顺序保持不变。

## 阶段状态（2026-10-03）

| 阶段 | 当前证据 | 仍需完成 |
|---|---|---|
| P0：平台与方案验证 | Qt 6.11.1 ARM64 APK、SAF、真实谱面 QSG → MediaCodec / AAC → MP4，16KB ELF 检查 | 片头、批量、真实设备的编码能力与长任务验证 |
| P1：工程与文件会话 | 独立源码工程、横屏清单、私有工程副本、原子恢复、异步保存快照、素材/字体导入 | 真机授权与生命周期、Android 12 / 16KB 设备兼容性验证 |
| P2：编辑与分析 | 原生 EditorPane / SourceEditor、完整谱面信息表单、名义管理、多难度、v2 高亮与输入桥、分难度撤销、整理核心、真实素材加载 | 操作菜单完整接入、异步检测和时间轴联动验收、触摸/IME 行为、保存与元数据脏状态一致性 |
| P3：时间轴与预览 | v2 PreviewPane、原生 QSG 场景、背景、六类音符统计、真实波形与 PV；已接入 v2 BottomPanel / TimelineQuickItem / 异步 AnalysisService | 完整音效、速率/跳转/音频时钟、预览参数、多比例布局及真机性能 |
| P4：作品导出 | 实际 v2 导出页已接入；真实谱面选区 MP4、WAV 及 SAF 发布在 Android 模拟器完成；视频单曲及批量后台允许/禁止切至 Home 的分支已验证；实际批量 UI 的 WAV/MP4、同名保留、取消和重试在模拟器通过；通知栏取消通过；封面 JPG/透明 PNG、谱面帧、素材及布局文件往返通过；批量封面实际入口、四种内置预设及用户预设前台导出通过 | 片头安卓曲绘修正后的重测、封面检查器完整矩阵、批量封面后台停顿、锁屏和系统中止验证 |
| P5：交付验收 | 验收要求已明确 | 完整功能回归、90% 界面对照、性能与设备矩阵、正式发布签名、最终 APK |

## 已有验证

- 宿主基础测试：文件会话、快照保存、权限失败、恢复、UTF-8、未知字段、多难度和未完成语法。
- 编辑器实际 UI 测试：输入写回、难度隔离、切换后保留撤销历史。
- `白金ディスコ` 实际工程：正确加载 Master、等级 13、谱面文本和高亮；预览统计为 Tap 394、Hold 34、Slide 82、Touch 0、Break 36、Total 546。
- 原始 `maidata.txt`：2348 字节，SHA256 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。测试入口复制工程后才允许编辑和恢复写入。
- 最近验证截图：`build-devtools/host/real-chart-preview.png`。该截图仅证明已接入功能，尚未证明 90% 界面相似度。
- 更新截图：`build-devtools/host/v2-chrome-timeline-ten.png`，已显示 v2 标题栏、主菜单、工具栏、Maple Mono 编辑字体、真实波形和 10 秒处音符/轨道。基础测试与编辑器 UI 测试再次通过。
- 宿主异常：MSVC 19.36 / Qt 6.11.1 的优化版 slide 轨道遍历在该工程 10 秒处崩溃；宿主验证仅对 `PreviewTrackLayerState.cpp` 使用 `/Od` 后，标准线程渲染循环截图通过。根因尚待进一步确认。Android Clang Release 保留优化并需单独验证；此处理不构成 Android 兼容性结论。
- Android 场景测试 APK：`build-devtools/android-arm64-6.11.1/MiaCodeMobile-arm64-test.apk`，已安装模拟器。92 个原生库均为 ARM64，所有 ELF LOAD 对齐通过 16KB 检查，`zipalign -c -P 16 4` 通过。仍需真实 16KB 设备验证。
- 打包问题已定位：Qt 提供的 `libavformat.so` 原始 ELF 对齐正确，Gradle 的 NDK r27 strip 输出破坏一个 LOAD 段的文件偏移。独立 `packaging/android/build.gradle` 保留该预编译库后，92 个库的审核全部通过。
- 手机高度适配：按真实标题栏、工具栏、预览与时间轴的最小几何计算工作区缩放；保留完整 v2 面板，并让触摸命中与绘制共用同一变换。模拟器中原先因高度不足而关闭的预览场景现已显示。

## 预览区拖拽卡住修复（2026-10-01）

模拟器在拖拽预览与编辑器之间的分隔条后，应用 Qt 主线程持续占用一个核心，日志反复报告六个统计单元无法放入 3×1 Grid；Android 系统服务仍能响应。根因是整体缩放比例读取预览区的动态最小高度：统计区宽度跨过 528 时，一行/两行切换改变高度，高度改变缩放，缩放又改变宽度，造成循环。

修复让整体缩放使用与宽度无关的 `PreviewPane.stableMinimumHeight`，统计区仍按实际宽度切换一行和两行；Grid 只指定列数，由 Qt 按单元数量推导行数，避免两项绑定分别更新时出现暂时的 3×1 约束。

- Android Clang Release APK 已重新构建并安装到 `emulator-5554`。
- 使用 `白金ディスコ` 的私有副本，在实际播放时拖拽分隔条，完成两行→一行→两行切换；PV、音符和时间轴在 33 秒及 65 秒处正常推进。
- 截图：`build-devtools/android-ui/drag-fix-widest.png`、`drag-fix-narrow-again.png`；修复后的对应日志没有 Grid 容量、绑定循环和着色器编译错误。
- 宿主 Release 自动回归 `--layout-smoke` 通过：连续 18 次调整预览宽度，确实覆盖一行和两行统计布局，并检查全局缩放稳定及 Grid 容量；日志 `build-devtools/host/drag-layout-smoke.log`。实际 EditorPane 的 `--ui-smoke` 输入、难度隔离与保留撤销测试通过，日志 `drag-ui-smoke.log`。
- 实际 v2 谱面信息表单：`build-devtools/android-ui/v2-metadata-android-late.png`；全屏布局：`v2-fullscreen-android-late.png`。这些证据不等同于 90% 相似度验收。
- 最新 APK 的 92 个 ARM64 原生库通过 16KB ELF LOAD 检查，`zipalign -c -P 16 4` 返回 0。报告：`build-devtools/android-arm64-6.11.1/drag-fix-apk-audit.json`；SHA256：`39c3ecfb968b1c4493767d993b89c662ff895c197214962a94da1e5928972c8a`。真实 16KB 设备验证仍待完成。
- 基础测试新增媒体覆盖与恢复、偏移/额外字段、七个谱师名义及统一名义、`pv.mp4` 自动识别和移除后恢复等用例，已通过。读取标题/曲师暂使用当前导入音频，独立音频选择器入口仍需补齐。

## 真实谱面导出核心（2026-10-01）

已新增 Android 导出任务：快照捕获谱面、素材、渲染参数及 v2 音效时间计划；使用实际 `PreviewQuickExportSession` 渲染，再由 MediaCodec / AAC / MediaMuxer 写入 MP4。原生编码在专用线程执行，帧队列容量为 2，音频先按块解码到私有暂存文件，再按块混音为 WAV。该任务已通过命令行验证入口运行，尚未接入实际 `ExportVideoPage`，因此 P4 仍在制作中。

- 实际工程 `白金ディスコ`，选区 10–15 秒，静止前导 1.5 秒，720×720 / 30fps；最终文件时长 6.5 秒，195 帧。MediaExtractor 检查帧数、尺寸、视频时间戳和 AAC 音轨，通过后才提交最终文件。
- 初次输出存在背景取样错误。`QSGSimpleTextureNode.setSourceRect` 接收相对纹理尺寸的像素矩形，既有层把矩形提前转换为归一化坐标，导致再次归一化后只取左上角极小区域。已修正独立移动工程的外层及内圈背景取样。接口依据：[Qt 文档](https://doc.qt.io/qt-6/qsgsimpletexturenode.html#setSourceRect)。
- 修正后的真实 Android 文件：`build-devtools/android-ui/chart-export/qa-chart-fixed.mp4`。再用宿主 Qt Multimedia 从最终 MP4 解码 0、3.2、6.4 秒取帧，画面包含实际 PV、音符和轨道：`decoded-fixed/decoded-1.png`。容器检查：`fixed-container.json`，任务检查：`fixed-report.json`。宿主静态背景的实际渲染样本：`build-devtools/host/export-qa-850aa1d1454441f58a6fb4cdad9505ab/chart-clip-000097.png`。
- 真实混音 WAV：48kHz、双声道、16bit、312000 音频帧；静止前导保持静音，正文包含 BGM 和 42 次谱面音效。宿主片头任务按 v2 时间规则得到 12.833333 秒 WAV，片头音频有实际信号；这不替代 Android 片头画面验证。音频检查：`audio-verification.json`。
- 继续优化：静止前导及负时间片头复用同一 PV 帧，动态 PV 使用单个保留纹理，避免逐帧进入静态图片缓存。逐帧 PV 读取目前仍使用 MediaMetadataRetriever，模拟器导出明显慢于实时，连续解码优化及真机性能验证待完成。
- 首个导出 APK（缓存优化之前）：92 个 ARM64 库均通过 ELF LOAD 16KB 检查，`zipalign -c -P 16 4` 返回 0；SHA256 `4ffee03c8351e65ec6757027b47c6b74bbb9456c575b9ff85b6e455024af37a0`。记录仅对应该构建，后续更新 APK 需重新审核。
- 当前回归：宿主实际布局测试覆盖 18 次统计区宽度变化，通过；基础 CTest 通过；无媒体的软件渲染 UI 测试通过输入、难度隔离和撤销。一次带 PV 的 UI 测试超时，须继续检查宿主媒体/渲染生命周期，不能把超时记录算作通过。
- 原始 G 盘 `maidata.txt` 哈希再次检查保持 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。

后台执行使用用户设置控制的前台服务。服务包含进度和取消入口，Qt 的后台运行保持开启。模拟器验证：允许后台时，片头任务切至 Home 后仍完成 385 帧；不允许后台时，等待任务开始后切至 Home，任务报告 `Export cancelled`，最终 MP4 未提交（`background-denied-verification-2.json`）。首次测试在任务开始前过早切到 Home，未取得任务报告，属于无效测试；已保留记录，未算作通过。测试结束恢复原来的后台设置与文件所有权。通知权限、锁屏、系统中止、超时与导出途中取消仍需验证。

独立 WAV 输出已在模拟器完成，文件 `build-devtools/android-ui/chart-export/qa-chart.wav`，任务 `wav-report.json`；不要求经过视频渲染。音频混音新增 `MobileExportAudioSpec`，用独立双声道信号核对源偏移、增益、定时音效、持续音效截断、输出时长以及取消后无最终文件，已与基础 CTest 一起通过。

Android 片头的初次容器检查通过，但解码画面发现漏打包 `bg_texture.frag.qsb`；已补充 ShaderTools 构建资源，宿主 3.2 秒片头样本恢复模糊背景与曲绘（`build-devtools/host/intro-shader-qa-d7c2b740af3d4136aa8c0770ae17b542/chart-intro-000096.png`）。Android 重建后模糊背景和谱师名义恢复，但曲绘槽仍是黑色，部分区域有图像碎片。`decoded-intro-fixed/decoded-1.png` 记录这一未解决问题，不能作为片头画面通过证据。分别关闭 mipmap、关闭 jacketSlot 裁切、限制 jacket 图片 sourceSize 后，实际 MP4 解码结果仍异常；三项试验均已撤回，保留 v2 的原有图片行为。后续需诊断 Android GLES / Qt Quick 离屏图像渲染，不能把这些无效试验当作修复。

## v2 / Mobile 工作区分离（2026-10-01）

已将 v2 中早期移动原型、脚本及迁移文档移至本工程审阅。7 个跟踪文件的迁移改动保留 patch 后恢复到 v2 HEAD；v2 `git status --porcelain` 为空，`git diff --check` 通过。用户随后授权删除无用原型：23 个早期文件均有当前对应版本（11 个完全相同，12 个已被新实现/文档替代），已删除这些文件及 7 个冗余工作副本，仅保留 `docs/handoff/v2-migration-residue-20261001` 内的 patch、旧哈希清单与审阅报告。现有 Mobile 源码保持独立；Qt / SDK 仍复用 v2 的 Git 忽略目录作为本机工具链，构建脚本可接受其他安装位置。后续在 Mobile 工程内继续开发，v2 作为只读参考。

## 实际 v2 导出页与设置联动（2026-10-02）

Mobile 已装配实际 `ExportVideoPage`、`ExportSidebarPage`、`ExportSession` 和 `PreviewSettingsModel`，并保留原请求弹窗及任务进度层。四类判定效果、皮肤与效果风格、内建/自定义外框、背景缩放及暂停判定区已接入真实预览；导出任务在后台加载启动时指定的皮肤与外框，避免异步预览加载或暂停显示状态污染离屏渲染。最终私有文件复制也移到后台线程，并检查取消。

- 宿主 Release 的实际导出页 smoke 通过：设置修改进入运行时/任务快照；调用实际选区试听按钮后从区间外回到 10 秒，接近终点时播放并在 15 秒停止；由实际 `ExportSession.startExport()` 输出 48kHz、双声道、16bit、312000 帧的 WAV（5 秒正文加 1.5 秒冻结前导）。证据目录 `build-devtools/host/export-settings-qa-8d8f85b494724bc8a40134f3935c90a2` 包含日志、导出页截图及 `verification.json`。首次脚本选取 0.25 秒区间，被 v2 5 秒最短区间规则纠正，脚本已修正后重测通过；未放宽产品规则。
- `AndroidFoundationSpec` 与 `MobileExportAudioSpec` 通过。拖拽回归 `--layout-smoke` 再次完成 18 次宽度变化，日志 `build-devtools/host/layout-regression-8a10e66327cc43729bcad2723553cf7e/test.log`。
- 基于上述宿主构建的离屏静态背景样本再次输出成功：`build-devtools/host/export-snapshot-qa-4917120d8da54dc7928b680685c52eaa/samples-000097.png`。此验证在 SAF 新代码集成之前，不代替当前安卓版本的验证。

新接入的 Android 文件请求桥将 v2 的素材文件选择转换为 SAF 导入；输出文件/目录选择绑定私有路径与目标 URI。发布使用后台流式复制，并重新读取目标文件核对大小与 SHA256，校验结束前任务保持运行。失败时保留私有完整文件；文件提供方不保证事务替换，中断写入可能留下不完整的目标文件。目录输入/输出能力已接入文件请求，批量调度本身仍待实现。新增 `ExportDestinationSpec` 检查单文件与目录授权范围、路径规范化及优先级。

这一阶段的 SAF 桥、异步复制及新测试的后续验证见下一节。批量、封面、完整设置持久化/音效、片头曲绘、编码质量/文件体积选项的原生落实及比例/90% 截图验收仍未完成。

## 实际界面 SAF 导出与弹出层适配（2026-10-03）

实际 `ExportVideoPage` 的文件名、浏览按钮、选区输入及开始导出按钮已在模拟器操作验证。通过 Android 系统文件选择器选择 Downloads，保留 URI 授权并以后台流式复制发布最终文件，发布后重新读取提供方文件核对 SHA256。此验证使用完整谱面测试副本，原始 G 盘 maidata.txt 的哈希仍为 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。模拟器时钟仍显示 10 月 2 日，记录日期按工作区环境为 10 月 3 日。

- WAV：10–15 秒选区加 1.5 秒冻结前导，48kHz、双声道、16bit、312000 帧、6.5 秒、1248044 字节。Downloads 与应用内完整文件逐字节一致，SHA256 `1953717aaf16019bae38e5ebfdee883c4bb9e29cfb4a214246f0883c702a8e7b`；证据 `build-devtools/android-ui/saf-export-qa-20261002/verification.json` 与 `report.json`。测试 APK 为本节输入提示修正之前的构建。
- MP4：从实际界面设置 30fps 与 10–15 秒选区，最终 **1024×1024**、195 帧、6.5 秒、304 个 AAC 音频包，PV 已启用，最后视频 PTS 6466666 微秒。公有/私有文件相同，SHA256 `8b371194985521eefa233428fb8ec2e0be2da330e4a99cdf78a25e1d644b2fc0`。证据 `build-devtools/android-ui/saf-export-qa-20261003/`，实际解码 0/3.2/6.4 秒样本在 `decoded/`，已查看正文样本包含 PV、Tap/Hold 与轨道。该次 APK 哈希 `0201fcc13982bdca2e29d99ef17fdf4113dc911e8e197d048d23067b0285405d`。
- 分辨率测试初次尝试点击 720 时，首项部分滚出列表且点击未生效，输出保持 1024；没有将该次视频记为 720 通过。后续修复后，720 预设从 UI 点击切换已通过，证据 `popup-first-option-verification-20261003.json`；最终 APK 的 720 视频输出尚未在本轮重跑。
- 实际选区试听在 15 秒停止，PV 和谱面对象正常显示，截图 `saf-range-paused-20261003.png`。SAF 取消已确认从系统选择器返回应用后保留文件名，证据 `saf-picker-cancel-20261003.json`；系统 Back 在选择器子目录先返回父目录，须退出选择器根目录才能验证取消。

界面适配修正：文件名输入关闭预测/自动大写并偏好 Latin；选区输入请求数字键盘，实际数字键盘截图 `saf-numeric-ime-20261003.png`。任务进度和完成提示显示用户选定的 `Download/...`，不再显示内部暂存位置；文件名输入框本身仍显示内部路径，后续需要完整的显示/编辑投影。`AndroidMain` 将 Overlay 的逻辑尺寸与 transform 对齐工作区，解决弹出层未继承缩放造成的字号/尺寸差异；实际恢复对话框居中且遮罩覆盖完整工作区。`AppComboBox` 打开时定位到当前选择的前一项，避免第一项部分滚出视口。原始、缩放修正与列表定位修正截图分别为 `popup-scale-before-20261003.png`、`popup-scale-after-visible-20261003.png`、`popup-first-option-visible-20261003.png`。依据：[Qt Popup 的 Overlay 缩放说明](https://doc.qt.io/qt-6/qml-qtquick-controls-popup.html)。

验证：三项 CTest（AndroidFoundationSpec、ExportDestinationSpec、MobileExportAudioSpec）通过；新增目标显示路径检查与授权范围检查均通过。最终宿主实际导出页设置、选区试听与 WAV 输出回归通过，证据 `build-devtools/host/popup-final-export-ui-8675b420097643b89371045a7e84231a`。Overlay 修改后的 18 次统计区宽度变化通过，日志 `build-devtools/host/popup-scale-layout-20261003.log`。Android 与宿主均增量 Release 构建成功，无清理构建产物。

最终测试签名 APK 已安装到 emulator-5554，SHA256 `e577a8c1d04efe4660944ccb9fd26902c0dd98f982079436fc728a0a4c16542e`。92 个 ARM64 库 ELF LOAD 16KB 与 zipalign -P 16 再次通过，报告 `build-devtools/android-arm64-6.11.1/popup-final-apk-audit-20261003.json`。正式签名和真实 16KB 设备验证仍待完成。当前证据支持单次实际界面的 WAV/MP4 发布流程及弹出层修正，不能作为批量/封面或 P5 完成结论。

## 实际批量导出队列（2026-10-03）

实际 v2 `ExportSession` 批量页已接入本机队列。准备阶段在线程中读取每份目录中的谱面和音频时长；各难度独立校验并捕获偏移、媒体、时长、曲目信息、打拍参数和片头元数据。用户的渲染、音频、片头样式设置随任务保存；批量按完整范围导出，当前工程的选区和未保存正文不参与批量输入。无效谱面汇总为失败，其他有效项目继续执行；取消后可再次启动。

- 新增 `MobileBatchExportSpec`：重复目录与难度去重、同名输出、既有文件保留、未知/缺失难度、无效偏移与 UTF-8、缺少音频、分谱面的元数据与打拍参数、片头样式、长 Unicode 文件名和准备阶段取消均通过。
- 宿主实际批量 UI 回归通过：两份有效谱面导出成功，一份 `first=nan` 谱面失败；当前未保存且偏移无效的工作区仍保持正文、难度、路径、revision 与脏状态；实际取消入口停止队列，之后再次导出成功。证据：`build-devtools/host/batch-publish-ui-7e253a0ec16c4c98a43b764d2315e02c`。
- 宿主实际单曲 UI 回归通过设置传递、10–15 秒区间试听和 WAV 输出，证据：`build-devtools/host/batch-publish-single-ui-eb9ff94dc0a54ff48705804e699b9ee3`。四项 CTest 全部通过，日志 `build-devtools/host/batch-publish-ctest-20261003.log`；Release 构建日志 `build-devtools/host/batch-publish-build-20261003.log`。
- SAF 目录发布已实现同名文件增加序号，并在成功结果中返回提供方的实际文件名。Android Release APK 构建成功并安装到 `emulator-5554`（Android 14，ARM64 转译）。APK SHA256 为 `909b06127b4806555432842b782e0e9919674c6d6c9c2f73770a54dd0b927306`；92 个 ARM64 ELF 的 16KB LOAD 检查与 `zipalign -c -P 16 4` 均通过，报告 `build-devtools/android-arm64-6.11.1/batch-apk-audit-20261003.json`。真实设备兼容性仍需验证。

模拟器通过实际 v2 批量页添加三份独立测试目录，使用系统 SAF 选择 `Download/MiaCodeMobileBatch20261003/output`。两份目录内容有效，一份设置 `first=nan`；主工作区始终为 `白金ディスコ`，批量不替换其正文、难度和媒体。

- WAV：两次运行均为成功 2、失败 1。第二次重新授权相同公共目录，取得新的私有输出目录；公共目录预先存在的文件和第一次输出均保留。新增四份 WAV 均为 48kHz、双声道、16bit、276800 音频帧，时长 5.766667 秒，与对应私有完整文件逐字节一致。当前测试使用 2 秒冻结前导；短谱面的正文、尾部与音频按 30fps 对齐。证据 `build-devtools/android-ui/batch-native-qa-20261003/verification.json`，截图 `batch-native-result-complete-20261003.png`、`batch-native-repeat-complete-20261003.png`。
- MP4：成功的两份输出均为 720×720、30fps、173 视频帧、270 个 AAC 音频包，时长 5.766667 秒；最后视频 PTS 为 5733333 微秒，容器检查通过，公共/私有文件逐字节一致。证据 `mp4-verification.json`。从最终公共 MP4 解码三帧，在 2.9 秒样本可见实际谱面判定效果，`decoded-mp4/decoded.json` 和 `decoded-1.png` 保存解码证据。取帧检查工具按短视频实际时长选择采样时间，避免使用超出片长的固定采样点。
- 取消：仅延长测试专用导入副本的第一份谱面，在实际进度弹窗点击取消。任务在 3998 帧中的第 1968 帧停止，返回 `Export cancelled`，公共目录文件清单保持不变，后续项目没有继续执行。截图 `batch-native-cancel-direct-20261003.png`、`batch-native-cancel-complete-20261003.png`；证据 `cancel-task-report.json`、`cancel-verification.json`。此前自动点击未生效或已经晚于结束的尝试保留记录，未计为取消通过。原先保存的基线清单由 adb 的带转义分栏输出生成，验证时以 `shlex.split` 规范化后比对单列清单。
- 重试：取消终止后恢复测试副本原始字节，再通过实际界面启动；重新得到成功 2、失败 1，两份 MP4 的容器与公共/私有 SHA256 再次通过。证据 `retry-verification.json`，截图 `batch-native-retry-complete-20261003.png`。测试没有修改 G 盘原始工程。

上述 JSON 位于 `build-devtools/android-ui/batch-native-qa-20261003/`；截图位于 `build-devtools/android-ui/`。这些证据只覆盖模拟器前台批量流程，后续后台验证见下一节。批量格式目前依赖单曲输出扩展名，文件名输入和失败项目标签仍显示内部私有路径，需完善显示与格式选择。封面、片头曲绘及 90% 界面验收仍未完成。

## 批量后台执行与取消所有权（2026-10-03）

批量队列现在从准备阶段开始持有 Android 前台服务，经过各项渲染、编码和 SAF 发布后，在整个队列退出时统一释放。单曲仍独立持有自己的服务；服务是否释放依据实际启动状态，避免未授权任务关闭其他任务持有的服务。原生取消信号在队列生命周期内持续保存，准备阶段或项目切换时也会停止后续项目；关闭应用同样设置队列取消。通知显示整体批量百分比。

- 宿主 Release 构建成功：`build-devtools/host/batch-background-build-20261003.log`。实际 v2 批量页回归通过部分失败、界面取消、原生执行阶段取消、原生准备阶段取消、随后重试及工作区不变，证据 `build-devtools/host/batch-background-ui-37257532bd844bfdbed3cf18409e9f53/storage/host-fixtures/b7a43260-a6a8-401f-8bff-3955be3dc11d/batch-ui-proof/verification.json`。单曲实际导出页回归通过，证据目录 `batch-background-single-1858fcd16ab045c9bf8b66ccf4c91acb`。
- 四项 CTest 全部通过，日志 `build-devtools/host/batch-background-ctest-env-20261003.log`。首次启动的环境未包含 Qt DLL 路径，检查进程模块确认未加载 Qt；终止这次测试进程后，补齐 Qt PATH 与 offscreen QPA 重测通过。首次未完成运行不计为通过。
- 用于以下设备验证的 APK SHA256 为 `3d5d00c959d3542a003bbf801ca8ad582df9cb3d1cd91c2947c6992e1aa6e4c3`，构建日志 `build-devtools/batch-background-native-build-20261003.log`；92 个 ARM64 ELF 的 16KB LOAD 检查及 `zipalign -c -P 16 4` 均通过。它对应通知节流修改之前的构建。
- 允许后台：实际界面选择两份测试目录，只延长第一份测试导入副本。启动后切到 Home，系统活动记录确认前台为 Launcher。两份 MP4 分别完成 398 与 173 帧，容器检查通过，公共文件与对应私有文件逐字节一致。第一项完成而第二项运行时仍为同一个 `ServiceRecord{bfc9224}`，队列结束后服务退出；电源记录确认导出期间持有 `MiaCode:chartExport` 部分唤醒锁。证据 `allowed-verification.json`、`allowed-polls.json`、`allowed-service-poll-*.txt`、`allowed-activities-home.txt`、`allowed-power.txt`；截图 `batch-background-home-20261003.png`、`batch-background-allowed-complete-20261003.png`。
- 禁止后台：实际设置关闭“允许在后台导出”，启动并确认第一项任务目录已创建后切到 Home。当前项在编码收尾时返回 `Export cancelled`，公共目录保持不变，第二项未启动，前台服务未启动。398 帧已提交，取消发生在最终编码/发布完成之前，此记录不表示渲染阶段提前终止。证据 `denied-verification.json`、`denied-task-report.json`、`denied-setting-nodes.log`；截图 `batch-background-denied-setting-20261003.png`、`batch-background-denied-complete-20261003.png`。原有后台设置已恢复为允许。
- 通知取消初次验证尚未通过：系统记录显示通知及取消 PendingIntent 已创建，但队列每 50ms 更新通知，即使百分比不变也发送，系统界面自动化无法取得空闲状态。已返回应用取消该次长任务，2713/3998 帧终止，公共目录不变，服务退出，测试输入恢复原始字节。记录 `notification-first-attempt.json`，不计为通知栏操作通过。

通知服务已调整为仅处理变化的百分比、进度通知至少间隔 1.5 秒、仅提示一次并隐藏变动时间；服务退出后的排队进度回调不再更新通知。此修改的 Release 测试 APK 构建及签名验证通过，SHA256 为 `54673f9c57c63fb728ad3378da8537660142aa5314560e617216219feef72fe0`；92 个 ARM64 ELF 的 16KB LOAD 检查与 `zipalign -c -P 16 4` 通过。构建日志 `build-devtools/batch-notification-throttle-build-20261003.log`，审核报告 `build-devtools/android-arm64-6.11.1/batch-notification-apk-audit-20261003.json`。该 APK 已安装并通过实际通知栏取消重测：点击 SystemUI 中 MiaCode 通知的取消按钮后，任务在 1050/3998 帧停止，公共目录保持不变，前台服务退出，测试输入恢复原始字节。证据 `build-devtools/android-ui/notification-throttle-qa-20261003/verification.json`，截图 `notification-throttle-expanded-20261003.png`。自动化工具发现 `uiautomator dump` 失败时可能保留旧 XML，现要求本次命令明确返回输出路径，失败时拒绝读取旧节点或点击；失败保护及空闲界面的正常读取均有本轮实际调用记录。

本节设备 JSON、报告与系统记录位于 `build-devtools/android-ui/batch-background-qa-20261003/`，截图位于 `build-devtools/android-ui/`。仍需锁屏、系统中止及真实设备验证，不能据此宣布 P5 完成。

## v2 封面页面接入与 Android 实际导出（2026-10-03）

Android 现在直接加载 v2 `CoverExportPage.qml`，复用左侧图层列表、中间 `CoverComposer` 和右侧画板/图层检查器。`MobileCoverComposition` 使用已有导出引擎与请求服务，谱面帧播放控制接入 `MobilePreview` 的同一播放权威。移动端封面与谱面帧采用 `QQuickRenderControl` 离屏渲染，保留原有合成模型；成品和布局文件经 SAF 异步发布，失败时保留私有输出并报告错误。

宿主 Release 构建通过，7 个 CTest 通过，日志 `build-devtools/host/cover-integration-ctest-20261003.log`。实际封面 UI 点击导出按钮生成 720×720 JPG 和透明 PNG，主谱面来源、修订、难度及脏状态保持一致；证据 `build-devtools/host/cover-ui-e275e33470df4d42bff569886b844ddb/storage/host-fixtures/f6d295a6-bdd0-4490-9819-369b9225770c/cover-ui-proof/verification.json`。单曲和批量实际 UI 回归也通过。首次宿主封面测试仅生成 JPG 后超时，保留为失败记录；调整真实鼠标事件时间戳和弹窗关闭等待后才取得上述两份输出。

Android 首次封面 JPG 的曲绘出现黑块，设备日志包含 `glGenerateMipmap` 的 `GL_INVALID_OPERATION`。针对 Android 关闭难度卡曲绘的 mipmap 后，同一设备、同一曲绘的实际页面和导出结果恢复显示。修改位于共享 `MaimaiBannerCard.qml`，片头视频仍需重新导出验证，不能仅凭共享组件宣布片头问题解决。

修正后的 Release 测试签名 APK SHA256 为 `50722af39d37d4c11cab69a33b51e14392a89e83f843c4978d1734e4805d5d90`，日志 `build-devtools/cover-jacket-native-build-20261003.log`，审核报告 `build-devtools/android-arm64-6.11.1/cover-jacket-apk-audit-20261003.json`。92 个 ARM64 库的 ELF LOAD 16KB 检查及 `zipalign -c -P 16 4` 通过。共享资源修正后的宿主 Release 构建也通过，日志 `build-devtools/host/cover-jacket-build-20261003.log`。

在 emulator-5554 的真实封面页面操作所得证据：

- 添加谱面帧，拖动原有时间滑杆至 7.14 秒，实时显示对应音符。通过页面导出按钮生成 1024×1024 RGB JPG（239454 字节）及 RGBA PNG（531209 字节）；PNG 角落 alpha 为 0，两份成品均已人工查看。
- 用户选择的 SAF 公共目录与私有输出字节完全一致；公共目录已有 `card.jpg` 的 SHA256 保持 `4bdf39ddbd52f7ef50a48ee75fb979caf18c63b61314fec36c3a5ba259809723`。主谱面 source、savedSource、难度、素材、来源 URI、工作路径及后台设置保持一致。
- 通过真实系统保存界面写出 1684 字节、版本 3 的 `cover-layout.miacover`；确认重置后再通过系统文件选择器导入，重新导出的 PNG 与导入前逐像素一致，且公共/私有文件字节一致。

设备记录 `build-devtools/android-ui/cover-native-qa-20261003/verification.json`，截图 `cover-frame-seek-device-20261003.png`、`cover-transparent-device-20261003.png` 和 `cover-layout-import-device-20261003.png`。同名保留目前可能形成 `card(1)(1).jpg`，名称策略需统一。PNG 透明边缘和音符效果的视觉质量仍需与 v2 对照。以上只覆盖本次模拟器封面流程；字体/图片/文本导入、批量预设、空工程切换与后台/取消行为尚未完成验收，也未证明界面 90% 相似度或 P5 完成。

## 封面素材、预设与双谱面帧重开回归（2026-10-03）

通过 Android 实际封面页面及 SAF 文件选择器导入图片和 Xiaolai 字体，新增 `MiaCode` 文字，设置加粗并拖动图片、文字图层。私有导入图片与既有公共 `card.jpg` 的 SHA256 相同，字体库中的文件与仓库字体原件的 SHA256 相同。通过实际页面导出、保存版本 3 的 `cover-assets.miacover`、确认重置、再导入布局并导出，前后两份 PNG 的像素与文件字节均相同，公共与私有文件也相同。该素材往返测试使用 APK `50722af39d37d4c11cab69a33b51e14392a89e83f843c4978d1734e4805d5d90`；证据 `build-devtools/android-ui/cover-native-qa-20261003/assets-verification.json`。布局包含应用私有素材路径，尚未证明跨设备可携带性。

双谱面帧预设暴露两个独立问题：Android GLES 拒绝部分 BGRA 纹理的 mipmap，非活动帧显示为空；关闭再打开封面页时，清空了非活动帧缓存，却没有重新生成。这一轮在 `CoverComposer.qml` 的背景、内圈背景、静态谱面帧及图片图层上关闭 Android mipmap，并在 Android 离屏渲染器准备完成后重建非活动帧。宿主实际封面 UI 回归新增关闭/重开操作和第三次双帧导出，检查非活动帧图片存在及主工作区保持不变。

- 宿主 Release 构建成功，实际页面连续三次导出成功，重开后两帧直接显示；报告 `build-devtools/host/cover-still-9cc9a1f3d2e848e587a423d50e1aa6a3/storage/host-fixtures/361097d6-d08d-44a0-8f81-5f84167ea6da/cover-ui-proof/verification.json`。7 项 CTest 通过，日志 `build-devtools/host/cover-still-reopen-ctest-20261003.log`。
- 最终 Android Release 测试签名 APK SHA256 为 `bd88c6522c2f4e03a7d87d3ca74a4bc73818f61373b18616eeedf7672644c1b4`，构建日志 `build-devtools/cover-still-reopen-native-build-20261003.log`，审核报告 `build-devtools/android-arm64-6.11.1/cover-still-reopen-apk-audit-20261003.json`。92 个 ARM64 ELF 的 16KB LOAD 检查与 `zipalign -c -P 16 4` 通过，已安装到 emulator-5554。
- Android 实际页面重开后，7.14 秒与 11.68 秒的两个帧均直接显示，未通过切换活动图层触发修复。截图 `build-devtools/android-ui/cover-dual-reopen-final-device-20261003.png` 已查看。实际按钮输出 1024×1024 RGBA PNG，245554 字节，SHA256 `f318bbf797476e6d77f6087f7d8a3ddb1f033216a95e1bbb944c1fe4739b193a`；公共与私有文件逐字节相同，角落 alpha 为 0。
- 实际页面保存用户预设 `CoverQA`，重启并更新 APK 后再从预设菜单应用。持久化布局与预设布局完全相同，难度卡、谱面帧、图片和带自定义字体的文字均恢复。实际按钮导出 1024×1024 RGBA PNG，590726 字节，SHA256 `4c9763d2dd9d781857c4220c051c710fe7b5906a3f47a7ba886f6154ecfd095b`；公共与私有文件逐字节相同。截图 `build-devtools/android-ui/cover-user-preset-restored-device-20261003.png` 和导出成品已查看。没有将不同 APK 之间的输出字节一致性计作通过。
- 上述最终 APK 两次导出前后，主工作区正文、保存正文、难度、素材、来源 URI、工作路径、PV 禁用及后台设置均保持一致；公共目录原有 `card.jpg` 的 SHA256 保持不变。设备证据分别为 `build-devtools/android-ui/cover-native-qa-20261003/dual-verification.json` 与 `user-preset-verification.json`。

这些证据覆盖 Android 14 模拟器的素材、布局往返、用户预设恢复和双帧重开流程。仍需完成批量封面、其余预设与检查器选项、空工程切换、后台/取消、片头视频、真机与比例截图验收；P5 尚未完成，也尚未证明 UI 90% 相似度。本轮没有提交或推送，v2 最新更新仍按前述顺序安排在 P5 验收及安卓端提交、推送之后。

## 封面工程切换与最后一个难度删除（2026-10-04）

工程替换时，封面会话现在先停止播放并脱离借用的谱面场景，再清除旧难度、渲染器及谱面帧缓存；保留用户的布局、素材和输出选择。同一工程删除最后一个难度时也执行清空，避免难度编号为 0 的重复选择提前返回并继续显示旧工程内容。

- 宿主 Release 构建成功，实际封面页面回归连续三次导出，并覆盖关闭工程、无难度工程、删除最后一个难度、空输入导出拒绝及原难度恢复；报告 `build-devtools/host/cover-last-difficulty-0bd2e797b3bf486f8a93c0163d8386a1/storage/host-fixtures/cc1bfb76-72c1-4c18-a9ae-7fe32b1b8fd4/cover-ui-proof/verification.json`。7 项 CTest 通过，日志 `build-devtools/host/cover-last-difficulty-ctest-20261004.log`。
- Android 工程替换回归使用 APK SHA256 `f6d65c0bc358ab46734cfe02965429ca0ef9170108526a029414a2f474bfbea2`。通过实际 SAF 打开无难度文件，旧曲绘及谱面帧清空，图片与文字图层保留；实际导出按钮显示未选中难度提示，公共目录保持不变。再通过实际 SAF 打开完整工程，恢复难度 5 及四份素材；素材与先前导入副本逐字节一致，布局和输出选择保持一致。恢复后的实际导出为 1024×1024 RGBA PNG，590725 字节，公共/私有文件一致；证据 `build-devtools/android-ui/cover-lifecycle-native-qa-20261003/verification.json`。
- 最新 Release 测试签名 APK SHA256 `841b1dd736911d9d979dbbe386f782e90b6025b1e64d800a3dfcc9cc0818e39d` 包含最后一个难度删除的修复。构建日志 `build-devtools/cover-last-difficulty-native-build-20261003.log`，92 个 ARM64 ELF 的 16KB LOAD 检查及 APK 对齐检查通过，审核报告 `build-devtools/android-arm64-6.11.1/cover-last-difficulty-apk-audit-20261003.json`。
- 在最新 APK 的实际难度管理页面删除唯一难度后，恢复记录为难度 0、正文 97 字符；重开封面页，旧谱面帧与曲绘清空，图片和文字仍保留。实际导出按钮拒绝导出，公共目录未新增文件。已查看截图 `cover-last-difficulty-empty-device-20261004.png` 和 `cover-last-difficulty-reject-device-20261004.png`。
- 最新 APK 随后的完整工程 SAF 重导入挂起，尚未通过：停在“正在处理文件”，原工作路径和难度 0 保持不变。Java 主线程停在 `QtNativeAccessibility.childIdListForAccessibleObject`，文件复制线程已空闲；Qt 主线程等待点在模拟器 ARM64 翻译层内，尚不足以判定根因。日志、Java/native 线程栈保存在上述生命周期证据目录的 `last-difficulty-reimport-*` 文件中。`last-difficulty-restored-session.json` 仅是挂起时快照，其文件名不代表恢复成功。旧 APK 的成功报告不能替代这一轮重导入验证。

页面截图中还观察到难度卡上方的异常纹理图形，导出 PNG 中未观察到相同图形；需继续定位并与 v2 对照。以上仍是局部回归证据，P5 和 90% 界面验收均未完成。本轮未提交或推送；v2 最新更新继续安排在 P5 验收、安卓端提交并推送之后。

## Android PV 重导入挂起定位（2026-10-04）

重新启动后第一次完整工程导入成功；执行实际封面打开、删除唯一难度、空输入导出拒绝，再通过 SAF 重导入完整工程时可重复挂起。复测在允许文件夹访问后只轮询应用恢复记录，没有再调用 accessibility；Java UI 线程处于事件循环空闲状态。因此先前的 accessibility 等待栈不能作为根因结论。

按需启动参数 `--trace-render-loop` 记录渲染循环、文件导入及媒体替换的阶段。正常启动不启用这些调试日志。日志确认返回应用后的 QSG 同步和渲染已结束，文件结果已收到，挂起发生在 `workspace.openSource()` 同步通知的 `MobilePreview::refreshMedia()`：音频替换已返回，视频替换尚未返回。证据为 `build-devtools/android-ui/cover-lifecycle-native-qa-20261003/media-trace-reimport-logcat.txt`。

预览现在保存内圈视频输出的生命周期引用，在替换视频来源、替换输出和解绑时清除内外圈输出持有的帧。候选 Release 测试签名 APK SHA256 `1eba3dc946e262fba29f15272379e7e392559fa3a9aee650a82d2bd2c68d1e03` 构建成功，92 个 ARM64 ELF LOAD 16KB 检查和 APK 对齐检查通过；报告 `build-devtools/android-arm64-6.11.1/preview-output-release-apk-audit-20261004.json`。

- 宿主 Release 构建及 7 项 CTest 通过，日志 `build-devtools/host/preview-output-release-ctest-20261004.log`。实际封面页面回归连续三次导出，覆盖双帧重开、删除最后一个难度、关闭工程、空工程拒绝导出及原工程恢复；报告 `build-devtools/host/preview-output-release-36aa5c06a22f4770997a52c6313e76ed/storage/host-fixtures/65ce1ba7-456a-4dd9-b3ab-9591685e60aa/cover-ui-proof/verification.json`。
- Android 同样操作仍挂起。新增阶段日志证明 `pause()` 和清空视频输出已返回，停在 `video_.setSource()`；恢复记录仍为难度 0、正文 97 字符。`preview-output-release-reimport-logcat.txt`、`preview-output-release-reimport-session.json` 和 `preview-output-release-native-stacks.txt` 保存实际结果，候选修改尚不能计为挂起修复通过。
- 第一份 `preview-output-release-reimport-polls.json` 使用默认 GBK 读取 Android UTF-8 输出，解码失败，其 `raw: null` 无验证效力。后续显式 UTF-8 的 `preview-output-release-reimport-polls-corrected.json` 与独立 session 快照确认原记录保持不变。

线程栈还显示 MediaCodec 输入缓冲等待；需要对照硬件及软件解码，再确认旧播放管线的退出条件。开发诊断参数 `--software-preview-decoding` 仅用于启动时的对照测试，通过 Qt 的解码设备选择变量禁用硬件解码；普通启动保持默认硬件解码策略。该测试不能代替真机硬件解码、完整封面或 P5 验收。

同一比较版 APK SHA256 `ab836619d42b9f23df8aa4869fcffbebd719f18cafdc93a1be33f89a803dcaf4` 增加 Qt FFmpeg 播放管线阶段日志和上述软件解码诊断参数。Android 与宿主 Release 构建成功，7 项 CTest 通过；构建日志 `build-devtools/decoder-comparison-build-20261004.log`，CTest 日志 `build-devtools/host/decoder-comparison-ctest-20261004.log`，92 个 ARM64 ELF 和 APK 对齐审核 `build-devtools/android-arm64-6.11.1/decoder-comparison-apk-audit-20261004.json`。

- 软件解码：实际封面页显示 PV 与谱面帧，关闭后删除唯一难度，重开并通过实际按钮确认空输入拒绝导出，再经 SAF 重导入。4 秒时恢复 Master 难度、2243 字符正文及新工程路径；线程记录包含 `av:h264:df` 软件解码线程。`software-decoder-verification.json`、`software-decoder-reimport-polls.json`、`software-decoder-reimport-logcat.txt` 和已查看的 `software-decoder-reimport-complete-20261004.png` 是这一组证据。
- 默认硬件解码：同一 APK 恢复默认启动选择，实际封面显示正常；再次执行封面关闭、唯一难度删除、空输入导出拒绝和 SAF 重导入，16 秒内记录均保持难度 0、97 字符正文和旧路径。日志停在旧视频 `Delete PlaybackEngine`，未出现视频替换返回。`hardware-decoder-verification.json`、`hardware-decoder-reimport-polls.json`、`hardware-decoder-reimport-logcat.txt`、`hardware-decoder-native-stacks.txt` 保留这一失败证据。

对照把问题范围收敛到当前 Android 14 x86_64 模拟器 ARM64 翻译环境的硬件解码退出路径；尚不能推定所有 ARM64 真机同样失败。软件解码仅是诊断对照，未作为产品默认设置，也未作为硬件解码问题解决或 P5 通过的依据。原始 G 盘谱面与公共测试素材未修改。本轮未提交或推送，v2 更新同步顺序保持不变。

捕获失败线程栈并确认没有导出服务运行后，重启测试应用，采用普通启动和实际 SAF 再次导入原工程，恢复 Master 难度与 2243 字符正文；已查看 `normal-start-restored-20261004.png`。普通启动日志中未出现新增阶段诊断日志。`normal-start-recovered-session-20261004.json` 和 `normal-start-logcat-20261004.txt` 保存恢复状态；此次初始导入成功不替代封面操作后重导入失败的结论。G 盘原始 `maidata.txt` SHA256 仍为 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。

## 批量封面实际入口与后台失败记录（2026-10-04）

Android 的实际 v2 封面页增加批量入口，使用现有 AppDialog 控件。用户可选择多个难度及当前布局、四种内置预设、保存的用户预设。任务启动时复制谱面、布局、卡片、字体、背景与输出配置，按难度和预设组合顺序渲染；结果经同一 SAF 发布器写入用户选择的目录。成品名称包含曲名、难度和预设，重复输出保留原文件。队列与单张封面、视频导出互斥，支持取消、失败后重试及逐项错误，保留当前编辑状态。

宿主实际鼠标点击回归覆盖两难度乘两预设共四张、启动后取消、重试、缺失图片的部分失败，以及非法预设拒绝。最终资源预初始化候选版报告为 `build-devtools/host/cover-page-prewarm-3ea66a1ca24144138e7200aabdf94a54/storage/host-fixtures/402cbc04-0b7a-4362-8e61-12c43b5df277/cover-batch-ui-proof/verification.json`；四阶段成功/失败/取消分别符合预期，工作区正文、修订、难度、脏状态、布局和持久化设置保持不变。7 项 CTest 通过。宿主运行使用 OpenGL 和 basic 渲染循环；此前默认 D3D 离屏测试崩溃及 threaded OpenGL 超时保留为失败记录，这一宿主配置不能替代 Android 的默认渲染循环验证。

Android 实际 UI 验证保存在 `build-devtools/android-ui/cover-batch-native-20261004/`：默认卡片和纯谱面帧两张透明 PNG 均为 1024×1024，公共与私有文件大小、SHA256 相同，主会话保持不变。随后通过实际页面选择当前布局、四个内置及 `CoverQA` 共六项，前台全部完成。切到 Home 后通过导出服务的通知取消入口取消，六项均成为取消状态，服务停止；取消后回到前台重试完成。第一轮读取远端文件曾误用 Windows 路径分隔符，得到的错误文本不算图片证据；修正为 Android 正斜线后才核对 PNG 签名、尺寸和字节。

后台完成尚未通过。原版、批次复用渲染上下文版，以及在打开封面页时提前初始化资源版，均在开始后立即切至 Home 的 12 秒内没有新增公共文件，返回应用后队列继续。前台服务仍在运行，因此服务存在不能作为导出完成依据。三组报告分别位于 `cover-batch-native-20261004`、`cover-batch-context-native-20261004` 和 `cover-page-prewarm-native-20261004` 的 `background-verification.json`。最后一组另保存 Android 线程栈与日志；其 Release 测试签名 APK SHA256 为 `40f0a99058a7f64671478a60910e01a95474cd8ad0e6957516b906e6ec917019`，92 个 ARM64 ELF 的 16KB LOAD 检查和 APK 对齐检查通过。候选修改不能计作后台问题解决。

新增按需批量阶段日志，仅在 `--trace-render-loop` 诊断启动时启用，区分排队、谱面帧、合成和 SAF 完成阶段。普通启动不打印阶段日志。单项对照日志显示队列已经准备完成，但切到 Home 后首个 `advance` 尚未执行，窗口 `handleObscurity` 等待渲染线程；返回前台后才出现渲染线程 `WM_Obscure` 和任务推进，最终发布成功。证据位于 `cover-batch-stage-trace-native-20261004/{background,resume}-logcat.txt`。Qt 6.11.1 [窗口隐藏处理源码](https://github.com/qt/qtdeclarative/blob/v6.11.1/src/quick/scenegraph/qsgthreadedrenderloop.cpp)包含对应的线程等待。这支持继续检查窗口暂停与渲染线程之间的等待，尚不足以判定所有 Android 真机都受影响。

打开封面页时预初始化的尝试未解决问题，已撤回，避免增加页面启动成本。批次内仍复用两套有界渲染器。新增 `--basic-preview-render-loop` 仅供渲染循环诊断对照，普通启动保持 Qt 默认选择。当前仍需定位后台停顿并复测用户禁止后台、锁屏、失败恢复和真机；完整 UI 比例/90% 相似度验收、P5 和正式发布均未完成。此轮没有提交或推送，v2 最新更新继续遵循先完成 P5 验收、提交及推送安卓成果、再补全的顺序。

渲染循环对照最终 APK SHA256 为 `9fb54129a65d6fea58ce5baa34adb035001e31679ba5ac211014a8bc911501b2`，ARM64、92 个 ELF LOAD 16KB 及 APK 对齐检查通过；报告 `build-devtools/android-arm64-6.11.1/cover-render-loop-comparison-apk-audit-20261004.json`。basic 的初次立即 Home 测试没有在切换前观察到任务启动，已明确标记为无效。重测先观察到六项队列准备完成，再切到 Home；首项谱面帧已生成，但 12 秒内没有公共成品，返回前台后六项均完成并停止服务。证据 `build-devtools/android-ui/cover-basic-loop-six-native-20261004/background-verification.json` 与前后台日志。由此不能把切换 basic 当作后台问题修复，后续还需定位封面合成时的具体等待。

撤回页面预初始化后的最终宿主 Release 构建、实际单张与批量 UI 回归及 7 项 CTest 通过；日志 `build-devtools/host/cover-render-loop-comparison-{build,ctest}-20261004.log`，两份 UI 报告位于 `build-devtools/host/cover-loop-final-1ac70bb0fb4747a3a6bbb7d3985ead4c/`。三种语言的 21 个批量封面文案 ID 集合一致，暂存与未暂存差异的 `git diff --check` 均通过。测试结束确认无导出服务运行，再以普通参数重启测试应用；未修改产品默认渲染或硬件解码策略。G 盘原始谱面 SHA256 保持 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。

## 封面合成的后台等待点与直接渲染回归（2026-10-04）

在原有六项后台对照中加入 QML engine、组件创建、事件处理与离屏渲染阶段日志。实际记录显示 engine、组件及 QML item 均已创建，停在第一次嵌套 `QCoreApplication::processEvents()`，尚未进入离屏帧渲染；返回前台后该调用返回并继续。证据 `build-devtools/android-ui/cover-composition-phase-native-20261004/{background,resume}-logcat.txt`。这一诊断 APK 只用于定位，不能计为问题解决。

移动端封面合成现在直接驱动其离屏场景的三次 polish/render，使用现有同步加载的本机图片和字体，移除上述嵌套事件处理；取消与文件发布仍在队列检查点处理。候选 Release 测试签名 APK SHA256 为 `30337300ff7d988746c3024b8d5078f5ee2d7be8c80cc49e6c1984635a82012d`，92 个 ARM64 ELF LOAD 16KB 和 APK 对齐检查通过，审核 `build-devtools/android-arm64-6.11.1/cover-direct-polish-apk-audit-20261004.json`。

- 宿主 Release 构建、实际单张与批量封面 UI 回归、7 项 CTest 通过。报告位于 `build-devtools/host/cover-direct-polish-bb7d9aab932e493d8c37c65903af76b8/`，日志 `build-devtools/host/cover-direct-polish-{build,ctest}-20261004.log`。
- Android basic 诊断对照先确认六项队列已准备完成，再切至 Home。8 秒时第一张公共 PNG 已生成；24 秒时仍只有这一张，尚未收到首项完成回调，队列未继续。这仅证明移除嵌套处理后首张能够在后台渲染，不能证明批量后台完成。
- 返回前台后六张全部发布，服务停止。逐张检查 1024×1024 RGBA PNG 签名、尺寸和 SHA256，均能匹配完整私有输出；主会话的正文、保存正文、难度、素材、来源 URI、工作路径、PV 和后台设置保持一致。双帧与纯谱面帧输出与前一版逐像素相同；四张含卡片的输出有局部像素差异，已查看当前及前版默认卡片与用户预设成品，未将跨 APK 像素一致性计为通过。证据 `build-devtools/android-ui/cover-direct-polish-basic-native-20261004/background-verification.json`，该报告保留 `P5Accepted: false`。

- 同一 APK 使用产品默认渲染循环，仅开启阶段日志，复测同样先确认六项队列准备完成再切至 Home。8 秒时生成第一张公共 PNG，24 秒时仍只有一张；后台批量完成未通过。返回前台后六张最终发布，逐张 PNG 尺寸和私有输出哈希匹配，主会话八项字段保持一致，服务最终停止。证据 `build-devtools/android-ui/cover-direct-polish-default-native-20261004/background-verification.json`；其中 `sixOutputsVerified: true` 仅表示返回前台后的成品检查，`completeSixWhileHome: false` 和 `P5Accepted: false` 保留。
- 默认循环前台续跑期间，最后两项的 QML item 创建分别出现约 80 秒和 76 秒的等待，Android activity 服务状态查询也曾超时，稍后恢复。最终六张成品不构成性能通过证据。保留 `current-logcat.txt`、`current-service-check.txt`、`background-native-stacks.txt` 供后续区分应用等待与模拟器环境问题；原生栈主要落在 ARM64 转译系统调用，尚未取得足以确认根因的原生调用链。

任务之间的窗口/事件等待及前台长等待仍需定位，ARM64 真机后台行为仍待验证。当前 x86_64 模拟器运行 ARM64 转译代码；原生 ABI 对照有助于区分应用与转译环境的问题，但下载对应 Qt 工具链的请求遇到超时和 TLS 连接失败，尚未取得工具链或执行该对照。没有因此改变产品默认渲染、解码或目标 ABI。P5、设备矩阵及界面 90% 验收仍未完成；本轮没有提交或推送，v2 更新同步顺序保持不变。

## 后台原生配置与窗口隐藏对照（2026-10-04）

继续定位批量封面后台停顿，完成两项实际 Android 对照，均未计为修复：

- 原生配置候选：在 `QGuiApplication` 创建前设置 `QT_BLOCK_EVENT_LOOPS_WHEN_SUSPENDED=0`，与现有清单的后台运行配置一致。[Qt 6.11.1 加载器源码](https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/android/jar/src/org/qtproject/qt/android/QtLoader.java) 将对应清单配置转换为该环境变量。候选日志确认原生侧读取值为 `0`；准备六项队列后切至 Home，6 秒生成首张，24 秒仍只有一张。回到前台后六张完成，PNG 尺寸及私有文件哈希匹配，主会话八项字段不变，服务停止。报告 `build-devtools/android-ui/cover-native-background-env-20261004/background-verification.json`；候选 APK SHA256 `eae67588fa00744a32d5a8907be29531ded591f2c2580775be6c97e6d0584c14`。环境变量空值本身尚不能确认为根因。
- 窗口隐藏候选：在上述候选中增加仅由诊断参数启用的应用状态变化处理，后台隐藏主窗口并在前台恢复原可见状态。六项队列准备后切至 Home，6 秒生成首张，24 秒仍只有一张。日志仍停在主窗口 render loop 的同步等待，没有取得后台隐藏生效的证据，不能据此判定成功。返回前台后六张完成，同样通过成品及会话一致性检查，服务停止。报告 `build-devtools/android-ui/cover-hide-background-window-20261004/background-verification.json`；候选 APK SHA256 `324c40221764813cf8f745ef387ac0eefdaaf5ee2dd2cd21bc011d47c3c66806`。

两项候选均已从源码撤回，并重新构建恢复后的版本。恢复 APK SHA256 与本轮前版本相同，为 `30337300ff7d988746c3024b8d5078f5ee2d7be8c80cc49e6c1984635a82012d`，已重新安装并以普通参数启动。两项构建和恢复构建中的 92 个 ARM64 ELF LOAD 16KB 检查及 APK `zipalign -c -P 16 4` 检查通过，但不构成真实 16KB 设备或 P5 验收结论。失败记录保留，不重复采用同一假设作为修复。下一步需检查后台导出与主窗口的渲染生命周期隔离，并取得原生 ABI 对照或真机证据。界面相似度、完整功能、性能和设备矩阵仍需继续完成。

## 实际 v2 参照构建与六比例截图（2026-10-04）

在移动工程的 `build-devtools/desktop-reference-c190bb2c` 中重新构建实际桌面 v2，用于界面验收参照。源版本为当前迁移基线 `c190bb2c138cf61032dd7ac97ec41027da4bb40d`，使用完整 Qt 6.10.3、Ninja、Release 和 4 个并发任务，586 步构建完成。39 项 Qt 组件缓存路径均指向 6.10.3，部署的 Qt6Core.dll 版本也为 6.10.3.0。构建日志为 `build-devtools/desktop-reference-c190bb2c-build-20261004.log`。这一操作仅生成验收参照；v2 工作区仍干净，后续最新更新的移植仍安排在 P5 验收及安卓提交、推送之后。

Android 14 模拟器 `emulator-5554` 已取得 16:9、19.5:9、20:9、21:9、4:3 和 16:10 六种横屏比例的实际窗口截图，记录位于 `build-devtools/ui-parity-20261004/native-matrix/matrix.json`。六种比例均显示主编辑、预览与时间轴面板。4:3 首张在密度切换时尚未稳定，正式检查使用 `4-3-settled-3.png`；连续最后两张顶部界面像素相同。测试结束恢复模拟器原尺寸和密度。这些截图验证的是模拟器布局，不能替代不同真机、Android 12、触摸命中或性能验证。

用于成对截图的工程副本位于 `build-devtools/ui-parity-20261004/fixture`。其中 `maidata.txt` 使用当前 Android 会话快照，其余媒体来自授权测试工程；原始复制文件哈希与快照文件哈希分别记录，不能混作同一来源。G 盘原始 `maidata.txt` 的 SHA256 再次核对仍为 `5e6bcc786b67b764938188f59d3bbf54efcbb508b4b2385b92248d25e10926a1`。

实际 v2 已启动并显示既有谱面工作区与恢复提示，但窗口控制工具在关闭提示时报告 `failed to activate captured window`；重新定位唯一参照窗口并恢复绑定后仍报同一错误。本轮未完成匹配快照的载入及成对截图，未计算相似度，`pairedUiAccepted` 与 `P5Accepted` 均保留为 false。菜单、设置、导出页等界面对照仍须按下述方法完成。

## 离线视频导出的画质与体积设置（2026-10-04）

补齐 Android 编码器对 v2 导出设置的实际消费。原先视频码率只按分辨率与帧率固定计算；现在抽出 v2 现有码率规则为共享 `videoExportTargetBitrateKbps`，单曲与批量均使用任务快照中的画质、体积与音频码率设置。体积模式也控制关键帧间隔及 PV：关闭 PV 的极小体积模式在独立导出快照中解析静态背景，不改变主工作区。

Android 编码器查询支持的码率范围，优先使用支持的 VBR，否则使用支持的 CBR；请求值超出设备范围时按范围限制，报告同时记录请求值与配置值。能力查询依据 Android 官方 [EncoderCapabilities](https://developer.android.com/reference/kotlin/android/media/MediaCodecInfo.EncoderCapabilities) 与 [VideoCapabilities](https://developer.android.com/reference/android/media/MediaCodecInfo.VideoCapabilities.html)。报告中的码率表示传给编码器的配置，不能理解为每段视频的实际平均码率或严格文件大小保证。

宿主 Release 全量构建和 8 项 CTest 通过，日志 `build-devtools/host/encoding-policy-ui-{build,ctest}-20261004.log`。新增规格覆盖 v2 的画质档、体积预设、码率上下限及批量设置快照。实际 v2 ExportSession 的页面回归通过：选择高质量、较小体积及 320 kbps，最终任务记录正确的画质、体积、320 kbps 请求值与 160 kbps 有效上限，同时保留 10–15 秒选区播放和 WAV 输出验证。报告位于 `build-devtools/host/encoding-policy-ui-valid-20261004/storage/host-fixtures/f656caa4-274b-490d-ae22-211ea177bfc6/export-ui-proof/verification.json`。首次宿主调用误把目录作为 `--fixture` 输入，返回 6，已修正为副本的 `maidata.txt`，首次调用不算通过。

实际 Android 14 ARM64 转译模拟器完成五组 720×720、30fps、48 帧 MP4，均包含音轨、通过帧数与时间戳检查，且主会话八项字段保持不变。证据位于 `build-devtools/android-ui/encoding-policy-native-20261004`，各组保存设置、任务、编码器与文件哈希报告：

| 验证组合 | 视频配置 | AAC 配置 | 关键帧间隔配置 | PV |
|---|---|---|---|---|
| 快速 / 标准体积 | 2.2 Mbps | 320 kbps | 2 秒 | 保留 |
| 高质量 / 标准体积 | 2.6 Mbps | 320 kbps | 2 秒 | 保留 |
| 快速 / 较小体积 | 1.8 Mbps | 160 kbps | 4 秒 | 保留 |
| 快速 / 极小体积 | 4 Mbps | 128 kbps | 6 秒 | 静态背景 |
| 快速 / 极小体积保留 PV | 4 Mbps | 128 kbps | 6 秒 | 保留 |

上述数值沿用现有 v2 规则，体积预设并不保证在所有分辨率和短片长度下文件大小单调变化。实际高质量与关闭 PV 两份最终 MP4 再经宿主 Qt Multimedia 解码取帧，已查看对应 PV 画面与静态背景，证据 `decoded-high` 和 `decoded-ultra`。短片和配置报告不构成长任务性能、所有设备编码器或真实关键帧间隔测量的通过证据。

普通 Release 包按 Qt 的规则忽略外部 `extraappparams`，首次安卓诊断启动没有触发导出，已记录为无效。随后使用固定验收参数的独立诊断 APK，92 个原生库与普通 APK 逐字节相同。首组监视脚本曾把远端尚未出现的报告错误文本作为 JSON 解析；核对实际任务完成后继续观察同一任务，未重启该导出。所有验证完成后已清除 CMake 诊断参数、重新打包并安装普通 APK，清单无诊断参数，普通 APK 与诊断前逐字节相同。模拟器出现 Pixel 启动器 ANR，选择等待后继续；恢复普通包时同一启动器提示再次出现，关闭异常启动器后通过实际恢复入口载入主工程。该环境异常单独记录，未计为应用功能通过或失败。最终主会话八项字段再次核对不变，无导出服务运行；普通启动截图为 `build-devtools/android-ui/encoding-policy-final-restored-20261004.png`，五组汇总为 `encoding-policy-native-20261004/verification.json`。

当前普通 Release 测试签名 APK SHA256 为 `f3f0aeab600b29fd810c64723a40d1f06909ce712af8451e2de32bedb65e6f69`。92 个 ARM64 ELF LOAD 16KB、APK 对齐及测试签名校验通过；恢复记录 `build-devtools/android-arm64-6.11.1/encoding-policy-ordinary-restored-20261004.json`，最终审核 `encoding-policy-final-apk-audit-20261004.json`。仍需真实设备及正式签名验证。v2 工作区保持干净，G 盘原始谱面哈希不变；本轮没有提交或推送。P5、90% 成对界面验收及批量封面后台问题仍未完成。

## UI 验收方法

在相同工程、难度、主题、预览时间和内容状态下获取 v2 与 Android 截图，按对应面板对齐。保留原始截图、对齐方式和差异图。权重：工作区结构 30%、编辑器/元数据 25%、预览与时间轴 25%、菜单/设置/导出 20%。逐项检查布局、字体、间距、颜色、图标、边框与交互状态，达到加权 90% 且无主要面板缺失才可通过。

比例矩阵：16:9、19.5:9、20:9、21:9、4:3、16:10。检查系统安全区、软键盘、全屏预览、弹窗、字体导入和手势命中区域。允许按用户屏幕比例分配面板，不能用裁剪来掩盖缺失内容。

## 产品功能验收

编辑：文本、语法高亮、多难度、整理、撤销/重做、查找替换、元数据、保存和恢复。

检测：原生解析、语法诊断、动态无理分析；快速编辑与切难度后拒绝旧结果；点击诊断定位正确。

预览：真实音符与波形、播放线、缩放、音频/PV/音效同步、拖动和跳转、编辑器跟随、全屏与生命周期恢复。

导出：真实谱面 MP4/WAV、选区/片段、片头、批量、封面背景/字体/图片/文本/谱面帧/预设；无网络条件下成功；取消和失败可恢复；后台权限按用户选择执行。

发布：API 31、ARM64、16KB、手机和平板、代表设备性能、来源与许可证、正式签名 APK。测试签名 APK 和编码探针不能替代这些验收项。
