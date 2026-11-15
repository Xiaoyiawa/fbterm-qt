# fbterm-qt 使用、调优与移植指南

> 项目：[README.md](README.md) · 设计：[DESIGN.md](DESIGN.md)
> 目标示例：Hi3798MV100 / Ubuntu 20.04 / hifb 1920×1080 / Qt 5.12 + QTermWidget

---

## 1. 怎么用

### 1.1 开机自启

设备上已安装：

```sh
systemctl status fbterm-qt.service
```

服务会：

- 切 `KD_GRAPHICS`，停止 tty1 fbcon 抢画面
- 禁用 `getty@tty1`
- 启动 `/bin/login` 的本地 PTY
- 正常退出不自动重启（`Restart=on-failure`）

启停：

```sh
systemctl start  fbterm-qt.service
systemctl stop   fbterm-qt.service
systemctl restart fbterm-qt.service
systemctl disable --now fbterm-qt.service
journalctl -u fbterm-qt.service -f
```

### 1.2 手动运行

```sh
cd /root/fbterm-qt
./bin/fbterm-qt
```

常用参数：

```text
--font-path=PATH      Unifont OTF 路径
--font-size=N         8..96，默认 32
--shell=CMD           默认 /bin/login
--scrollback=N        默认 5000
--keyboard=PATH       指定键盘，默认自动扫描
--mouse=PATH          指定鼠标，默认自动扫描
```

### 1.3 键盘/鼠标

- **键盘**：无键盘时每 500ms 扫 `/dev/input/event*`；插上自动接管
- **鼠标**：每 10s 扫一次，只处理滚轮
- 所有设备 `EVIOCGRAB`，防止 tty1 再收一份
- 支持热插拔：拔掉自动释放，再插自动重新识别

快捷键：

| 操作 | 行为 |
|---|---|
| `Ctrl+Alt+Q` | 退出程序，恢复文本控制台 |
| `Ctrl+滚轮` | 字号 ±2px |
| 滚轮 | QTermWidget 回滚；程序启用鼠标上报时交给程序 |
| `F1-F12` | xterm 序列（F2 = `ESC O Q`） |
| `Backspace` | `0x7f`（DEL） |
| shell 里 `exit` | 直接退出程序，不再重开 login |

### 1.4 中文输入法

默认启用 libpinyinime，数据在：

```text
share/pinyin.txt
share/dictionary.data
```

操作：

| 操作 | 行为 |
|---|---|
| `Shift` 单击 | 切换中文/英文（无中英图标） |
| 字母 | 中文模式输入拼音，库自动分音节（`zhendema` → `zhen'de'ma`） |
| `空格` | 选当前页第 1 个候选 |
| `1`-`9` | 选当前页对应候选 |
| `PageUp` / `PageDown` | 候选翻页 |
| `Backspace` | 删除一个拼音字母 |
| `Esc` | 清空拼音 |
| `Enter` | 原样提交拼音 |
| 标点 | 中文模式自动全角 |
| 切换 | 右下角短暂提示 `中` / `EN` |

关闭：

```sh
./bin/fbterm-qt --disable-ime
```

或改 systemd：

```sh
systemctl edit fbterm-qt.service
# [Service]
# ExecStart=
# ExecStart=/root/fbterm-qt/bin/fbterm-qt --disable-ime
```

### 1.5 终端环境

启动时强制：

```text
TERM=xterm-256color
COLORTERM=truecolor
```

配色：Windows Terminal **Campbell**，背景 `#0C0C0C`。

---

## 2. 怎么调

### 2.1 字体

```sh
./bin/fbterm-qt --font-size=40 --font-path=share/font.ttf
```

- 字号默认 32，整数像素
- 未指定时**只**找 `font.ttf` / `font.otf`：
  `./font.ttf`、`./font.otf`、`./share/font.ttf`、`./share/font.otf`、
  `/root/fbterm-qt/share/font.ttf`、`/root/fbterm-qt/share/font.otf`
- 两个都没有就使用 Qt 默认字体；要加载 `unifont.otf` 或其它字体，必须显式
  `--font-path=...`
- Unifont 是 dual-width（半角 8/16px，全角 16/32px），QTermWidget 会提示
  variable-width，这是正常现象
- 16 的倍数会关闭抗锯齿，画面更锐利

### 2.2 配色

文件：

```text
share/color-schemes/Campbell.colorscheme
```

QTermWidget 的格式是按段给 RGB：

