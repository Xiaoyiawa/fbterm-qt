# lvfbterm-qt

Qt 5 / QTermWidget 版终端，使用 Qt 的 `linuxfb` 平台插件直接输出 `/dev/fb0`。
这个版本是在目标机上原生编译的（Qt 5.12.8 armhf）。

## 依赖

```sh
apt-get install -y g++ qtbase5-dev qtbase5-dev-tools libqtermwidget5-0-dev libutf8proc-dev
```

## 构建

```sh
cd qt
./build.sh
```

产物会覆盖 `../bin/lvfbterm`（旧 LVGL 版会被部署脚本备份为 `lvfbterm-lvgl`）。

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
--keyboard=PATH       默认自动扫描 /dev/input/event*
--mouse=PATH          默认自动扫描
```

快捷键：

- `Ctrl+Alt+Q` 退出
- `Ctrl+滚轮` 字号 ±2px

## 说明

- 显示：Qt `linuxfb` 插件，已验证能走 hifb 上屏（qterm 同一条路径）。
- 终端：QTermWidget 自带 VT 解析和 PTY；**没有使用 libvterm**。
- 字体：QFontDatabase 加载 Unifont，经 Qt/FreeType 渲染；Unifont 是 dual-width，QTermWidget 会提示 variable-width，但实测登录/MOTD/中文对齐正常。
- 键盘：Qt evdevkeyboard 通用插件，程序启动前自动找 `/dev/input/event*` 并设置 `QT_QPA_GENERIC_PLUGINS`。
- 程序启动时切 `KDSETMODE(KD_GRAPHICS)`，阻止 tty1 的 fbcon 和键盘输入叠加；
  同时设置 `QT_QPA_ENABLE_TERMINAL_KEYBOARD=0`，不再把 SSH/tty1 的 stdin 当第二路键盘。
- 键位表使用 `linux.keytab`，Backspace 发送 `0x7f`（DEL），不会出现 `^H`。
- Ctrl+滚轮缩放会优先于程序鼠标上报（QTermWidget 没暴露终端 mouse mode），这是当前限制。

## 键盘问题排查

- **不需要启动前插键盘**：无键盘时每 500ms 扫描一次 `/dev/input/event*`，
  找到就停止扫描；键盘拔掉（read 返回 ENODEV）会自动重新进入 500ms 扫描。
- 鼠标每 10s 扫描一次；滚轮事件合成 `QWheelEvent` 给 QTermWidget。
- 所有设备都 `EVIOCGRAB`，避免 tty1/fbcon 再收一份键。
- 如果仍有双输入，先确认没有两个 `lvfbterm` 实例：

```sh
pgrep -a lvfbterm
pkill -x lvfbterm
```

已验证：
- 应用先无键盘启动，2 秒后插入 uinput 键盘，500ms 内自动接管；
- `abc` + Backspace + `d` 显示 `abd`，见 `docs/qt-input-test.png`。
