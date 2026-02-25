# LumiOS UI Design Specification / UI 设计规范

## 1. Design Philosophy / 设计哲学

LumiOS 追求**简洁、流畅、现代**的视觉语言。

- **Clarity / 清晰**: 内容优先，界面元素服务于信息传达
- **Depth / 层次**: 通过模糊、透明、阴影建立空间层次感
- **Fluidity / 流畅**: 所有交互都有物理感的动画响应
- **Consistency / 一致**: 统一的设计语言贯穿整个系统

## 2. Liquid Glass Material / 液态玻璃材质

### 2.1 Core Properties / 核心属性

液态玻璃 (Liquid Glass) 是 LumiOS 的核心视觉材质:

- **Gaussian Blur / 高斯模糊**: radius 20-40px
- **Tint / 着色**:
  - Light: `rgba(255, 255, 255, 0.72)`
  - Dark: `rgba(28, 28, 30, 0.78)`
- **Saturation boost / 饱和度增强**: 1.8x
- **Inner border / 内边框**: 1px `rgba(255,255,255,0.18)`
- **Shadow / 阴影**: `0 8px 32px rgba(0,0,0,0.12)`, `0 2px 8px rgba(0,0,0,0.08)`
- **Corner radius / 圆角**: 16-24px
- **Refraction / 折射**: 背后内容偏移 1-2px (基于陀螺仪)

### 2.2 Glass Hierarchy / 玻璃层次

| Layer / 层 | Blur | Opacity | Usage / 用途 |
|---|---|---|---|
| Ultra-thin / 超薄 | 12px | 0.45 | 状态栏, 导航栏 |
| Thin / 薄 | 20px | 0.60 | 通知中心, 快捷设置 |
| Regular / 常规 | 30px | 0.72 | 卡片, 对话框 |
| Thick / 厚 | 40px | 0.85 | 模态面板 |
| Chromatic / 色彩 | 30px | 0.65 | 强调色着色玻璃 |

### 2.3 Dynamic Tinting / 动态着色

- 提取背后内容主色调 (dominant color extraction)
- 主色调以 10-15% 不透明度叠加到玻璃表面
- 创造"融入环境"的视觉效果

## 3. Color System / 色彩系统

### 3.1 Accent Colors / 强调色

| Name | Hex | Usage / 用途 |
|---|---|---|
| Primary / 主色 | `#007AFF` | 链接、主按钮、选中态 |
| Secondary / 辅色 | `#5856D6` | 次要操作 |
| Success / 成功 | `#34C759` | 成功状态、开关 |
| Warning / 警告 | `#FF9500` | 警告信息 |
| Destructive / 危险 | `#FF3B30` | 删除、错误 |

### 3.2 Light Mode / 浅色模式

| Token | Value |
|---|---|
| Background | `#F2F2F7` |
| Surface | `#FFFFFF` |
| Text Primary | `rgba(0,0,0, 0.87)` |
| Text Secondary | `rgba(60,60,67, 0.60)` |
| Separator | `rgba(60,60,67, 0.12)` |

### 3.3 Dark Mode / 深色模式

| Token | Value |
|---|---|
| Background | `#000000` |
| Surface | `#1C1C1E` |
| Text Primary | `rgba(255,255,255, 0.87)` |
| Text Secondary | `rgba(235,235,245, 0.60)` |
| Separator | `rgba(84,84,88, 0.36)` |

## 4. Typography / 字体排版

Font stack: `"LumiOS Sans"`, `"Inter"`, `"Noto Sans SC"`, `"Noto Sans"`

| Style | Size | Weight | Line Height |
|---|---|---|---|
| Display Large | 34pt | 700 | 41pt |
| Display Medium | 28pt | 700 | 34pt |
| Title Large | 22pt | 700 | 28pt |
| Title Medium | 17pt | 600 | 22pt |
| Body Large | 17pt | 400 | 22pt |
| Body Medium | 15pt | 400 | 20pt |
| Body Small | 13pt | 400 | 18pt |
| Caption | 12pt | 400 | 16pt |

## 5. Animation & Motion / 动画与动效

