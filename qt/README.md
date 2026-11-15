# fbterm-qt

Qt 5 / QTermWidget 版终端，使用 Qt 的 `linuxfb` 平台插件直接输出 `/dev/fb0`。
在目标机上原生编译（Qt 5.12.8 armhf）。

## 依赖

```sh
apt-get install -y g++ qtbase5-dev qtbase5-dev-tools \
    libqtermwidget5-0-dev libutf8proc-dev
```

## 构建

```sh
cd qt
./build.sh
```

产物：`../bin/fbterm-qt`。

## 运行

```sh
cd /root/fbterm-qt
./bin/fbterm-qt
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

- `Ctrl+Alt+Q` 退出
- `Ctrl+滚轮` 字号 ±2px

## 开机自启动

仓库根目录的 systemd unit 和安装脚本：

```sh
cd /root/fbterm-qt
./scripts/install-service.sh
systemctl status fbterm-qt.service
journalctl -u fbterm-qt.service -f
```

`install-service.sh` 会禁用 `getty@tty1`，避免和 framebuffer 终端抢 VT。
`Ctrl+Alt+Q` 正常退出后服务不自动重启（`Restart=on-failure`）。

## 输入热插拔

- 无键盘：每 **500ms** 扫描 `/dev/input/event*`；找到后停止
- 鼠标：每 **10s** 扫描一次
- 键盘拔掉：`read()` 返回 `ENODEV` → 关闭并重新进入 500ms 扫描
- 所有设备 `EVIOCGRAB`，避免 tty1/fbcon 重复收键
- 按键合成 `QKeyEvent` 给 `TerminalDisplay`
- Backspace 走自定义 `share/keytab/fbterm-qt.keytab`，发 `0x7f`，不会出现 `^H`
- F1-F12 使用 xterm 序列（F2=`ESC O Q`），不会再把 F2 的尾部 `B` 当字符

## 说明

- 显示：Qt `linuxfb`，启动时 `KD_GRAPHICS`，退出恢复 `KD_TEXT`
- 终端：QTermWidget 自带 VT 解析和 PTY；**不使用 libvterm**
- 字体：QFontDatabase 加载 Unifont，经 Qt/FreeType 渲染
- 配色：`share/color-schemes/Campbell.colorscheme`（Windows Terminal Campbell，深色 `#0C0C0C`）
- 已知：QTermWidget 会提示 Unifont variable-width；Ctrl+滚轮优先于程序鼠标上报
- 已知：中文输入法、复制粘贴、systemd 自启未做
