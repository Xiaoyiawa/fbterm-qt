# lvfbterm

在 Linux 帧缓冲 `/dev/fb0` 上运行的本地伪终端。
其实只是为自己的机顶盒写的
因为我不想每次都要ssh进去才能操作...
纯vibe coding，勿喷QAQ

基于 **Qt 5 + QTermWidget**，使用 Qt 的 `linuxfb` 平台插件直接输出到帧缓冲设备

- 显示：Qt5 `linuxfb` → `/dev/fb0`
- 终端：QTermWidget（VT 解析、PTY、滚动、选区、鼠标上报）
- 字体：QFontDatabase + Unifont OTF（Qt 底层 FreeType）
- 配色：Windows Terminal **Campbell**（深色 `#0C0C0C`，非 Campbell PowerShell 蓝底）
- 输入：自管 evdev，`QSocketNotifier` + `EVIOCGRAB`，支持热插拔
- 会话：`/bin/login`（默认）；登录 shell 里 `exit` 或注销后**直接退出程序**
- 语言：C++11，qmake 构建

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

## 开机自启动

设备上已安装 systemd 服务：

```sh
systemctl enable lvfbterm.service
systemctl start lvfbterm.service
```

仓库里的安装脚本（会禁用 `getty@tty1`，避免和 framebuffer 终端抢 VT）：

```sh
cd /root/lvfbterm
./scripts/install-service.sh
```

查看：

```sh
systemctl status lvfbterm.service
journalctl -u lvfbterm.service -f
```

停止/禁用：

```sh
systemctl stop lvfbterm.service
systemctl disable --now lvfbterm.service
```

`Ctrl+Alt+Q` 正常退出后服务不会自动重启（`Restart=on-failure`）；
需要继续运行用 `systemctl start lvfbterm.service`。如果希望退出后也自动拉起，
把 `Restart=on-failure` 改成 `Restart=always`。

## 输入热插拔

输入层不依赖 Qt 的 evdev 通用插件，而是自己读 `/dev/input/event*`：

- **没有键盘时每 500ms 扫描一次**；找到键盘后停止扫描
- **鼠标每 10s 扫描一次**
- 键盘拔掉时 `read()` 返回 `ENODEV` → 关闭 fd、删 `QSocketNotifier`；
  如果没有键盘了，重新进入 500ms 扫描
- 每个设备都 `EVIOCGRAB`，避免 tty1/fbcon 再收一份键
- 按键合成 `QKeyEvent` 发给 `TerminalDisplay`
- 滚轮合成 `QWheelEvent`


## 显示

- 启动时对 `/dev/tty0` 执行 `KDSETMODE(KD_GRAPHICS)`，阻止 tty1 fbcon 叠加；
  退出时恢复 `KD_TEXT`
- 不在启动时探测 Qt 通用 evdev 插件，避免和 tty1/SSH stdin 形成第二路输入
- hifb 的 mmap 不参与扫描输出；Qt linuxfb 走自己的刷新路径，已实测可显示

## 已知限制

- QTermWidget 会提示 Unifont 是 variable-width（dual-width 的正常现象），
  实测登录/MOTD/中文对齐正常。
- 暂不支持中文输入法、复制粘贴、systemd 自启。
- 鼠标热插拔逻辑已实现；当前设备没有鼠标，滚轮/缩放尚未实机验证。