```ini
[Background]
Color=12,12,12
[Foreground]
Color=204,204,204
[Color1]
Color=197,15,31
[Color1Intense]
Color=231,72,86
```

改完重启服务：

```sh
systemctl restart fbterm-qt.service
```

代码里按完整路径加载，文件可在同一个目录新增其它 `.colorscheme`，
再改 `qt/terminalwindow.cpp` 的 `schemes` 列表。

### 2.3 键表

```text
share/keytab/fbterm-qt.keytab
```

- 基于 xterm 默认键表
- `F2 = "\EOQ"`，避免 Linux 键表 `ESC[[B` 把尾部 `B` 漏进 shell
- `Backspace = "\x7f"`，避免 `^H`

构建时安装到：

```text
/usr/share/qtermwidget5/kb-layouts/fbterm-qt.keytab
```

代码用 `setKeyBindings("fbterm-qt")`（QTermWidget 只认名字，不认路径）。

### 2.4 shell / 回滚

```sh
./bin/fbterm-qt --shell="/bin/bash -l" --scrollback=10000
```

- 默认 `/bin/login`
- 回滚只影响主屏幕；备用屏幕滚轮交给程序
- `exit`/注销后直接退出程序

### 2.5 输入扫描频率

在 `qt/evdevinput.cpp` 构造函数里：

```cpp
keyboardTimer_->setInterval(500);   // 没键盘时
mouseTimer_->setInterval(10000);    // 鼠标
```

按机器 USB 枚举速度改。键盘插入后自动停止 500ms 定时器。

### 2.6 显示 / TTY

`qt/consolemode.cpp`：

```cpp
ioctl(ttyFd_, KDSETMODE, KD_GRAPHICS);  // 启动
ioctl(ttyFd_, KDSETMODE, KD_TEXT);      // 退出
```

systemd 里禁用 `getty@tty1`，否则会抢 VT。不要同时保留两个 framebuffer 程序。

### 2.7 输入法

- 引擎源码在 `qt/vendor/pinyinime/`，已编进主程序，不需要额外 `.so`
- 数据路径：`./share/pinyin.txt`、`./share/dictionary.data`；
  安装目录 `/root/fbterm-qt/share/`
- 换词库：替换这两个文件后重启服务
- 选词后库通过 `pinyin_ime_get_segments()` 保留剩余音节；例如选“真”后
  候选条显示 `de'ma`，继续选即可，不会重置已选结果
- 候选每页 9 个，`PageUp` / `PageDown` 翻页
- 拼音串/候选条样式在 `qt/terminalwindow.cpp` 的 `imeLabel_` 样式表
- 全角标点表在 `qt/ime.cpp` 的 `fullWidth()`
- 扫描频率/热插拔在 `qt/evdevinput.cpp`

### 2.8 systemd 环境

```sh
systemctl edit fbterm-qt.service
```

可覆盖：

```ini
[Service]
Environment=TERM=xterm-256color
Environment=QT_QPA_PLATFORM=linuxfb
Environment=QT_QPA_GENERIC_PLUGINS=none
Environment=QT_QPA_ENABLE_TERMINAL_KEYBOARD=0
Environment=XDG_RUNTIME_DIR=/run/fbterm-qt
```

`QT_QPA_GENERIC_PLUGINS=none` 是故意的：输入由 `EvdevInput` 自管，
避免 Qt 再开一路，造成双输入。

---

## 3. 怎么移植

### 3.1 目标机依赖

Debian/Ubuntu：

```sh
apt-get install -y g++ qtbase5-dev qtbase5-dev-tools \
    libqtermwidget5-0-dev libutf8proc-dev
```

还需要：

- Qt 的 `linuxfb` 平台插件：`/usr/lib/*/qt5/plugins/platforms/libqlinuxfb.so`
- `libqtermwidget5` 和 `libutf8proc`
- fontconfig / FreeType（Qt 字体栈）
- systemd（可选，但推荐）
- evdev 输入设备 `/dev/input/event*`
- 中文输入法数据：`share/pinyin.txt`、`share/dictionary.data`
  （源码已 vendored 在 `qt/vendor/pinyinime/`）

### 3.2 帧缓冲

- `/dev/fb0` 存在，32bpp 或 Qt linuxfb 支持的格式
- HiFB 设备已验证；Qt 的 linuxfb 路径可用
- 分辨率直接决定 QTermWidget 行列

先自检：

```sh
cd /root/fbterm-qt
./bin/fbterm-qt --help
```

### 3.3 修改路径

搜索所有 `fbterm-qt` / `/root/fbterm-qt`：

