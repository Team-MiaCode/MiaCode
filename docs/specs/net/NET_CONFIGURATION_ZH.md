# Net 配置与持久化设置

## 应用根目录配置

`config.json` 位于应用文件根目录。Windows 发布包中与入口 `MiaCode.exe` 同级，macOS 发布包中与 `MiaCode.app` 同级；开发构建中与应用可执行文件同级。发布包附带该文件，应用启动时也会创建缺省文件，内容为 `{}`。上传入口的属性由用户手动填写。

沿用布尔字段 `net_config` 控制桌面上传入口：值为 `true` 时显示上传菜单并允许打开上传标签页。在线谱面下载默认开放。

配置按进程首次访问时的内容读取，手动编辑后重启应用生效。

## 用户持久化设置

`net_config.json` 位于用户配置目录，与 `preferences_v2.json` 同级。路径复用 `PreferenceDocument::preferencesFilePath()` 所在目录。程序自动创建该文件，保存 Net 页面产生的持久化设置。

| 字段 | 类型 | 内容 |
| --- | --- | --- |
| `last_net_batch_output_dir` | string / null | 批量下载输出目录，缺省使用桌面路径，桌面路径为空时使用用户主目录 |
| `last_net_batch_upload_dir` | string / null | 批量上传源目录 |
| `net_upload_remember_credentials` | boolean | 记住账号选项 |
| `net_upload_credentials` | object | 按调用主体区分的加密账号数据 |

Windows 端使用当前系统用户的 DPAPI 加密用户名与密码，JSON 中保存加密结果的 Base64 字符串。调用主体参与加密上下文校验；读取时解密并校验数据完整性。勾选记住账号且登录成功后保存，取消记住账号时清除对应的加密数据。

程序迁移 `preferences_v2.json` 中的 Net 持久化设置，目标文件中的显式值优先，包括 `null` 和 `false`。旧版明文账号字段（`net_upload_username`、`net_upload_password`、`net_upload_credential_ref`）不迁移，直接移除，用户需重新登录；Windows 凭据管理器中的旧条目不再读取。原上传开关从用户持久化设置中移除，入口由应用根目录配置管理。
