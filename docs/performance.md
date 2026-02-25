# LumiOS Performance Optimization Design / 性能优化设计

## 1. Design Goals / 设计目标

针对当前 Android 系统的普遍性能问题，LumiOS 从底层架构到 UI 渲染全链路优化。

| Metric / 指标 | Target / 目标 | Android Typical / 安卓典型值 |
|---|---|---|
| Cold boot to lock screen / 冷启动到锁屏 | < 3s | 15-30s |
| App launch (cold) / 应用冷启动 | < 300ms | 800-2000ms |
| Touch-to-display latency / 触摸到显示延迟 | < 8ms | 20-50ms |
| UI frame budget / UI 帧预算 | < 4ms @120Hz | 8-12ms |
| Idle RAM / 空闲内存占用 | < 256MB | 2-4GB |
| Idle CPU / 空闲 CPU 使用 | < 1% | 3-8% |
| Battery idle drain / 待机耗电 | < 0.5%/h | 1-3%/h |
| Storage I/O latency / 存储 IO 延迟 | < 1ms (random 4K) | 5-20ms |

## 2. Android Pain Points & LumiOS Solutions / 安卓痛点与解决方案

### 2.1 GC Stutter / GC 卡顿

**Android**: ART GC 导致 10-50ms stop-the-world 暂停，造成掉帧。

**LumiOS**: 核心组件全部 C/C++，零 GC。UI 渲染管线原生代码，不依赖 VM。
Android 兼容层容器化隔离，GC 不影响系统 UI。compositor/shell 使用 SCHED_FIFO 实时调度。

### 2.2 Binder IPC Overhead / Binder 开销

**Android**: 一次操作触发数十次 Binder 调用，每次 50-200μs 上下文切换。

**LumiOS**: Unix domain socket + 共享内存 IPC (< 5μs)。compositor↔shell 通过 Wayland 零拷贝。
关键热路径使用 io_uring 异步 I/O。Binder 仅限兼容层容器内。

### 2.3 Over-layered Stack / 过度分层

**Android**: App→Framework→SystemServer→HAL→Kernel (6+ 层)。

**LumiOS**: App→lumi-toolkit→compositor→DRM/KMS→Display (3 层)。
HAL 直接使用 Linux 内核接口 (evdev/DRM/ALSA/V4L2)，无 System Server。

### 2.4 Background Abuse / 后台滥用

**Android**: Service/BroadcastReceiver/JobScheduler 持续消耗资源。

**LumiOS**:
- cgroup v2 强制: 后台 CPU weight=10, 内存≤50MB, I/O≤1MB/s
- 冻结机制: 后台 >30s 应用通过 cgroup freezer 冻结
- 唤醒对齐: 后台定时器合并到 15 分钟窗口
- 无 BroadcastReceiver 滥用

### 2.5 Storage Degradation / 存储退化

**Android**: SQLite WAL 膨胀、F2FS 碎片化导致长期变慢。

**LumiOS**: 系统分区 erofs 只读压缩(零碎片)。数据分区 F2FS + discard TRIM。
包数据库用文件结构非 SQLite。定期 fstrim。日志用环形缓冲区。

### 2.6 Thermal Throttling / 降频卡顿

**Android**: CPU 过热降频后系统整体变卡。

**LumiOS**: 主动热管理(监控趋势非阈值)。UI 渲染永远在大核。
sched_setattr util_clamp: compositor uclamp_min=512, shell=256, 后台 uclamp_max=128。
高温时帧率平滑 120→90→60Hz。

### 2.7 Memory Pressure / 内存压力

**Android**: LMKD 激进杀后台，切回需冷启动。

**LumiOS**: ZRAM zstd 压缩(+50% 可用内存)。分级管理: 压缩→冻结→释放缓存→优雅终止。
应用状态快照，恢复 <100ms。基础系统仅需 256MB。

## 3. Kernel Optimizations / 内核优化

### 3.1 Scheduler / 调度器
- SCHED_AUTOGROUP: 自动分组减少交互延迟
- HZ=300: 3.33ms tick, 平衡延迟与开销
- NO_HZ_FULL: 空闲无 tick
- PREEMPT=y: 完全抢占式内核

### 3.2 Memory / 内存
- ZRAM + ZSMALLOC: 压缩内存 + 高效小对象分配
- TRANSPARENT_HUGEPAGE: 透明大页减少 TLB miss
- swappiness=60, dirty_ratio=10, vfs_cache_pressure=50

### 3.3 I/O
- BFQ 调度器: 优先前台 I/O
- BFQ_GROUP_IOSCHED: cgroup I/O 隔离
- read_ahead_kb=128: 适度预读

### 3.4 Network / 网络
- TCP BBR 拥塞控制 + FQ 队列
- TCP Fast Open + MTU 探测

## 4. Compositor Performance / 合成器性能

### 4.1 Rendering Pipeline / 渲染管线
- Triple buffering 防止争用卡顿
- Mailbox present mode 保持最新帧
- Direct scanout 全屏应用绕过 compositor
- Damage tracking 只重绘变化区域

### 4.2 Touch Optimization / 触控优化
- 触摸事件内核→compositor <2ms
- Touch boost: 触摸时提升 CPU/切换 120Hz
- 触摸预测: 基于速度加速度预测下一帧位置

## 5. Power Management / 电源管理

- **CPU idle states**: 深度 C-state 策略优化
- **GPU DVFS**: 按实际渲染负载动态调频
- **Display**: 低亮度时降低刷新率; LTPO 可变刷新
- **Modem**: 智能 DRX 周期; 合并网络请求
- **Wake locks**: 严格超时 + 自动释放, 防止应用阻止休眠
- **Suspend-to-RAM**: <500ms 进入, <200ms 恢复
