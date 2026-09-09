/*
 * Linux SDL2 CYD panel simulator (Step 6s).
 * Links fake_display + contract + codec + render (no IDF).
 *
 * Modes:
 *   --demo              scripted clear / raw / delta rects
 *   --apply file.bin…   dispatch each envelope, then hold
 *   --stdin             length-prefixed envelopes from stdin (u32 LE + bytes)
 *
 * Keys: q/Esc quit · space pause demo · n next demo step (when paused)
 */

#include "codec.h"
#include "contract.h"
#include "fake_display.h"
#include "hal_display.h"

#include <SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    DEFAULT_SCALE = 2,
    DEMO_STEP_MS = 900,
    MAX_MSG = 8192,
    RGB_SCRATCH = 80 * 240
};

static int g_scale = DEFAULT_SCALE;
static bool g_paused;
static bool g_dirty = true;

static void present(SDL_Texture *tex, SDL_Renderer *ren)
{
    const uint16_t *panel = fake_display_panel();
    if (SDL_UpdateTexture(tex, NULL, panel,
                          HAL_DISPLAY_WIDTH * (int)sizeof(uint16_t)) != 0) {
        fprintf(stderr, "SDL_UpdateTexture: %s\n", SDL_GetError());
        return;
    }
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    g_dirty = false;
}

static int dispatch_bytes(const uint8_t *buf, size_t len)
{
    int rc = contract_dispatch(buf, len);
    if (rc != CONTRACT_OK) {
        fprintf(stderr, "dispatch rc=%d len=%zu\n", rc, len);
        return rc;
    }
    g_dirty = true;
    return CONTRACT_OK;
}

static uint8_t *load_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    long sz;
    uint8_t *buf;
    if (!f) {
        perror(path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len_out = (size_t)sz;
    return buf;
}

static void fill_rgb(uint16_t *dst, int w, int h, uint16_t c)
{
    size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; i++) {
        dst[i] = c;
    }
}

static void checker_rgb(uint16_t *dst, int w, int h, uint16_t c0, uint16_t c1)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            dst[y * w + x] = ((x ^ y) & 1) ? c1 : c0;
        }
    }
}

static int pack_and_dispatch_clear(uint16_t color)
{
    uint8_t buf[CONTRACT_HDR_SIZE];
    size_t n = contract_pack_clear(buf, sizeof(buf), 1, 1, color);
    if (!n) {
        return -1;
    }
    return dispatch_bytes(buf, n);
}

static int pack_and_dispatch_rect(int16_t x, int16_t y, uint16_t w, uint16_t h,
                                  uint8_t enc, const uint16_t *rgb)
{
    size_t bound = codec_encode_bound((codec_enc_t)enc, (int)w, (int)h);
    uint8_t *payload = (uint8_t *)malloc(bound);
    uint8_t *msg;
    size_t plen = 0;
    size_t total;
    int rc;

    if (!payload) {
        return CONTRACT_ERR_NOSPACE;
    }
    rc = codec_encode((codec_enc_t)enc, rgb, (int)w, (int)h, payload, bound, &plen);
    if (rc != CODEC_OK) {
        free(payload);
        return rc;
    }
    total = (size_t)CONTRACT_HDR_SIZE + plen;
    msg = (uint8_t *)malloc(total);
    if (!msg) {
        free(payload);
        return CONTRACT_ERR_NOSPACE;
    }
    if (!contract_pack_rect(msg, total, 1, 1, x, y, w, h, enc, payload,
                            (uint32_t)plen)) {
        free(payload);
        free(msg);
        return -1;
    }
    free(payload);
    rc = dispatch_bytes(msg, total);
    free(msg);
    return rc;
}

typedef enum {
    DEMO_CLEAR_RED = 0,
    DEMO_CLEAR_BLACK,
    DEMO_RAW_GREEN,
    DEMO_DELTA_BLUE,
    DEMO_DELTA_CHECKER,
    DEMO_BARS,
    DEMO_COUNT
} demo_step_t;

