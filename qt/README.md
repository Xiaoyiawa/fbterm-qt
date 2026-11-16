# fbterm-qt

Qt 5 / QTermWidget 版终端，使用 Qt 的 `linuxfb` 平台插件直接输出 `/dev/fb0`。
主机交叉编译（Qt 5.12.8 armhf 目标）。

## 依赖

```sh
apt-get install -y g++ qtbase5-dev qtbase5-dev-tools \
    libqtermwidget5-0-dev libutf8proc-dev
```

## 构建

用 Makefile，不调用额外脚本：

```sh
export CC=$HOME/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-gcc
export CXX=$HOME/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-g++
export LD=$HOME/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-g++
export AR=$HOME/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-ar
export RANLIB=$HOME/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-ranlib

make SYSROOT=build/sysroot
make install
```

`build/fbterm-qt` 是二进制，`build/usr/` 是完整安装树。

## 运行

```sh
/usr/bin/fbterm-qt
```

参数：

```text
--font-path=PATH      Unifont OTF 路径
--font-size=N         8..96，默认 32
--shell=CMD           默认 /bin/login
--scrollback=N        默认 5000
--keyboard=PATH       指定键盘（默认自动扫描）
--mouse=PATH          指定鼠标（默认自动扫描）
--disable-ime         关闭内置中文输入法
```

中文输入法默认启用，数据在 `share/pinyin.txt`、`share/dictionary.data`。
`Shift` 单击切换中英；空格/数字选词，→/← 翻页；末尾 `->` 表示还有候选；选词后保留剩余拼音
（`zhendema` 选“真”后剩 `de'ma`）；中文模式标点自动全角；切换时右下角短暂
提示“中”/“EN”，不显示常驻中英图标。

快捷键：

- `Ctrl+Alt+Q` 退出
- `Ctrl+滚轮` 字号 ±2px

## 开机自启动

systemd 服务：

```sh
systemctl status fbterm-qt.service
journalctl -u fbterm-qt.service -f
```

`make install` 会把 unit stage 到 `build/usr/lib/systemd/system`；部署时整棵 `build/usr` 覆盖到目标 `/`。
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
- 字体：`--font-path` 可重复，第一个是主字体，后续是 fallback；没指定时找
  `font.ttf` / `font.otf`；都没有用 Qt 默认字体
- 候选条/toast 使用终端当前字体
- 配色：`share/color-schemes/Campbell.colorscheme`（Windows Terminal Campbell，深色 `#0C0C0C`）
- 已知：QTermWidget 会提示 Unifont variable-width；Ctrl+滚轮优先于程序鼠标上报
- 已知：中文输入法、复制粘贴、systemd 自启未做