```sh
grep -RIn "fbterm-qt\|/root/fbterm-qt" \
    qt systemd scripts README.md GUIDE.md
```

需要改的常见位置：

- `qt/options.cpp`：默认字体搜索路径
- `qt/terminalwindow.cpp`：配色、keytab、工作目录
- `systemd/fbterm-qt.service`：`WorkingDirectory`、`ExecStart`、`RuntimeDirectory`
- `scripts/deploy.sh`：`DEPLOY_DIR`
- `README.md` / `GUIDE.md`：示例路径

### 3.4 构建

目标机原生（推荐，Qt 版本不必交叉）：

```sh
cd /root/fbterm-qt/qt
./build.sh
```

`build.sh` 会：

1. `qmake fbterm-qt.pro`
2. `make -j4`
3. 复制到 `../bin/fbterm-qt`
4. 安装 keytab 到 `/usr/share/qtermwidget5/kb-layouts/`

主机部署：

```sh
DEPLOY_HOST=root@192.168.1.15 DEPLOY_DIR=/root/fbterm-qt ./scripts/deploy.sh
```

### 3.5 不同架构 / 发行版

- `qt/fbterm-qt.pro` 里 `LIBS` 写死了
  `/usr/lib/arm-linux-gnueabihf/libutf8proc.so.2`
  → 改成目标架构路径，或装 `libutf8proc-dev` 后用 `-lutf8proc`
- QTermWidget 头文件默认 `/usr/include/qtermwidget5`
  → 按发行版改 `INCLUDEPATH`
- Qt 版本建议 ≥ 5.12；QTermWidget 版本差异主要在配色/键表 API
- 若用交叉 Qt，用 Qt 的 sysroot 重新跑 `qmake`，不要复用目标机 Makefile

### 3.6 验收清单

```sh
systemctl is-active fbterm-qt.service        # active
ls /root/fbterm-qt/bin/fbterm-qt             # 可执行
echo $TERM                                  # xterm-256color
tput colors                                 # 256
```

手工检查：

- 登录后中文/MOTD 正常
- Backspace 删除，不出现 `^H`
- F2 不再输出 `B`
- 拔插键盘 0.5s 内恢复
- `exit` 后程序退出，屏幕黑/文本控制台，不白屏
- `Ctrl+Alt+Q` 恢复文本模式

---

## 4. 排障

| 现象 | 检查 |
|---|---|
| 黑屏 | `QT_QPA_PLATFORM=linuxfb`；`/dev/fb0`；journal；先跑 `tools/fb_probe` |
| 无键盘 | `ls /dev/input/event*`；`journalctl -u fbterm-qt` 是否有 `evdev device added`；是否被别的进程 grab |
| F2 输出 `B` | keytab 是否装到 `/usr/share/qtermwidget5/kb-layouts/fbterm-qt.keytab`；日志是否有 translator 错误 |
| Backspace `^H` | 同上，确认 keytab 的 `Backspace : "\x7f"` |
| 双输入 | 只能有一个 `fbterm-qt` 进程；`QT_QPA_GENERIC_PLUGINS=none`；不要同时开 Qt 自带 evdev |
| exit 白屏 | 当前版本 `finished()` 直接 `quit()`，不再 `clear()`/重开 login |
| TERM 不对 | `systemctl show -p Environment fbterm-qt`；`qt/terminalwindow.cpp` 的 `setEnvironment` |
| 配色不对 | `share/color-schemes/Campbell.colorscheme` 是否被覆盖；`setColorScheme` 路径 |
| 服务起不来 | `journalctl -u fbterm-qt -n 100`；`/dev/fb0`、Qt linuxfb 插件、`/root/fbterm-qt` 权限 |

---

## 5. 文件地图

```text
qt/
├── main.cpp              linuxfb、字体、启动
├── terminalwindow.*      QTermWidget、TERM、Campbell、候选浮层
├── ime.*                 libpinyinime 状态机、全角标点
├── vendor/pinyinime/     内置拼音引擎源码
├── evdevinput.*          500ms/10s 热插拔、QKeyEvent、Shift 单击
├── consolemode.*         KD_GRAPHICS / KD_TEXT
├── signalbridge.*        SIGTERM → Qt quit
├── options.*             命令行
└── fbterm-qt.pro

systemd/fbterm-qt.service
scripts/install-service.sh
scripts/deploy.sh
share/color-schemes/Campbell.colorscheme
share/keytab/fbterm-qt.keytab
share/pinyin.txt
share/dictionary.data
docs/*.png
```
