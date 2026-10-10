---
lifecycle: working
owner: src/app/services/net
canonical_id: net.migration-target
last_verified: 2026-10-10
---

# Net 下载与在线预览迁移目标

本阶段交付谱面下载和在线预览，接入工具箱与工具菜单，使用独立标签页。功能范围依据迁移指导文件与 v1 源码，用户提供的图示确定页面布局方向；实现复用当前 v2 的主题、控件、任务、文档、音频与舞台服务。

## 功能与页面

| 页面或服务 | 目标行为 | 验收依据 |
| --- | --- | --- |
| 下载 | 条件查询、日期范围、排序、目录记忆、连接检测、全选、音频/封面/谱面/PV、ZIP、取消与进度日志 | 用户下载图示及 v1 下载源码 |
| 在线预览 | 缓存准备、独立打开、文档身份检查、来源标记、舞台播放、编辑及另存为 | 预览迁移契约与 v2 文档服务 |
| 工具箱 | “谱面下载”入口，重复打开复用标签页 | 用户入口规范 |

下载页的详细目标见 [下载页面目标](NET_DOWNLOAD_PAGE_TARGET_ZH.md)。公开调用契约见 [API 规范](NET_API_CONTRACT_ZH.md) 与 [操作目录](generated/NET_OPERATION_CATALOG_ZH.md)。

桌面 HEAD、功能清单对应与验证依据见 [桌面实现与验证记录](NET_DESKTOP_MIGRATION_VERIFICATION_ZH.md)。

## 验证范围


下载、资源发布、PV 与 ZIP、缓存、取消和文档来源使用有容量约束的本地 HTTP 固定样本验证；测试输出归属 `build-devtools`，随测试目录生命周期清理。

## 来源

桌面 v2 的源码与自身文档决定实现状态。v1 对照提交为 `6573efb611056bccd046dd17bc2f4cceda78bf88`；MiaCode Mobile 的验收和同步按其独立项目顺序执行。
