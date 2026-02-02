#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <poll.h>

#define PCM_DEVICE "default"
#define SAMPLES 2048 
#define LED_SIZE 8      
#define LED_GAP 2        
#define MAX_HEIGHT_SCALE 50.0 
#define CONFIG_FILE ".spectrumrc"

unsigned char font8x8[11][8] = {
    {0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x3C}, // 0
    {0x08,0x18,0x28,0x08,0x08,0x08,0x08,0x3E}, // 1
    {0x3E,0x42,0x02,0x02,0x3E,0x40,0x40,0x3E}, // 2
    {0x3E,0x02,0x02,0x3E,0x02,0x02,0x02,0x3E}, // 3
    {0x08,0x18,0x28,0x48,0x7E,0x08,0x08,0x08}, // 4
    {0x7E,0x40,0x40,0x7C,0x02,0x02,0x42,0x3C}, // 5
    {0x3C,0x40,0x40,0x7C,0x42,0x42,0x42,0x3C}, // 6
    {0x7E,0x02,0x04,0x08,0x10,0x20,0x20,0x20}, // 7
    {0x3C,0x42,0x42,0x3C,0x42,0x42,0x42,0x3C}, // 8
    {0x3C,0x42,0x42,0x42,0x3E,0x02,0x02,0x3C}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}  // :
};

struct termios orig_termios;

void save_config(int scale, int show) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (f) {
        fprintf(f, "%d %d", scale, show);
        fclose(f);
    }
}

void load_config(int *scale, int *show) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (f) {
        if (fscanf(f, "%d %d", scale, show) != 2) {
            *scale = 4; *show = 0; // Default: non visibile se corrotto
        }
        fclose(f);
    } else {
        *scale = 4; *show = 0; // Default: non visibile se manca il file
        save_config(*scale, *show);
    }
}

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
    printf("\e[?25h\n");
}

void set_conio_terminal_mode() {
    tcgetattr(0, &orig_termios);
    atexit(reset_terminal_mode);
    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
}

void hsv_to_rgb(float h, unsigned char *r, unsigned char *g, unsigned char *b) {
    float s = 1.0, v = 1.0;
    int i = floor(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    float rf, gf, bf;
    switch (i % 6) {
        case 0: rf = v, gf = t, bf = p; break;
        case 1: rf = q, gf = v, bf = p; break;
        case 2: rf = p, gf = v, bf = t; break;
        case 3: rf = p, gf = q, bf = v; break;
        case 4: rf = t, gf = p, bf = v; break;
        case 5: rf = v, gf = p, bf = q; break;
    }
    *r = (unsigned char)(rf * 255); *g = (unsigned char)(gf * 255); *b = (unsigned char)(bf * 255);
}

void draw_scaled_char(unsigned char *buf, int start_x, int start_y, int char_idx, int xres, int yres, int line_len, int bpp, int scale, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (font8x8[char_idx][i] & (1 << (7 - j))) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int px = start_x + (j * scale) + sx;
                        int py = start_y + (i * scale) + sy;
                        if (px >= 0 && px < xres && py >= 0 && py < yres) {
                            long loc = (px * bpp) + (py * line_len);
                            buf[loc+0] = b; buf[loc+1] = g; buf[loc+2] = r;
                        }
                    }
                }
            }
        }
    }
}

