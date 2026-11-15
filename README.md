# lvfbterm

在 Linux 帧缓冲 `/dev/fb0` 上运行的本地伪终端。

当前版本基于 **Qt 5 + QTermWidget**，使用 Qt 的 `linuxfb` 平台插件直接输出到
Hi3798MV100（hifb，1920×1080/32bpp）的 HDMI 画面。

- 显示：Qt5 `linuxfb` → `/dev/fb0`
- 终端：QTermWidget（VT 解析、PTY、滚动、选区、鼠标上报）
- 字体：QFontDatabase + Unifont OTF（Qt 底层 FreeType）
- 输入：自管 evdev，`QSocketNotifier` + `EVIOCGRAB`，支持热插拔
- 会话：`/bin/login`（默认），注销自动重开
- 语言：C++11，qmake 构建

![Qt 终端截图](docs/qt-screenshot.png)

## 目标设备

`root@192.168.1.15`：

- Hi3798MV100 / armv7l / Ubuntu 20.04 armhf
- `/dev/fb0`：`hifb`，1920×1080，32bpp
- Qt 5.12.8 + QTermWidget 0.14.1（已安装）

## 目录

```
.
├── qt/                     Qt/QTermWidget 主程序
│   ├── main.cpp            linuxfb、字体、启动 QT
│   ├── terminalwindow.*    QTermWidget 窗口/字体/快捷键
│   ├── evdevinput.*        键盘/鼠标热插拔输入
│   ├── consolemode.*       KD_GRAPHICS / KD_TEXT
│   └── lvfbterm-qt.pro
├── scripts/deploy.sh       复制源码到设备并在设备上构建
├── tools/                  fb_probe / uinput_type / fb_ffmpeg_bridge
├── docs/                   实机截图
├── assets/                 make 时解出的 unifont.otf（git 忽略）
└── bin/                    lvfbterm（git 忽略）
```

## 依赖

设备上：

```sh
apt-get install -y g++ qtbase5-dev qtbase5-dev-tools \
    libqtermwidget5-0-dev libutf8proc-dev
```

`unifont.otf` 需要放到 `share/unifont.otf`，或者用 `--font-path` 指定。

## 构建

在目标机上原生构建：

```sh
cd /root/lvfbterm/qt
./build.sh
```

产物：`../bin/lvfbterm`。

从主机部署并构建：

```sh
./scripts/deploy.sh
```

默认 `DEPLOY_HOST=root@192.168.1.15`、`DEPLOY_DIR=/root/lvfbterm`，
可覆盖。

## 运行

```sh
cd /root/lvfbterm
./bin/lvfbterm
```

参数：

```text
--font-path=PATH      Unifont OTF 路径
--font-size=N         8..96，默认 32
--shell=CMD           默认 /bin/login
--scrollback=N        默认 5000
--keyboard=PATH       指定键盘（默认自动扫描）
--mouse=PATH          指定鼠标（默认自动扫描）
```

快捷键：

- `Ctrl+Alt+Q` 退出，恢复文本控制台
- `Ctrl+滚轮` 字号 ±2px

## 输入热插拔

输入层不依赖 Qt 的 evdev 通用插件，而是自己读 `/dev/input/event*`：

- **没有键盘时每 500ms 扫描一次**；找到键盘后停止扫描
- **鼠标每 10s 扫描一次**
- 键盘拔掉时 `read()` 返回 `ENODEV` → 关闭 fd、删 `QSocketNotifier`；
  如果没有键盘了，重新进入 500ms 扫描
- 每个设备都 `EVIOCGRAB`，避免 tty1/fbcon 再收一份键
- 按键合成 `QKeyEvent` 发给 `TerminalDisplay`
- 滚轮合成 `QWheelEvent`

`Backspace` 使用 QTermWidget 的 `linux.keytab`，发送 `0x7f`（DEL），不会出现 `^H`。

已验证：程序先无键盘启动，2 秒后插入 uinput 键盘，500ms 内自动接管；
输入 `abc` + Backspace + `d` 显示 `abd`，见 [docs/qt-input-test.png](docs/qt-input-test.png)。

## 显示

- 启动时对 `/dev/tty0` 执行 `KDSETMODE(KD_GRAPHICS)`，阻止 tty1 fbcon 叠加；
  退出时恢复 `KD_TEXT`
- 不在启动时探测 Qt 通用 evdev 插件，避免和 tty1/SSH stdin 形成第二路输入
- hifb 的 mmap 不参与扫描输出；Qt linuxfb 走自己的刷新路径，已实测可显示

## 已知限制

- QTermWidget 使用自带 VT 解析，**不再使用 libvterm**。
  工作区里的 `freetype/`、`libvterm/` 是旧 LVGL 版遗留源码，Qt 版不链接。
- QTermWidget 会提示 Unifont 是 variable-width（dual-width 的正常现象），
  实测登录/MOTD/中文对齐正常。
- `Ctrl+滚轮` 缩放优先于程序鼠标上报（QTermWidget 未暴露终端 mouse mode）。
- 暂不支持中文输入法、复制粘贴、systemd 自启。
- 鼠标热插拔逻辑已实现；当前设备没有鼠标，滚轮/缩放尚未实机验证。
