# 无理检测参考实现与稳定版对照（2026-10-20）

## 对照范围与方法

- 输入：桌面 `Chart/` 下 27 份主 `maidata.txt` 的 Master（`&inote_5`），排除 `.miacode` 自动保存文件。
- 稳定版：`v1.0.0`（`e9aa9484`）、`v1.2.0`（`2f5bb005`）。分别取标签中的解析器、无理检测源文件和 slide 数据，使用同一 Qt 6.10.2 / Release 构建独立诊断程序。
- 修复前：`e2a9005f` 的无理分析实现；修复后：本次工作区代码。保留各谱面的 `&first`，默认手部半径 30 px，静态撞尾阈值 200 ms，默认排除 touch 多押。
- 比较运行时诊断的类型、级别、发生时刻、行列和 marker key，并比较全部解析 marker 与静态参考。翻译参数、详情措辞单独比较，不当作判定变化。
- 参考仓库：MaiMuriDX，固定到 [`0ebab7ff`](https://github.com/Minepig/MaiMuriDX/tree/0ebab7ff6853a159ef6af76d9e3c43b23157bb5e)。实际运行 Python 参考实现，同时检查代码与开发笔记；普通手部半径显式设为与 MiaCode 相同的 30 px。

`snowloop`、`undress me` 的 Master 在四个版本中均有解析错误；保留其输出用于比较，但不计为成功解析的完整谱面。其余 25 份成功解析。没有修改桌面原谱。

## 三个案例的结果

表内为运行时诊断数；各版本的三首谱面解析 marker 完全一致。

| 谱面 | 1.0.0 | 1.2.0 | 修复前 | 修复后 |
| --- | --- | --- | --- | --- |
| 超限导能~Overchannel | 多押 1 | 多押 1 | 多押 1 | 0 |
| 海神 | 内屏 1 | 内屏 1 | 内屏 1 | 内屏 1 |
| いとをかし | 多押 2 | 多押 2 | 多押 2 | 0 |

三处问题在两个稳定版中均已存在，没有发现它们是后续重构或近期同起点 slide 告警修改引入的偏差。1.0.0 到 1.2.0 新增了详情结构化参数和内屏判定窗口说明；1.2.0 到修复前为 hold 标签补了 `h`，这些均未改变三例的判定时刻或级别。

### 超限导能~Overchannel

```simai
(146){16}6qq3[4:4]/4<2[4:4]*<2<4b[4:8],,,,,,,,,,,,,,,,,,,,,,,,E
```

重合段进行中已有触点合并；误报出现在短 slide 结束后的 16.7 ms 抬手延迟中。长连锁此时进入折返段，动作 segment 索引和切线都已变化，原有的合并延续规则无法再把短 slide 的停留与长连锁合并，于是连同 `6qq3` 算成三只手。

参考实现会在结束前半个判定 tick 检查位置和切线：若短 slide 结尾嵌入另一条已离开首 area、尚未到达最终 critical 的 slide，则标记为 `before_slide`，取消结尾停留。见 [`post_parse_workup`](https://github.com/Minepig/MaiMuriDX/blob/0ebab7ff6853a159ef6af76d9e3c43b23157bb5e/majparse.py#L498-L531) 和 [`ActionSlide`](https://github.com/Minepig/MaiMuriDX/blob/0ebab7ff6853a159ef6af76d9e3c43b23157bb5e/action.py#L83-L130)。

本次在分析内部补齐该结尾规则，沿用距离小于 20 px、方向差小于 3° 的条件。按参考实现使用视觉 path 做嵌入检查；运行时判定仍使用已有 real_path。原谱的多押发生时刻为 109.1611 s，修复后不再出现。

### 海神

```simai
(159){16}4,4,8,3,6,1,5-8[8:1],4,7,2,5V74[32:7],,,4,
```

参考实现中 `5V74` 的判定序列含 `B6/A6` 任选区，触到 B6 即可推进，不要求一定接触 A6。实际链路为：`5-8` 处理 A5、B6/A6，跳过 A7，随后再次处理 B6、跳过 B5；后面的 tap 4 完成 A4，提前清除 `5V74`。

参考实现分别使用 0、15、30、60 px 手部半径运行，均报相同的内屏无理；MiaCode 的最小回归用例也覆盖这四档。保留原谱 75.8000 s 的报告（提前约 419.2 ms，Perfect 容忍约 241.2 ms），不因“可能避开 A6”而删除或降级。

参考资料：[`SlideBasicData` 与折线判定区](https://github.com/Minepig/MaiMuriDX/blob/0ebab7ff6853a159ef6af76d9e3c43b23157bb5e/slides.py)、[开发笔记：正规手法、邻区接触与保守检测](https://github.com/Minepig/MaiMuriDX/blob/0ebab7ff6853a159ef6af76d9e3c43b23157bb5e/docs/%E5%BC%80%E5%8F%91%E7%AC%94%E8%AE%B0.md)。结论限定于参考实现的检测规则，不表示所有实际手法都会蹭掉。

### いとをかし

参考实现将不超过一帧（`3 / 180 s`）的普通 hold 先转成 tap，再识别其是否为 slide 启动动作。见 [`FAKE_HOLD_DURATION`](https://github.com/Minepig/MaiMuriDX/blob/0ebab7ff6853a159ef6af76d9e3c43b23157bb5e/core.py#L36) 和 [hold 解析](https://github.com/Minepig/MaiMuriDX/blob/0ebab7ff6853a159ef6af76d9e3c43b23157bb5e/majparse.py#L296-L307)。

110 BPM 的 `h[384:1]` 只有约 5.68 ms。其起点与同 lane 的 slide 启动重合，应复用 slide 手部动作。原实现保留其 hold 身份并额外加抬手延迟，于是在 slide 已经启动时又算入独立 hold 手。

本次只归一化无理检测的手部语义：原谱的 hold 类型、时长和 marker key 保持不变，预览手部叠层与运行时采用一致占用。修复原谱 97.9500 s 和 100.1333 s 的两条多押；原有一条静态撞尾参考保留。长于一帧的 hold、没有对应启动 slide 的独立短 hold 仍参与多押。

## 目录回归与已有版本差异

27 份输入中，本次修改相对修复前只有上表两首的三条多押消失；所有解析 marker、全部静态参考以及其余运行时诊断逐项一致。

稳定版之间和稳定版到当前代码另有以下已有差异：

| 输入 | 差异 | 代码依据 / 核查 |
| --- | --- | --- |
| himmelsflügel | 1.0.0 无运行时报告；1.2.0 起在 85.2 s 有一条多押 Warning | `700651d8` 引入同起点、重合 slide 与独立 press 的提示；这是 MiaCode 明确加入的保守提示，参考实现本身不报此条。本次保持原行为。 |
| micro.wav | 1.0.0 / 1.2.0 无运行时报告；修复前已有两条慢 slide 占区 Overlap，面板合并为一条 | `9675bc86` 加入晚到目标的 slide 占区来源回查。参考实现也能报告这两个被慢 slide 挡住的 `2h` 为 Overlap；保留。 |
| DATAERROR | 1.2.0 到当前有两个 marker 的 `head_each` 从 true 变成 false | 其余 marker 字段及无理诊断、静态参考均一致；属于既有头星 each 归组差异，本次没有改变。 |

因此不能把所有稳定版差异都当作回归，也没有据此回滚已有规则。此轮核查范围为上述实际输入和涉及的检测契约，不声称已经证明任意谱面的版本行为完全等价。

## 验证

- 先新增回归断言，原代码出现 7 个预期失败；修复后 `muri_spec` 全部通过。
- 覆盖重合段结束、短 hold 的 1 帧边界（16.7 ms / 17 ms）、独立短 hold 的真实三押、手部叠层，以及海神四档半径。
- CTest：`muri_spec`、`simai_parser_spec`、`timeline_marker_offset_spec`、`preview_sfx_timeline_spec`，4/4 通过。
- 主程序 `MiaCode` 的 Release 构建通过；链接器仍提示忽略重复静态库。
- 27 份主谱面四路诊断对照；两份既有语法错误单独注明。

复测本地谱面的命令：

```sh
cmake --build build-macos --config Release --target muri_spec miacode_muri_dump --parallel 4
ctest --test-dir build-macos -C Release -R '^(muri_spec|simai_parser_spec|timeline_marker_offset_spec|preview_sfx_timeline_spec)$' --output-on-failure
build-macos/miacode_muri_dump --maidata '/Users/caoyusen/Desktop/Chart/海神/maidata.txt' --difficulty MAS --with-markers
```
