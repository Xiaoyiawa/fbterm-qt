/*
 * fb_ffmpeg_bridge: read one /dev/fb0 frame, then re-emit it through
 * ffmpeg's rawvideo output path.
 *
 * The point is to prove whether ffmpeg's write() path can drive the HiFB
 * panel while the LVGL mmap/commit path does not.
 *
 * Usage: fb_ffmpeg_bridge [/dev/fb0] [width] [height]
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int read_full(int fd, unsigned char* buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        ssize_t n = read(fd, buf + done, len - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read fb");
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "short read from fb: %zu/%zu\n", done, len);
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

static int write_full(int fd, const unsigned char* buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, buf + done, len - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EPIPE) {
                fprintf(stderr, "ffmpeg closed the pipe early at %zu/%zu\n", done, len);
                return -1;
            }
            perror("write pipe");
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "pipe write returned 0 at %zu/%zu\n", done, len);
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

int main(int argc, char** argv)
{
    const char* fb_path = argc > 1 ? argv[1] : "/dev/fb0";
    const int width = argc > 2 ? atoi(argv[2]) : 1920;
    const int height = argc > 3 ? atoi(argv[3]) : 1080;
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "bad size %dx%d\n", width, height);
        return 2;
    }

    const size_t frame_size = (size_t)width * (size_t)height * 4u;
    unsigned char* frame = (unsigned char*)malloc(frame_size);
    if (frame == NULL) {
        perror("malloc");
        return 1;
    }

    int fb = open(fb_path, O_RDONLY);
    if (fb < 0) {
        perror("open fb");
        free(frame);
        return 1;
    }
    if (lseek(fb, 0, SEEK_SET) < 0) {
        perror("lseek fb");
    }
    if (read_full(fb, frame, frame_size) != 0) {
        close(fb);
        free(frame);
        return 1;
    }
    close(fb);

    printf("captured %zu bytes from %s (%dx%d bgr0)\n", frame_size, fb_path,
           width, height);

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        perror("pipe");
        free(frame);
        return 1;
    }

    char video_size[64];
    snprintf(video_size, sizeof(video_size), "%dx%d", width, height);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free(frame);
        return 1;
    }

    if (pid == 0) {
        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
            _exit(126);
        }
        close(pipefd[0]);
        close(pipefd[1]);

        char* const child_argv[] = {
            (char*)"ffmpeg",
            (char*)"-hide_banner",
            (char*)"-loglevel", (char*)"error",
            (char*)"-y",
            (char*)"-f", (char*)"rawvideo",
            (char*)"-pixel_format", (char*)"bgr0",
            (char*)"-video_size", video_size,
            (char*)"-i", (char*)"pipe:0",
            (char*)"-frames:v", (char*)"1",
            (char*)"-f", (char*)"rawvideo",
            (char*)"-pix_fmt", (char*)"bgr0",
            (char*)fb_path,
            NULL,
        };
        execvp(child_argv[0], child_argv);
        fprintf(stderr, "execvp(ffmpeg) failed: %s\n", strerror(errno));
        _exit(127);
    }

    close(pipefd[0]);
    signal(SIGPIPE, SIG_IGN);
    const int wr = write_full(pipefd[1], frame, frame_size);
    close(pipefd[1]);
    free(frame);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        printf("ffmpeg exit=%d, pipe_write=%s\n", WEXITSTATUS(status),
               wr == 0 ? "ok" : "failed");
        return WEXITSTATUS(status) == 0 && wr == 0 ? 0 : 1;
    }
    if (WIFSIGNALED(status)) {
        printf("ffmpeg killed by signal %d\n", WTERMSIG(status));
    }
    return 1;
}
