---
owner: src/app/services/net
canonical_id: net.optional-configuration
lifecycle: working
last_verified: 2026-10-10
---

# Net 应用配置

桌面应用从用户配置目录中与 `preferences.json` 同级的 `net_config.json` 读取 Net 设置。路径复用 `PreferenceDocument::preferencesFilePath()` 所在目录，随平台的用户配置目录解析。配置按进程启动时的内容读取，手动编辑后重启应用生效。

本机 Windows 路径：

```text
C:/Users/kanago/AppData/Local/MiaCode/
├─ preferences.json
└─ net_config.json
```

`net_config` 为 JSON 布尔值，`true` 启用在线谱面功能入口，允许打开对应标签页；`false` 关闭这些入口。字段缺省按 `null` / `false` 读取。用户创建配置文件，程序仅更新存在且格式有效的文件；文件缺省时使用空路径及关闭状态。

```json
{
  "net_config": true,
  "last_net_batch_output_dir": null
}
```

| 字段 | 类型 | 内容 |
| --- | --- | --- |
| `net_config` | boolean / null | 桌面 Net 入口开关，缺省为关闭 |
| `last_net_batch_output_dir` | string / null | 批量下载输出目录 |

配置文件存在时，程序迁移 `preferences.json` 中的 Net 设置，保留原字段名。目标文件中的显式值优先，包括 `null` 和 `false`。目标写入成功后清除偏好中的对应字段。配置验收由 `net_configuration_spec` 覆盖用户偏好同级路径、文件缺省、删除、格式错误、字段迁移及迁移失败时保留来源。
