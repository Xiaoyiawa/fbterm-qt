# lvfbterm 设计文档

> 一个直接占用 `/dev/fb0`、用 LVGL + FreeType 渲染、用 libvterm 解析 ANSI 的本地伪终端。
> 目标机：`root@192.168.1.15`（Hi3798MV100 / armv7l / Ubuntu 20.04）。

---

## 1. 目标

在 Linux 帧缓冲设备上提供一个**全屏、可交互的本地终端**：

- 画面：LVGL 负责显示驱动与合成，FreeType 负责字形栅格化，Unifont 提供字形。
- 终端语义：libvterm 解析 VT/ANSI/xterm 序列，维护屏幕、光标、颜色、备用屏幕、鼠标模式。
- 会话：`forkpty()` 拉起 `/bin/login`（默认），键盘输入写入 PTY，PTY 输出喂给 libvterm。
- 交互：USB 键盘（evdev）+ USB 鼠标滚轮；无鼠标指针。
- 缩放：`Ctrl + 滚轮` 改字号，重算行列并 `SIGWINCH`。
- 退出：`Ctrl + Alt + Q`。

## 2. 非目标（当前版本）

- 不做中文输入法（后续可接 [libpinyinime](https://github.com/RobinNotBad/libpinyinime)，明确不用 LVGL 自带 IME）。
- 不做鼠标点击、拖选、复制粘贴、超链接、Sixel/Kitty 图形。
- 不做多标签、多窗口、状态栏。
- 不做开机自启（先手动运行，后续可加 systemd unit）。
- 不做回滚历史重排（缩放后旧行裁剪/补空）。
- 不做声音；BEL 忽略。

## 3. 目标设备事实

| 项 | 值 |
|---|---|
| SoC | Hi3798MV100，4×Cortex-A7，NEON/VFPv4，937 MB RAM |
| 系统 | Ubuntu 20.04.6 LTS armhf，glibc 2.31 |
| 内核 | 4.4.35，`console=ttyAMA0` |
| 帧缓冲 | `/dev/fb0`，`hifb`，1920×1080，32bpp，stride 7680，`yres_virtual=2160` |
| 像素格式 | `rgba 8/16,8/8,8/0,8/24` → 内存 B,G,R,X（XRGB8888） |
| 输入 | 当前只有 `/dev/input/mice`；USB 键盘/鼠标插入后出现 `event*` |
| 工具 | 设备侧只有 gcc 9.4；交叉工具链在 `~/x-tools/arm-unknown-linux-gnueabihf/`（GCC 9.5，glibc 2.28） |
| 其他 | `xterm-256color` terminfo 存在；`/bin/login` 无 `pam_securetty` 限制；设备有 `libstdc++.so.6.0.28` |

## 4. 总体架构

```
                  ┌──────────────────────────────┐
  USB keyboard ──▶│ input::InputManager (evdev)  │
  USB mouse    ──▶│  - 修饰键状态 / 热插拔扫描    │
                  │  - 滚轮优先级路由             │
                  └───────┬──────────────┬───────┘
                          │ keys         │ wheel
                          ▼              ▼
                  ┌──────────────┐  ┌────────────────────┐
                  │ libvterm     │  │ 本地操作：缩放/回滚 │
                  │ keyboard API │  │ 或转成鼠标上报      │
                  └──────┬───────┘  └─────────┬──────────┘
                         │ output bytes       │
                         ▼                    │
                  ┌──────────────┐            │
                  │ PTY master   │◀───────────┘
                  │  ↕ /bin/login│
                  └──────┬───────┘
                         │ output bytes
                         ▼
                  ┌──────────────────────────────┐
                  │ term::Terminal               │
                  │  VTerm + VTermScreen         │
                  │  cells / cursor / modes      │
                  │  scrollback (5000)           │
                  └──────┬───────────────┬───────┘
                         │ damage        │ settermprop
                         ▼               ▼
                  ┌──────────────────────────────┐
                  │ render::Renderer             │
                  │  FreeType 字形缓存            │
                  │  画到 XRGB8888 canvas        │
                  └──────┬───────────────────────┘
                         │ lv_obj_invalidate_area
                         ▼
                  ┌──────────────────────────────┐
                  │ LVGL                         │
                  │  lv_canvas (1920×1080)       │
                  │  lv_linux_fbdev (partial)    │
                  └──────┬───────────────────────┘
                         │ flush
                         ▼
                    /dev/fb0 (hifb)
```

### 4.1 为什么用 LVGL canvas，而不是每格一个 label

终端是固定栅格，更新是“局部矩形 + 滚动”。每格一个 `lv_label` 会产生几千个对象、每帧大量 draw task，滚动时尤其慢。

这里采用：

- 一个全屏 `lv_canvas`（XRGB8888，1920×1080）。
- 自己用 FreeType 把每个 cell 的背景、字形、下划线、光标直接画进 canvas 缓冲区。
- 只对发生变化的矩形调用 `lv_obj_invalidate_area()`，LVGL 只合成脏区并 flush 到 fb。
- LVGL 仍然负责：显示/驱动、tick、脏区合成、帧缓冲输出。
- **hifb 的 mmap 写必须每帧提交 `ioctl(fb, 0x4606, &vinfo)`**：
  mmap 写完不会自动扫描输出；旧项目 `lcd_refresh()` 就是这句。
  `LV_LINUX_FBDEV_MMAP` 保持 1，`Display::commitFb()` 在每次
  `lv_timer_handler()` / `lv_refr_now()` 之后调用 0x4606。

这样既保留 LVGL 的显示栈，又避免把终端拆成几千个控件。

### 4.2 字形渲染

- FreeType 2.15，运行时加载 OTF。
- 字号为 16 的整数倍（16/32/48/64/80/96）时用 `FT_LOAD_TARGET_MONO` + 1-bit 位图，最锐利。
- 其他字号用灰度抗锯齿（`FT_RENDER_MODE_NORMAL`）。
- 字形缓存：`(face, glyph_index, bold, italic, size, mode) → bitmap + metrics`，LRU 上限约 4096 个字形；缩放时整体失效。
- 粗体：优先用字体自带 bold face；没有则 `FT_GlyphSlot_Embolden`。
- 斜体：优先自带 italic face；没有则 `FT_GlyphSlot_Oblique`。
- 字体缺字：`FT_Get_Char_Index() == 0` 时画空心方块。
- 宽字符：libvterm 的 `cell.width == 2`，占两个 cell 宽。

### 4.3 字体与 fallback

- `--font-path=PATH` 可重复，顺序即优先级：第一个是主字体，后面是 fallback。
- 未给 `--font-path` 时按顺序搜索：
  1. `./unifont.otf`
  2. `./share/unifont.otf`
  3. `../share/unifont.otf`
  4. `/usr/local/share/lvfbterm/unifont.otf`
  5. `/usr/share/fonts/opentype/unifont/unifont.otf`
  6. `/usr/share/fonts/truetype/unifont/unifont.otf`
  7. `/usr/share/unifont/unifont.otf`
- 等宽判定：**不依赖** `FT_FACE_FLAG_FIXED_WIDTH` / fontconfig `FC_MONO`。
  Unifont 是 dual-width（半角等宽 + 全角双宽），fontconfig 标 `spacing: 90`。
  实际检查 ASCII 可见字符的 advance 是否一致；全角允许 2×。
- 主字体不是等宽：打印警告，仍继续用，格宽取 `'M'` 的 advance。
- 所有字体都加载失败：当前版本画空心方块，保证程序不崩；LVGL 内置英文字体（`lv_font_unscii_16`）回退已编入但尚未接到自绘管线，列为 TODO。
- 缺字 fallback：按 `--font-path` 顺序找第一个有该 codepoint 的 face。

### 4.4 PTY 与会话

- `openpty()` + `fork()` + `setsid()` + `TIOCSCTTY()`，子进程 `execv()`。
- 默认：`/bin/login`。`--shell=PATH` 时跳过 login，直接执行 PATH（可带参数，简单空白分割）。
- 子进程环境：`TERM=xterm-256color`、`COLORTERM=truecolor`、`LANG=C.UTF-8`（仅当未设置时）。
- 窗口大小：`TIOCSWINSZ`，行列由“屏幕尺寸 / 字格尺寸”算出；缩放时更新并发 `SIGWINCH`。
- `/bin/login` 退出（注销）→ 自动重新拉起；`--shell` 进程退出 → 整个程序退出。
- PTY 输出：非阻塞读，单线程主循环内喂给 libvterm；不做额外线程，避免锁。
- libvterm 的输出回调（设备查询、鼠标上报、键盘序列）直接写回 PTY master。

### 4.5 输入路由

键盘：

- 固定 US QWERTY。
- 修饰键：左/右 Ctrl、Alt、Shift 分别跟踪。
- 特殊键交给 `vterm_keyboard_key()`，由 libvterm 处理 DECCKM、应用键盘、修饰参数。
- Ctrl + 字母：自己发传统控制码（`Ctrl+A..Z` → 0x01..0x1A，`Ctrl+[` → ESC，`Ctrl+\`/`]`/`^`/`_` → 0x1C..0x1F），避免 libvterm 0.3.3 对 `Ctrl+M/I/[` 发 CSI-u 的兼容性问题。
- Alt + 字符：ESC 前缀。
- `Ctrl+Alt+Q`：退出程序，永远最高优先。
- 键盘 `EVIOCGRAB` 独占，退出释放；热插拔每 2 秒扫描 `/dev/input/event*`。

滚轮优先级（从高到低）：

1. 程序鼠标模式 `VTERM_PROP_MOUSE != NONE` → `vterm_mouse_button(4/5)` 原样给程序（带 Ctrl/Alt/Shift 修饰）。
2. 按住 Ctrl → 字号 ±2px（8–96），重算行列，`SIGWINCH`。
3. 备用屏幕 → 上滚发 `↑`、下滚发 `↓`。
4. 主屏幕 → 回滚历史（每格 3 行）。

光标：

- 跟随 `VTERM_PROP_CURSORSHAPE` / `CURSORBLINK` / `CURSORVISIBLE`。
- 默认方块，500ms 闪烁。
- 回滚查看历史时隐藏光标；新输出立即跳回底部（因此一般看不到“悬停光标”状态）。

BEL：忽略。

### 4.6 回滚

- libvterm `sb_pushline` 把滚出屏幕的行交给终端，存入 `std::deque<HistoryLine>`，上限 `--scrollback`（默认 5000）。
- 历史行保留原始列数和 cell 属性；缩放后不重排，渲染时按当前列数裁剪/补空。
- `sb_popline` 支持反向滚动时把历史行取回屏幕。
- 备用屏幕不写历史。

### 4.7 滚动优化

- `moverect` 回调：把 canvas 像素按矩形搬移（`memmove`），只重画暴露出来的行。
- 光标绘制前保存 cell 像素补丁；搬移/重绘前先恢复补丁，避免光标被“搬走”。
- 如果驱动/回调没有给 `moverect`，退化为整屏重画。

## 5. 模块与目录

```
lvfbterm/
├── DESIGN.md
├── README.md
├── Makefile
├── config/
│   └── lv_conf.h              # 精简 LVGL 配置
├── src/
│   ├── main.cpp               # 入口
│   ├── app/
│   │   ├── app.h/.cpp         # 生命周期、主循环、信号
│   │   ├── options.h/.cpp     # 命令行
│   │   └── log.h/.cpp         # 日志
│   ├── term/
│   │   ├── cell.h             # 紧凑 cell / 历史行
│   │   ├── terminal.h/.cpp    # libvterm 封装、回调、damage、回滚
│   │   ├── scrollback.h/.cpp  # 历史环形缓冲
│   │   └── pty.h/.cpp         # forkpty / 读写 / resize / 子进程
│   ├── input/
│   │   ├── input.h/.cpp       # evdev 扫描、修饰键、滚轮路由
│   │   └── keymap_us.h/.cpp   # US 键位映射
│   ├── render/
│   │   ├── font.h/.cpp        # FreeType 初始化、face、等宽检查、fallback
│   │   ├── glyph.h/.cpp       # 字形缓存
│   │   ├── renderer.h/.cpp    # 画 cell / 光标 / 下划线到 canvas
│   │   └── palette.h/.cpp     # xterm 256 色 + 默认色
│   └── ui/
│       ├── canvas.h/.cpp      # LVGL canvas 与脏区
│       └── display.h/.cpp     # LVGL 初始化、fbdev、tick
├── tools/
│   └── fb_probe.c             # 设备帧缓冲探测
├── scripts/
│   └── deploy.sh
├── tests/
└── build/                     # 生成物
    ├── obj/{app,lvgl,vterm,freetype}
    ├── deps/{include,lib}
    └── ...
└── bin/lvfbterm
```

## 6. 主循环

```
lv_init()
lv_linux_fbdev_create() + set_file("/dev/fb0")
canvas = Canvas(1920x1080)
font = FontManager(...)
renderer = Renderer(canvas, font)
terminal = Terminal(rows, cols, scrollback)
pty = Pty("/bin/login", rows, cols)
input = InputManager(...)

while (!quit) {
    timeout = min(lv_timer_handler(), 16ms)
    poll(pty_fd, input_fds, signalfd, timeout)
    if key:   route -> terminal/pty
    if wheel: route -> program/zoom/scroll
    if pty:   read -> terminal.feed() -> damage
    if sig:   SIGCHLD -> restart/quit, SIGTERM -> quit
    terminal.flush()   // vterm_screen_flush_damage + render dirty + invalidate
    lv_timer_handler() // flush to fb
}
```

单线程：libvterm、FreeType、LVGL 都只在主线程使用，无锁。

## 7. 命令行

```
lvfbterm [options]

  --fb=PATH            帧缓冲设备，默认 /dev/fb0
  --font-size=N        字号（像素高度），默认 32，范围 8..96
  --font-path=PATH     字体路径，可重复；第一个主字体，其余 fallback
  --scrollback=N       回滚行数，默认 5000
  --shell=CMD          PTY 中执行的程序，默认 /bin/login
  --keyboard=PATH      指定 evdev 键盘（可重复），默认自动扫描
  --mouse=PATH         指定 evdev 鼠标（可重复），默认自动扫描
  --log-level=LEVEL    error|warn|info|debug，默认 info
  --help
  --version
```

## 8. 构建

- 只使用 Makefile，不使用 CMake。
- 交叉工具链：`$(HOME)/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-`
- 依赖：
  - LVGL：源码在 `../lvgl`，编成 `liblvgl.a`，静态链进主程序。
  - libvterm：源码在 `../libvterm`，编成 `libvterm.so`，动态部署。
  - FreeType：源码在 `../freetype`，`configure` 交叉编译成 `libfreetype.so`，动态部署。
- 产物：
  - `bin/lvfbterm`
  - `build/deps/lib/libvterm.so*`
  - `build/deps/lib/libfreetype.so*`
- 主程序 `-Wl,-rpath,'$ORIGIN/../lib'`。
- C++11（`-std=c++11`），C11（`-std=c11`），`-O2 -g -Wall -Wextra`。

## 9. 部署

```
/root/lvfbterm/
├── bin/lvfbterm
├── lib/libvterm.so*
├── lib/libfreetype.so*
└── share/unifont.otf
```

启动：

```
cd /root/lvfbterm
./bin/lvfbterm --font-path=share/unifont.otf
```

手动运行，暂不做 systemd。`Ctrl+Alt+Q` 退出并恢复文本模式。

## 10. 风险与限制

1. **hifb 兼容性**：标准 fbdev ioctl/mmap 需实测；`FBIOBLANK`/`FBIO_WAITFORVSYNC` 可能不支持，失败只告警。
2. **fbcon 干扰**：启动时对 `/dev/tty0` 发 `KDSETMODE(KD_GRAPHICS)`，退出恢复 `KD_TEXT`；若权限/设备不支持则跳过。
3. **CPU 性能**：Cortex-A7 单核软件渲染 1080p；依靠脏区 + 滚动像素搬移，整屏重画只在缩放/回滚时发生。
4. **字体非等宽**：只告警不拒绝；格宽取 `'M'`，字可能挤/留白。
5. **历史不重排**：缩放后旧行按新列宽裁剪/补空。
6. **无输入法**：只能输入 ASCII；中文可显示、可由程序输出。
7. **USB 键鼠热插拔**：2 秒轮询扫描，插入后最多 2 秒可用。
8. **grab 失败**：非 root 或被其他进程占用时，键盘事件可能同时进入控制台。

## 11. 里程碑

- M1：fbdev + LVGL 画出一屏测试图案。
- M2：PTY + libvterm + 登录提示，键盘可输入。
- M3：FreeType + Unifont 正常渲染 ASCII/中文/颜色/光标。
- M4：缩放、回滚、鼠标滚轮、退出恢复。
- M5：部署脚本、README、设备截图验证。

---

## 12. Qt 版（当前推荐）

LVGL 版在 hifb 上踩到 mmap 不参与扫描输出的问题后，增加了 Qt 5 / QTermWidget 版本。

### 12.1 技术选择

- 显示：Qt 5.12 的 `linuxfb` 平台插件。设备上 qterm 已验证这条路径可用，
  最小 Qt 全屏测试也确认能显示红底和文字。
- 终端：`QTermWidget`（自带 VT 解析、PTY、滚动、选区、鼠标上报）。
- 字体：`QFontDatabase::addApplicationFont("unifont.otf")` + QFont pixel size；
  Qt 底层用 FreeType 渲染。
- 键盘：Qt `evdevkeyboard` 通用插件；启动前扫描 `/dev/input/event*`，
  设置 `QT_QPA_GENERIC_PLUGINS=evdevkeyboard:/dev/input/eventN`。
- 构建：目标机原生 `qmake && make`（Qt 5.12.8 armhf + QTermWidget 0.14）。

### 12.2 与 LVGL 版的取舍

| 项 | LVGL 版 | Qt 版 |
|---|---|---|
| 显示 | LVGL fbdev，hifb 需 write() | Qt linuxfb，已验证 |
| VT | libvterm | QTermWidget 内置 |
| 字体 | FreeType 自绘 | QFont/FreeType |
| 构建 | 交叉编译 + 静态 LVGL | 目标机 qmake 原生 |
| 状态 | legacy | 推荐 |

### 12.3 已知限制

- QTermWidget 会提示 Unifont 是 variable-width（dual-width 的正常现象），
  实测登录/MOTD/中文对齐正常。
- QTermWidget 没暴露终端 mouse mode，Ctrl+滚轮缩放优先于程序鼠标上报。
- 中文输入法、复制粘贴、systemd 仍未做。
