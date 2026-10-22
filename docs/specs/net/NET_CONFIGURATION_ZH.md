# Net 应用配置

桌面应用从用户配置目录中与 `preferences_v2.json` 同级的 `net_config.json` 读取 Net 设置。路径复用 `PreferenceDocument::preferencesFilePath()` 所在目录，随平台的用户配置目录解析。配置按进程启动时的内容读取，手动编辑后重启应用生效。

在线谱面下载默认开放，无需账号或配置文件。`net_config` 为 JSON 布尔值，仅控制上传入口：`true` 显示上传菜单并允许打开上传标签页；`false`、缺省或配置文件无效时隐藏上传入口，下载仍可使用。

用户创建配置文件，程序仅更新存在且格式有效的文件；文件缺省时，下载输出目录使用桌面路径，桌面路径不可用时使用用户主目录，不自动创建配置文件。

```json
{
  "net_config": true,
  "last_net_batch_output_dir": null
}
```

| 字段 | 类型 | 内容 |
| --- | --- | --- |
| `net_config` | boolean / null | 桌面 Net 上传入口开关，缺省为关闭；不影响下载 |
| `last_net_batch_output_dir` | string / null | 批量下载输出目录 |

配置文件存在时，程序迁移 `preferences_v2.json` 中的 Net 设置，保留原字段名。目标文件中的显式值优先，包括 `null` 和 `false`。目标写入成功后清除偏好中的对应字段。