static int demo_run_step(int step)
{
    static uint16_t rgb[RGB_SCRATCH];
    printf("demo step %d/%d\n", step + 1, (int)DEMO_COUNT);
    switch ((demo_step_t)step) {
    case DEMO_CLEAR_RED:
        return pack_and_dispatch_clear(0xF800);
    case DEMO_CLEAR_BLACK:
        return pack_and_dispatch_clear(0x0000);
    case DEMO_RAW_GREEN:
        fill_rgb(rgb, 40, 30, 0x07E0);
        return pack_and_dispatch_rect(20, 30, 40, 30, CODEC_ENC_RAW_RGB565, rgb);
    case DEMO_DELTA_BLUE:
        fill_rgb(rgb, 48, 24, 0x001F);
        return pack_and_dispatch_rect(200, 160, 48, 24, CODEC_ENC_DELTA_RLE_V1,
                                      rgb);
    case DEMO_DELTA_CHECKER:
        checker_rgb(rgb, 32, 24, 0xF800, 0xFFFF);
        return pack_and_dispatch_rect(120, 80, 32, 24, CODEC_ENC_DELTA_RLE_V1,
                                      rgb);
    case DEMO_BARS: {
        static const uint16_t cols[3] = {0xF800, 0x07E0, 0x001F};
        for (int i = 0; i < 3; i++) {
            fill_rgb(rgb, 80, 240, cols[i]);
            if (pack_and_dispatch_rect((int16_t)(i * 80 + 40), 0, 80, 240,
                                       CODEC_ENC_RAW_RGB565, rgb) != CONTRACT_OK) {
                return -1;
            }
        }
        return CONTRACT_OK;
    }
    default:
        return -1;
    }
}

/* Accumulator for --stdin framing: [u32 le len][len bytes] */
typedef struct {
    uint8_t hdr[4];
    size_t hdr_got;
    uint32_t msg_len;
    uint8_t msg[MAX_MSG];
    size_t msg_got;
    bool in_body;
} stdin_acc_t;

static int stdin_feed(stdin_acc_t *a, const uint8_t *chunk, size_t n)
{
    size_t i = 0;
    while (i < n) {
        if (!a->in_body) {
            a->hdr[a->hdr_got++] = chunk[i++];
            if (a->hdr_got == 4) {
                memcpy(&a->msg_len, a->hdr, 4);
                a->hdr_got = 0;
                if (a->msg_len == 0 || a->msg_len > MAX_MSG) {
                    fprintf(stderr, "bad stdin len %u\n", a->msg_len);
                    return -1;
                }
                a->in_body = true;
                a->msg_got = 0;
            }
        } else {
            size_t need = a->msg_len - a->msg_got;
            size_t take = n - i;
            if (take > need) {
                take = need;
            }
            memcpy(a->msg + a->msg_got, chunk + i, take);
            a->msg_got += take;
            i += take;
            if (a->msg_got == a->msg_len) {
                printf("stdin msg %u B\n", a->msg_len);
                (void)dispatch_bytes(a->msg, a->msg_len);
                a->in_body = false;
                a->msg_got = 0;
            }
        }
    }
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s [--scale N] --demo\n"
            "  %s [--scale N] --apply file.bin [file.bin ...]\n"
            "  %s [--scale N] --stdin\n"
            "\n"
            "Keys: q/Esc quit · space pause/resume demo · n next step (paused)\n",
            argv0, argv0, argv0);
}

