# fbterm-qt 设计文档（Qt 版）

> Qt5 + QTermWidget，直接使用 `/dev/fb0` 的本地伪终端。
> 目标机：Hi3798MV100 / armv7l / Ubuntu 20.04 / hifb 1920×1080。

## 1. 目标

在帧缓冲设备上提供全屏、可交互的本地终端：

- 画面：Qt5 `linuxfb` 输出到 hifb
- 终端：QTermWidget 的 VT 解析 + PTY
- 字体：Unifont OTF，经 QFont/FreeType 渲染
- 输入：USB 键盘 + 鼠标滚轮，支持热插拔
- 默认 `/bin/login`，注销后自动重开

## 2. 为什么从 LVGL 迁到 Qt

LVGL 版在 hifb 上遇到 mmap 区域不是实际扫描输出的问题：
`cat /dev/random > /dev/fb0` 能显示，LVGL mmap 写完却黑屏。
设备上的 qterm 证明 Qt `linuxfb` 路径可用，最小 Qt 红底窗口也验证通过。
因此改用 Qt，删掉了 LVGL 依赖和 LVGL 版代码。

## 3. 架构

```
/dev/input/event* ──► EvdevInput (QSocketNotifier)
                      │  500ms 无键盘扫描 / 10s 鼠标扫描
                      │  EVIOCGRAB
                      ▼
                 QKeyEvent / QWheelEvent
                      ▼
                 TerminalWindow (QTermWidget)
                      │  PTY master
                      ▼
                 /bin/login ↔ shell
                      │
                      ▼
                 QTermWidget 渲染
                      ▼
                 QWidget → Qt linuxfb → /dev/fb0
```

### 3.1 显示

- `QT_QPA_PLATFORM=linuxfb`
- `QT_QPA_GENERIC_PLUGINS` 置空，禁用 Qt 自带的 evdev 插件
- `QT_QPA_ENABLE_TERMINAL_KEYBOARD=0`，禁用 stdin/tty 键盘
- 启动时 `KDSETMODE(KD_GRAPHICS)`，退出 `KD_TEXT`
- `showFullScreen()` + 隐藏鼠标指针

### 3.2 输入

`qt/evdevinput.cpp`：

- 设备扫描：`/dev/input/event0..63`
  - 键盘：`EV_KEY` 且具备 `KEY_A`、`KEY_ENTER`、`KEY_SPACE`
  - 鼠标：`EV_REL` 且具备 `REL_WHEEL`
- 无键盘时 500ms 定时扫描；找到后停止
- 鼠标 10s 定时扫描
- 每个 fd 一个 `QSocketNotifier(Read)`
- `EVIOCGRAB` 独占；拔掉时 read 返回 `ENODEV`，移除并恢复扫描定时器
- 中文输入法：`qt/ime.*` 包 libpinyinime；字母先入 IME，空格/数字选词；
  选词后用 `pinyin_ime_get_segments()` 保留剩余音节（`zhendema` → `zhen'de'ma`，
  选“真”后剩 `de'ma`），不重置已选结果；候选每页 9 个，PageUp/PageDown 翻页
- Shift 单击切换中英，右下角 1.2 秒 toast 显示“中”/“EN”，不画常驻图标；
  中文模式标点转全角
- 按键映射：
  - US QWERTY；Shift/Ctrl/Alt/Caps
  - 字母、数字、符号、方向键、F1-F12、Home/End/PgUp/PgDn、Insert/Delete
  - 合成 `QKeyEvent` 发给当前焦点控件（`Konsole::TerminalDisplay`）
- 滚轮合成 `QWheelEvent`，交给 QTermWidget 做回滚/程序鼠标上报
- Backspace 使用自定义 `share/keytab/fbterm-qt.keytab` → `0x7f`
- F1-F12 使用 xterm 序列（`linux.keytab` 的 F1-F5 是 `ESC[[A..E`，不兼容
  `TERM=xterm-256color`，会导致 F2 尾部 `B` 被当字符）

### 3.3 终端

QTermWidget 负责：

- PTY 创建、`fork/exec`、窗口大小、`SIGWINCH`
- VT/ANSI、备用屏幕、颜色、光标、滚动、选区、程序鼠标上报
- `/bin/login` 或 shell 退出后 `finished()` → `QCoreApplication::quit()`
  （不调用 `clear()`、不重启 login；避免退出时白屏）

### 3.4 配色

使用 Windows Terminal 的 **Campbell** 配色（背景 `#0C0C0C`，前景 `#CCCCCC`，
标准 Campbell 16 色）。文件在 `share/color-schemes/Campbell.colorscheme`，
启动时按完整路径 `setColorScheme()`；不是蓝底的 Campbell PowerShell。

### 3.5 字体

- 默认字体只找 `font.ttf` / `font.otf`（当前目录、`share/`、安装目录）；
  找不到使用 Qt 默认字体，其它字体需显式 `--font-path`
- `QFontDatabase::addApplicationFont(...)`
- `QFont` pixel size 可配（默认 32）；16 的倍数关闭抗锯齿，其余开启
- QTermWidget 会提示 Unifont 是 variable-width（dual-width 正常现象），不影响实测对齐

### 3.5 快捷键

- 全局事件过滤器：`Ctrl+Alt+Q` 退出，`Ctrl+滚轮` 字号 ±2
- 因为焦点在 QTermWidget 内部 `TerminalDisplay`，快捷键放在
  `QApplication::installEventFilter` 而不是父类 `keyPressEvent`

## 4. systemd 开机自启

`systemd/fbterm-qt.service`：

- `WorkingDirectory=/root/fbterm-qt`
- `ExecStart=/root/fbterm-qt/bin/fbterm-qt`
- `Environment=QT_QPA_PLATFORM=linuxfb`
- `Environment=QT_QPA_GENERIC_PLUGINS=none`（不加载 Qt 自带 evdev 插件）
- `Environment=QT_QPA_ENABLE_TERMINAL_KEYBOARD=0`
- `RuntimeDirectory=fbterm-qt` + `XDG_RUNTIME_DIR=/run/fbterm-qt`
- `Conflicts=getty@tty1.service`
- `Restart=on-failure`（`Ctrl+Alt+Q` 正常退出不重启）
- `KillSignal=SIGTERM`，`TimeoutStopSec=5`

`qt/signalbridge.*` 把 `SIGTERM/SIGINT/SIGHUP` 通过 pipe + `QSocketNotifier`
转成 Qt 退出事件，保证 `ConsoleMode` 析构时恢复 `KDTEXT`。
`scripts/install-service.sh` 负责安装 unit、禁用 getty@tty1、enable + start。

## 5. 构建与部署

- 目标机原生构建：`qt/build.sh`（qmake + make）
- 依赖：`g++ qtbase5-dev qtbase5-dev-tools libqtermwidget5-0-dev`
- 主机：`scripts/deploy.sh` 把 `qt/` 打包到设备并在设备上构建
- 产物：`/root/fbterm-qt/bin/fbterm-qt`

## 5. 目录

```
qt/           Qt 主程序
docs/         实机截图
scripts/      部署
tools/        fb_probe / uinput_type / fb_ffmpeg_bridge
```

## 6. 限制

- 不使用 libvterm；QTermWidget 自带终端模拟器
- Ctrl+滚轮缩放优先于程序鼠标上报
- 中文输入法、复制粘贴、systemd 自启未做
- 鼠标滚轮热插拔逻辑已实现，实机鼠标待验证