### 5.1 Spring Physics / 弹簧物理

所有动画基于弹簧物理模型:

```c
struct spring_config {
    float stiffness;  // 刚度
    float damping;    // 阻尼
    float mass;       // 质量
};

#define SPRING_RESPONSIVE  { 400, 28, 1.0 }  // 快速响应
#define SPRING_GENTLE      { 200, 20, 1.0 }  // 柔和
#define SPRING_BOUNCY      { 300, 15, 1.0 }  // 弹性
#define SPRING_STIFF       { 500, 30, 1.0 }  // 刚硬
```

### 5.2 Transition Timing / 过渡时间

| Transition / 过渡 | Duration | Type |
|---|---|---|
| Tap feedback | 50ms | Scale 0.97 + opacity |
| Page push / 推入 | 350ms | Spring responsive |
| Page pop / 弹出 | 300ms | Spring responsive |
| Sheet present | 400ms | Spring gentle |
| Sheet dismiss | 300ms | Spring stiff |
| Notification | 400ms | Spring bouncy |
| App launch | 450ms | Spring + scale + blur |
| App close | 350ms | Spring + scale + blur |

### 5.3 Blur Transitions / 模糊过渡

打开覆盖层时:
1. 背景高斯模糊 0→30px (350ms spring)
2. 背景缩小 scale 1.0→0.95
3. 背景暗化 overlay 0→0.3
4. 前景从下方弹出 (spring bouncy)

## 6. Components / 组件

### 6.1 Glass Card / 玻璃卡片
- 圆角 16px, 模糊 Regular(30px), 内边距 16px, 间距 12px

### 6.2 Navigation Bar / 导航栏
- 高度 48px, 模糊 Ultra-thin, 底部安全区 34px
- 图标 24x24, 线条风格 2px 线宽

### 6.3 Status Bar / 状态栏
- 高度 44px (含安全区), 模糊 Ultra-thin
- 左: 时间 / 右: 信号 WiFi 电池

### 6.4 Notification Card / 通知卡片
- 圆角 20px, 模糊 Thin(20px), 头像 36px 圆形
- 标题 Title Small, 内容 Body Small max 2 lines

### 6.5 Quick Settings / 快捷设置
- 4 列网格, 每 tile 玻璃卡片 圆角 12px
- 切换动画: scale + color (200ms)

### 6.6 App Icon / 应用图标
- 60x60pt, squircle 形状, 圆角 ~13.4px
- 阴影 `0 4px 12px rgba(0,0,0,0.15)`

## 7. Gestures / 手势

| Gesture / 手势 | Action / 操作 |
|---|---|
| 底部上滑 | 返回主屏幕 |
| 底部上滑暂停 | 多任务 |
| 底部左右滑 | 切换最近应用 |
| 左边缘右滑 | 返回 |
| 顶部下滑 | 通知中心 |
| 顶部右侧下滑 | 控制中心 |
| 长按 | 上下文菜单 (haptic) |

## 8. GPU Effects / GPU 加速特效

### 8.1 Kawase Blur / 川�的模糊
- 多 pass 下采样+上采样, 4 pass ≈ 30px 高斯模糊
- GPU shader 实现, <1ms @1080p

### 8.2 Vibrancy / 鲜明度
- 饱和度 boost 1.8x + 轻微对比度增强
- 文字在玻璃上保持高可读性

### 8.3 Parallax Depth / 视差深度
- 陀螺仪驱动: 壁纸 ±20px, 图标 ±8px
- 创造"窗口中的世界"深度感

### 8.4 Ambient Reflection / 环境光反射
- 玻璃表面高光随设备倾斜移动
- 细微 specular 光斑增强真实感

## 9. Accessibility / 无障碍

- **Reduce Motion**: 关闭 spring 动画, 使用简单淡入淡出
- **Reduce Transparency**: 关闭模糊, 使用实色背景
- **Increase Contrast**: 加深分隔线和边框
- **Bold Text**: 所有 Regular 权重提升为 Medium
- **Dynamic Type**: 支持 0.8x-1.4x 字体缩放
- **Screen Reader**: 完整 AT-SPI2 无障碍树
