/*
 * uinput_type: create a virtual USB-like keyboard and type an ASCII string.
 * Used only to test fbterm-qt on a box without a physical keyboard.
 *
 * Usage: uinput_type "user\npassword\ncommand\n"
 * It waits 3 seconds after creating the device so fbterm-qt's 2-second
 * hotplug scan can pick it up, types the text, then keeps the device alive
 * for 15 more seconds.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fd = -1;

static int write_full(const void* data, size_t len)
{
    const char* p = (const char*)data;
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, p + done, len - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

static int emit(int type, int code, int value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = (unsigned short)type;
    ev.code = (unsigned short)code;
    ev.value = value;
    return write_full(&ev, sizeof(ev));
}

static int keybit(int code)
{
    return ioctl(fd, UI_SET_KEYBIT, code);
}

static int letter_keycode(char c)
{
    switch (c) {
    case 'a': return KEY_A;
    case 'b': return KEY_B;
    case 'c': return KEY_C;
    case 'd': return KEY_D;
    case 'e': return KEY_E;
    case 'f': return KEY_F;
    case 'g': return KEY_G;
    case 'h': return KEY_H;
    case 'i': return KEY_I;
    case 'j': return KEY_J;
    case 'k': return KEY_K;
    case 'l': return KEY_L;
    case 'm': return KEY_M;
    case 'n': return KEY_N;
    case 'o': return KEY_O;
    case 'p': return KEY_P;
    case 'q': return KEY_Q;
    case 'r': return KEY_R;
    case 's': return KEY_S;
    case 't': return KEY_T;
    case 'u': return KEY_U;
    case 'v': return KEY_V;
    case 'w': return KEY_W;
    case 'x': return KEY_X;
    case 'y': return KEY_Y;
    case 'z': return KEY_Z;
    default: return -1;
    }
}

static int keycode_for(char c, int* needShift)
{
    *needShift = 0;
    if (c >= 'a' && c <= 'z') return letter_keycode(c);
    if (c >= 'A' && c <= 'Z') { *needShift = 1; return letter_keycode((char)(c - 'A' + 'a')); }
    if (c >= '1' && c <= '9') return KEY_1 + (c - '1');
    if (c == '0') return KEY_0;
    switch (c) {
    case ' ': return KEY_SPACE;
    case '\n': return KEY_ENTER;
    case 0x7f: return KEY_BACKSPACE;
    case '-': return KEY_MINUS;
    case '_': *needShift = 1; return KEY_MINUS;
    case '=': return KEY_EQUAL;
    case '+': *needShift = 1; return KEY_EQUAL;
    case '[': return KEY_LEFTBRACE;
    case ']': return KEY_RIGHTBRACE;
    case '{': *needShift = 1; return KEY_LEFTBRACE;
    case '}': *needShift = 1; return KEY_RIGHTBRACE;
    case '\\': return KEY_BACKSLASH;
    case '|': *needShift = 1; return KEY_BACKSLASH;
    case ';': return KEY_SEMICOLON;
    case ':': *needShift = 1; return KEY_SEMICOLON;
    case '\'': return KEY_APOSTROPHE;
    case '"': *needShift = 1; return KEY_APOSTROPHE;
    case ',': return KEY_COMMA;
    case '<': *needShift = 1; return KEY_COMMA;
    case '.': return KEY_DOT;
    case '>': *needShift = 1; return KEY_DOT;
    case '/': return KEY_SLASH;
    case '?': *needShift = 1; return KEY_SLASH;
    case '`': return KEY_GRAVE;
    case '~': *needShift = 1; return KEY_GRAVE;
    case '!': *needShift = 1; return KEY_1;
    case '@': *needShift = 1; return KEY_2;
    case '#': *needShift = 1; return KEY_3;
    case '$': *needShift = 1; return KEY_4;
    case '%': *needShift = 1; return KEY_5;
    case '^': *needShift = 1; return KEY_6;
    case '&': *needShift = 1; return KEY_7;
    case '*': *needShift = 1; return KEY_8;
    case '(': *needShift = 1; return KEY_9;
    case ')': *needShift = 1; return KEY_0;
    default: return -1;
    }
}

static void tap(int code, int shift)
{
    if (shift) {
        emit(EV_KEY, KEY_LEFTSHIFT, 1);
        emit(EV_SYN, SYN_REPORT, 0);
    }
    emit(EV_KEY, code, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(80000);
    emit(EV_KEY, code, 0);
    emit(EV_SYN, SYN_REPORT, 0);
    if (shift) {
        emit(EV_KEY, KEY_LEFTSHIFT, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }
    usleep(80000);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s \"text\\n\"\n", argv[0]);
        return 2;
    }

    fd = open("/dev/uinput", O_WRONLY);
    if (fd < 0) {
        perror("open /dev/uinput");
        return 1;
    }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    int bits[KEY_MAX + 1];
    memset(bits, 0, sizeof(bits));
    for (const char* p = argv[1]; *p; ++p) {
        int shift = 0;
        int code = keycode_for(*p, &shift);
        if (code >= 0) bits[code] = 1;
    }
    bits[KEY_LEFTSHIFT] = 1;
    bits[KEY_ENTER] = 1;
    bits[KEY_SPACE] = 1;
    for (int code = KEY_A; code <= KEY_Z; ++code) {
        bits[code] = 1;
    }
    for (int i = 0; i <= KEY_MAX; ++i) {
        if (bits[i]) keybit(i);
    }

    struct uinput_user_dev udev;
    memset(&udev, 0, sizeof(udev));
    snprintf(udev.name, UINPUT_MAX_NAME_SIZE, "fbterm-qt-test-kbd");
    udev.id.bustype = BUS_USB;
    udev.id.vendor = 0x1234;
    udev.id.product = 0x5678;
    udev.id.version = 1;
    if (write_full(&udev, sizeof(udev)) != 0) {
        perror("write uinput_user_dev");
        close(fd);
        return 1;
    }
    if (ioctl(fd, UI_DEV_CREATE) != 0) {
        perror("UI_DEV_CREATE");
        close(fd);
        return 1;
    }

    fprintf(stderr, "virtual keyboard created; waiting for fbterm-qt scan\n");
    usleep(3000000);

    for (int arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "@F2") == 0) {
            tap(KEY_F2, 0);
            tap(KEY_ENTER, 0);
            continue;
        }
        for (const char* p = argv[arg]; *p; ++p) {
            int shift = 0;
            int code = keycode_for(*p, &shift);
            if (code < 0) {
                fprintf(stderr, "skip char %d\n", (int)(unsigned char)*p);
                continue;
            }
            tap(code, shift);
        }
        tap(KEY_ENTER, 0);
        if (arg + 1 < argc) {
            fprintf(stderr, "waiting for prompt before next chunk\n");
            usleep(6000000);
        }
    }

    fprintf(stderr, "typed; keeping device alive\n");
    usleep(15000000);

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
