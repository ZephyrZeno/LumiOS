# LumiOS Security Architecture / 安全架构

## 1. Security Goals / 安全目标

对比 Android 常见安全问题，LumiOS 从架构层面加固:

| Area / 领域 | Android Issue / 安卓问题 | LumiOS Solution / 方案 |
|---|---|---|
| App isolation | 同 UID 共享数据 | 每应用独立 user + mount namespace |
| Root exploits | 内核漏洞提权 | seccomp + landlock + dm-verity |
| Supply chain | 恶意 SDK/广告 | 包签名验证 + 沙箱网络隔离 |
| Data at rest | 可绕过 FBE | LUKS2 全盘加密 + TPM 绑定 |
| IPC attack | Binder 漏洞面大 | Unix socket + 最小权限 |
| OTA tampering | 降级攻击 | A/B + rollback protection |

## 2. Defense in Depth / 纵深防御

```
Layer 5: Application sandbox (cgroup + namespace + seccomp)
         应用沙箱
Layer 4: Mandatory Access Control (AppArmor profiles)
         强制访问控制
Layer 3: System integrity (dm-verity + erofs read-only)
         系统完整性
Layer 2: Kernel hardening (KASLR + CFI + stack canary)
         内核加固
Layer 1: Secure boot (verified boot chain)
         安全启动
Layer 0: Hardware (TPM/TEE/Secure Element)
         硬件信任根
```

## 3. Verified Boot / 验证启动

- Bootloader 验证 boot.img 签名 (AVB 2.0 compatible)
- boot.img 包含内核 + initramfs + DTB
- initramfs 验证 system 分区 (dm-verity merkle tree)
- 任何篡改导致启动失败或进入恢复模式
- Rollback protection: 版本号单调递增，防止降级

## 4. System Partition Integrity / 系统分区完整性

- 系统分区使用 **erofs** 只读压缩文件系统
- **dm-verity** 块级完整性验证 (SHA-256 merkle tree)
- 运行时任何读取被篡改块会返回 I/O 错误
- 系统更新通过 A/B 分区切换，原子性操作

## 5. Application Sandbox / 应用沙箱

每个应用运行在独立沙箱中:

```
Per-app isolation / 每应用隔离:
├── UID: unique user per app / 独立用户
├── Mount namespace: private /data/<app> / 私有挂载
├── PID namespace: cannot see other processes / 不可见其他进程
├── Network namespace: per-app firewall rules / 独立防火墙
├── cgroup: CPU/memory/IO limits / 资源限制
├── seccomp-bpf: allowed syscall whitelist / 系统调用白名单
├── Landlock: filesystem access rules / 文件系统访问规则
└── AppArmor: mandatory access control profile / 强制访问控制
```

### 5.1 Seccomp Profiles / Seccomp 配置

三级 seccomp 策略:
- **Strict**: 仅允许 ~60 个基础系统调用 (普通应用)
- **Standard**: ~120 个系统调用 (需要网络/文件的应用)
- **Permissive**: ~200 个系统调用 (系统服务)

### 5.2 Landlock Filesystem Rules / Landlock 文件系统规则

```
Default app rules:
  READ:  /usr/share, /app/<name>/
  WRITE: /data/<app>/<name>/
  EXEC:  /app/<name>/bin/
  DENY:  everything else
```

## 6. Permission System / 权限系统

### 6.1 Runtime Permissions / 运行时权限

| Permission / 权限 | Description / 描述 |
|---|---|
| camera | 相机访问 |
| microphone | 麦克风访问 |
| location | 位置信息 (粗略/精确) |
| contacts | 联系人读写 |
| storage | 用户文件访问 |
| phone | 电话/短信 |
| bluetooth | 蓝牙设备扫描/连接 |
| notifications | 发送通知 |
| background | 后台运行 |
| network | 网络访问 (默认禁止!) |

### 6.2 Key Difference from Android / 与安卓关键区别

- **网络权限默认关闭**: 应用必须申请网络权限 (Android 默认允许)
- **无静默权限**: 所有权限需用户明确授予
- **一次性权限**: 相机/麦克风/位置支持"仅本次"授权
- **权限审计**: 系统记录所有权限使用日志，用户可查看

## 7. Encryption / 加密

### 7.1 Full Disk Encryption / 全盘加密
- 数据分区使用 LUKS2 + AES-256-XTS
- 密钥派生: Argon2id (memory-hard, 防暴力破解)
- 支持 TPM 2.0 绑定 (如硬件可用)

### 7.2 File-level Encryption / 文件级加密
- 每用户独立加密密钥
- 锁屏状态下凭据加密文件不可访问
- 设备加密文件始终可访问 (闹钟/来电等)

### 7.3 Network Security / 网络安全
- 系统默认仅允许 TLS 1.3+ 连接
- 证书透明度 (CT) 验证
- DNS-over-HTTPS 默认启用
- 应用 cleartext 通信默认禁止

## 8. Kernel Hardening / 内核加固

```
CONFIG_RANDOMIZE_BASE=y          # KASLR 地址随机化
CONFIG_CFI_CLANG=y               # 控制流完整性
CONFIG_STACKPROTECTOR_STRONG=y   # 栈保护
CONFIG_HARDENED_USERCOPY=y       # 用户空间拷贝检查
CONFIG_INIT_ON_ALLOC_DEFAULT_ON=y # 分配时清零
CONFIG_INIT_ON_FREE_DEFAULT_ON=y  # 释放时清零
CONFIG_SLAB_FREELIST_RANDOM=y    # SLAB 空闲列表随机化
CONFIG_SHUFFLE_PAGE_ALLOCATOR=y  # 页分配器随机化
CONFIG_STATIC_USERMODEHELPER=y   # 禁止动态 usermode helper
```

## 9. Package Security / 包安全

- 所有 .lmpk 包必须 GPG 签名
- 仓库索引数据库签名验证
- 包安装时 SHA-256 校验
- 自动安全更新推送
- CVE 漏洞自动扫描和通知

## 10. Privacy / 隐私

- **Tracker blocker**: 系统级广告追踪器阻断
- **Permission indicators**: 相机/麦克风使用时状态栏指示灯
- **Sensor access control**: 加速度计/陀螺仪等传感器也需权限
- **MAC randomization**: WiFi/BT MAC 地址默认随机化
- **Minimal telemetry**: 系统不收集用户数据，可选匿名崩溃报告