int main() {
    int font_scale, show_clock;
    load_config(&font_scale, &show_clock);

    set_conio_terminal_mode();
    printf("\e[?25l"); fflush(stdout);

    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) { perror("Errore FB"); return 1; }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    long screensize = vinfo.yres_virtual * finfo.line_length;
    unsigned char *fbp = (unsigned char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    unsigned char *back_buffer = (unsigned char *)malloc(screensize);
    int bpp = vinfo.bits_per_pixel / 8;

    snd_pcm_t *handle;
    if (snd_pcm_open(&handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0) < 0) return 1;
    snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 44100, 1, 100000);

    short buffer[SAMPLES];
    double *in = fftw_alloc_real(SAMPLES);
    fftw_complex *out = fftw_alloc_complex(SAMPLES / 2 + 1);
    fftw_plan p = fftw_plan_dft_r2c_1d(SAMPLES, in, out, FFTW_ESTIMATE);

    float hue = 0.0;
    int mid_y = vinfo.yres / 2;
    int step_x = LED_SIZE + LED_GAP;
    int num_visible_bars = vinfo.xres / step_x;

    struct pollfd pfd = { .fd = 0, .events = POLLIN };

    while (1) {
        if (poll(&pfd, 1, 0) > 0) {
            char ch;
            if (read(0, &ch, 1) > 0) {
                int changed = 0;
                if (ch == '+') { font_scale++; changed = 1; }
                if (ch == '-' && font_scale > 1) { font_scale--; changed = 1; }
                if (ch == 'h' || ch == 'H') { show_clock = !show_clock; changed = 1; }
                if (changed) save_config(font_scale, show_clock);
                if (ch == 27) break; 
            }
        }

        if (snd_pcm_readi(handle, buffer, SAMPLES) != SAMPLES) {
            snd_pcm_prepare(handle);
            continue;
        }

        for (int i = 0; i < SAMPLES; i++) {
            in[i] = (buffer[i] / 32768.0) * (0.5 * (1 - cos(2 * M_PI * i / (SAMPLES - 1))));
        }
        fftw_execute(p);
        memset(back_buffer, 0, screensize); 

        for (int i = 0; i < num_visible_bars && i < SAMPLES/2; i++) {
            double magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
            int bar_height = (int)(log10(magnitude + 1.0) * MAX_HEIGHT_SCALE * (mid_y / 100.0));
            
            float h_ratio = (float)i / num_visible_bars;
            unsigned char r_base, g_base, b_base;
            if (h_ratio < 0.5) {
                float local = h_ratio / 0.5;
                r_base = 255; g_base = (unsigned char)(255 * (1.0 - local)); b_base = 0;
            } else {
                float local = (h_ratio - 0.5) / 0.5;
                r_base = (unsigned char)(255 * (1.0 - local)); g_base = 0; b_base = (unsigned char)(255 * local);
            }

            int start_x = i * step_x;
            for (int h = 0; h < bar_height; h += (LED_SIZE + LED_GAP)) {
                float dim_factor = 1.0 - ((float)h / mid_y * 0.8);
                if (dim_factor < 0.1) dim_factor = 0.1;
                unsigned char r = (unsigned char)(r_base * dim_factor);
                unsigned char g = (unsigned char)(g_base * dim_factor);
                unsigned char b = (unsigned char)(b_base * dim_factor);

                int y_up = mid_y - h - LED_SIZE;
                int y_down = mid_y + h;
                for (int dy = 0; dy < LED_SIZE; dy++) {
                    for (int dx = 0; dx < LED_SIZE; dx++) {
                        int fx = start_x + dx;
                        if (fx >= vinfo.xres) continue;
                        if (y_up + dy >= 0) {
                            long loc = (fx * bpp) + ((y_up + dy) * finfo.line_length);
                            back_buffer[loc+0] = b; back_buffer[loc+1] = g; back_buffer[loc+2] = r;
                        }
                        if (y_down + dy < vinfo.yres) {
                            long loc = (fx * bpp) + ((y_down + dy) * finfo.line_length);
                            back_buffer[loc+0] = b; back_buffer[loc+1] = g; back_buffer[loc+2] = r;
                        }
                    }
                }
            }
        }

        if (show_clock) {
            time_t rawtime;
            struct tm *ti;
            time(&rawtime);
            ti = localtime(&rawtime);
            char t_str[6]; sprintf(t_str, "%02d:%02d", ti->tm_hour, ti->tm_min);

            unsigned char r, g, b;
            hsv_to_rgb(hue, &r, &g, &b);
            hue += 0.001; 
            if (hue > 1.0) hue -= 1.0;

            int spacing = font_scale;
            int total_w = 5 * (8 * font_scale) + 4 * spacing;
            int tx = vinfo.xres - total_w - 20;
            int ty = 20;

            for (int i = 0; i < 5; i++) {
                int idx = (t_str[i] == ':') ? 10 : t_str[i] - '0';
                draw_scaled_char(back_buffer, tx + (i * (8 * font_scale + spacing)), ty, idx, vinfo.xres, vinfo.yres, finfo.line_length, bpp, font_scale, r, g, b);
            }
        }

        memcpy(fbp, back_buffer, screensize);
    }

    return 0;
}