int main(int argc, char **argv)
{
    enum { MODE_NONE, MODE_DEMO, MODE_APPLY, MODE_STDIN } mode = MODE_NONE;
    const char *apply_files[64];
    int n_apply = 0;
    int demo_step = 0;
    uint32_t next_demo_ms = 0;
    bool running = true;
    bool stdin_done = false;
    stdin_acc_t sacc;

    memset(&sacc, 0, sizeof(sacc));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--demo") == 0) {
            mode = MODE_DEMO;
        } else if (strcmp(argv[i], "--stdin") == 0) {
            mode = MODE_STDIN;
        } else if (strcmp(argv[i], "--apply") == 0) {
            mode = MODE_APPLY;
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            g_scale = atoi(argv[++i]);
            if (g_scale < 1) {
                g_scale = 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            if (n_apply < 64) {
                apply_files[n_apply++] = argv[i];
            }
            if (mode == MODE_NONE) {
                mode = MODE_APPLY;
            }
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }
    if (mode == MODE_NONE) {
        mode = MODE_DEMO;
    }
    if (mode == MODE_APPLY && n_apply == 0) {
        fprintf(stderr, "--apply needs at least one file\n");
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    const int win_w = HAL_DISPLAY_WIDTH * g_scale;
    const int win_h = HAL_DISPLAY_HEIGHT * g_scale;
    SDL_Window *win = SDL_CreateWindow(
        "esp32-wl-display CYD sim (320x240)", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, win_w, win_h, SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(ren, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT);

    SDL_Texture *tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
        HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    fake_display_reset();
    present(tex, ren);

    if (mode == MODE_APPLY) {
        for (int i = 0; i < n_apply; i++) {
            size_t len = 0;
            uint8_t *buf = load_file(apply_files[i], &len);
            if (!buf) {
                running = false;
                break;
            }
            printf("apply %s (%zu B)\n", apply_files[i], len);
            if (dispatch_bytes(buf, len) != CONTRACT_OK) {
                free(buf);
                running = false;
                break;
            }
            free(buf);
            present(tex, ren);
        }
        printf("applied %d file(s); window open (q to quit)\n", n_apply);
    } else if (mode == MODE_DEMO) {
        next_demo_ms = SDL_GetTicks();
        printf("demo: space=pause n=step q=quit\n");
    } else {
        int fd = fileno(stdin);
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl >= 0) {
            fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        }
        printf("stdin mode: u32le-len + envelope (q to quit)\n");
    }

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode k = ev.key.keysym.sym;
                if (k == SDLK_ESCAPE || k == SDLK_q) {
                    running = false;
                } else if (k == SDLK_SPACE && mode == MODE_DEMO) {
                    g_paused = !g_paused;
                    if (!g_paused) {
                        next_demo_ms = SDL_GetTicks();
                    }
                    printf(g_paused ? "paused\n" : "resume\n");
                } else if (k == SDLK_n && mode == MODE_DEMO && g_paused) {
                    if (demo_run_step(demo_step) != CONTRACT_OK) {
                        fprintf(stderr, "demo step failed\n");
                    }
                    demo_step = (demo_step + 1) % (int)DEMO_COUNT;
                }
            }
        }

        if (mode == MODE_DEMO && !g_paused) {
            uint32_t now = SDL_GetTicks();
            if (now >= next_demo_ms) {
                if (demo_run_step(demo_step) != CONTRACT_OK) {
                    fprintf(stderr, "demo step failed\n");
                    running = false;
                }
                demo_step = (demo_step + 1) % (int)DEMO_COUNT;
                next_demo_ms = now + DEMO_STEP_MS;
            }
        }

        if (mode == MODE_STDIN && !stdin_done) {
            uint8_t chunk[512];
            ssize_t r = read(fileno(stdin), chunk, sizeof(chunk));
            if (r > 0) {
                if (stdin_feed(&sacc, chunk, (size_t)r) != 0) {
                    stdin_done = true;
                }
            } else if (r == 0) {
                stdin_done = true;
                printf("stdin EOF; window open (q to quit)\n");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                stdin_done = true;
                perror("stdin");
            }
        }

        if (g_dirty) {
            present(tex, ren);
        } else {
            SDL_Delay(10);
        }
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
