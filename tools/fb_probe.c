/*
 * fb_probe: read-only-ish /dev/fb0 diagnostics for qt_fbterm bring-up.
 *
 * It never changes the visible page: it only prints ioctl data, writes into
 * the second page when yres_virtual allows it, and issues FBIOPAN_DISPLAY with
 * the current yoffset (a no-op). Build it for the target and run it before
 * starting qt_fbterm.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "/dev/fb0";
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        return 1;
    }

    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    memset(&fix, 0, sizeof(fix));
    memset(&var, 0, sizeof(var));

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0) {
        fprintf(stderr, "FBIOGET_*: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    printf("fb: %s\n", path);
    printf("  id=%s smem_len=%u line_length=%u type=%u visual=%u\n", fix.id,
           fix.smem_len, fix.line_length, fix.type, fix.visual);
    printf("  xres=%u yres=%u xres_virtual=%u yres_virtual=%u bpp=%u\n",
           var.xres, var.yres, var.xres_virtual, var.yres_virtual,
           var.bits_per_pixel);
    printf("  red=%u/%u green=%u/%u blue=%u/%u transp=%u/%u\n",
           var.red.offset, var.red.length, var.green.offset, var.green.length,
           var.blue.offset, var.blue.length, var.transp.offset,
           var.transp.length);
    printf("  xoffset=%u yoffset=%u\n", var.xoffset, var.yoffset);

    if (fix.smem_len == 0) {
        fprintf(stderr, "smem_len is zero; framebuffer is not initialized\n");
        close(fd);
        return 2;
    }

    void* map = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "mmap: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    volatile uint32_t* pix = (volatile uint32_t*)map;
    printf("  first pixel: 0x%08x\n", pix[0]);

    if (var.yres_virtual >= var.yres * 2) {
        const size_t page = (size_t)fix.line_length * var.yres;
        volatile uint32_t* back = (volatile uint32_t*)((uint8_t*)map + page);
        const uint32_t old = back[0];
        back[0] = 0x00a5a5a5u;
        const uint32_t readback = back[0];
        back[0] = old;
        printf("  back page write/read: 0x%08x (%s)\n", readback,
               readback == 0x00a5a5a5u ? "ok" : "mismatch");
    } else {
        printf("  single page (yres_virtual=%u)\n", var.yres_virtual);
    }

    struct fb_var_screeninfo pan = var;
    pan.activate = FB_ACTIVATE_NOW;
    if (ioctl(fd, FBIOPAN_DISPLAY, &pan) == 0) {
        printf("  FBIOPAN_DISPLAY(current yoffset): ok\n");
    } else {
        printf("  FBIOPAN_DISPLAY: %s\n", strerror(errno));
    }

    munmap(map, fix.smem_len);
    close(fd);
    return 0;
}
