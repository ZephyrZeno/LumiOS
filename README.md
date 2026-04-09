# LumiOS

**LumiOS 公共核心仓库：`lumid`、社区版 `lumi-shell`、核心系统组件与设计文档**

[English](README_EN.md) | 中文

LumiOS 公共仓库现在聚焦于可公开发布的核心部分，而不是完整私有系统镜像。这里包含：

- `lumid` 与 `lumictl`
- 社区版 `system/core/lumi-shell`
- 公共 `lumi-toolkit` / `lumi-render` / `lumi-compositor` 代码与文档
- 系统架构、构建和测试说明

完整的私有 Shell / compositor / render / toolkit 组合位于本地工作区的 `LumiShell/` 目录中，并与这里的社区版 shell 共享会话状态协议。

## 当前快照重点

- `lumid` 补充了配置与 socket 单元测试，控制面和守护进程边界更清晰。
- 社区版 `lumi-shell` 现在能够：
  - 从 `guest-manifest.json` 载入应用目录
  - 从共享 `storage.db` 读取系统会话状态
  - 可选消费 `lumid` 发布的 `system.state.updated` / `system.state.rejected` 事件
  - 在 `--stay-alive` 模式下结合 socket 轮询和目录变更监视增量刷新
- 文档已经按当前拆仓结构重写：包管理、SDK 与语言规范分别指向 `LumiPkg`、`LumiSDK`、`LumiScript`。

## 仓库范围

```text
LumiOS/
├── system/core/
│   ├── lumid/                init / service manager
│   ├── lumi-shell/           community shell entrypoint
│   ├── lumi-compositor/      public compositor code
│   ├── lumi-render/          render abstraction
│   └── lumi-toolkit/         shared UI utilities
├── docs/                     architecture / build / security / performance
└── README*.md
```

不再由本仓库直接维护的内容：

- `.lmpk` 工具链与包规范 → [LumiPkg](https://github.com/ZephyrZeno/LumiPkg)
- 应用 SDK 与 `liblumiapp` → [LumiSDK](https://github.com/ZephyrZeno/LumiSDK)
- LumiScript 语言与运行时 → [LumiScript](https://github.com/ZephyrZeno/LumiScript)

## 关键组件

### `lumid`

`lumid` 是 LumiOS 的 init / service manager，负责：

- 服务依赖排序与启动顺序
- 基础 cgroup / 进程监管
- Unix socket 控制接口
- `lumictl` 命令行控制工具

### 社区版 `lumi-shell`

公共仓库中的 `lumi-shell` 不是完整图形壳，而是一个可移植的社区入口，重点关注：

- 共享会话快照读取
- `lumid` 事件桥接
- 面板摘要生成
- 后台持续刷新与测试

它为私有完整 `LumiShell/` 提供可公开讨论和可自动化测试的会话模型。

## 构建与测试

### `lumid`

```bash
cd system/core/lumid
make
make test
```

### 社区版 `lumi-shell`

```bash
cd system/core/lumi-shell
meson setup build --prefix=/usr
meson compile -C build
meson test -C build
```

### 手动运行 shell 摘要

```bash
./system/core/lumi-shell/build/lumi-shell \
  --panel quick-settings \
  --battery 92 \
  --wifi on \
  --storage-path /tmp/lumi-session.db
```

### 持续监听模式

```bash
./system/core/lumi-shell/build/lumi-shell \
  --stay-alive \
  --storage-path /tmp/lumi-session.db \
  --lumid-socket /tmp/lumid.sock
```

更详细的步骤见 [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md)。

## 文档

| 文档 | 内容 |
|------|------|
| [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) | 当前仓库的构建、测试与 smoke test 指南 |
| [docs/architecture.md](docs/architecture.md) | 公共核心仓库的架构边界与共享状态模型 |
| [docs/performance.md](docs/performance.md) | 性能设计与目标 |
| [docs/security.md](docs/security.md) | 安全模型 |
| [docs/ui-design.md](docs/ui-design.md) | UI 与 liquid glass 方向说明 |
| [system/core/lumi-shell/README.md](system/core/lumi-shell/README.md) | 社区版 shell 的接口与参数 |

## 相关项目

| 项目 | 说明 |
|------|------|
| [LumiPkg](https://github.com/ZephyrZeno/LumiPkg) | `.lmpk` 包管理与打包工具 |
| [LumiSDK](https://github.com/ZephyrZeno/LumiSDK) | `liblumiapp`、语言绑定与 toolkit |
| [LumiScript](https://github.com/ZephyrZeno/LumiScript) | 编译器、字节码与运行时 |

## License

GPLv3（内核相关补丁遵循 GPLv2）
