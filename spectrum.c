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

#define PCM_DEVICE "default"
#define SAMPLES 2048 

#define LED_SIZE 8      
#define LED_GAP 2       
#define MAX_HEIGHT_SCALE 50.0 

int main() {
    printf("\e[?25l"); 
    fflush(stdout);

    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) { perror("Errore FB"); return 1; }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    long screensize = vinfo.yres_virtual * finfo.line_length;
    unsigned char *fbp = (unsigned char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    
    unsigned char *back_buffer = (unsigned char *)malloc(screensize);
    if (!back_buffer) { perror("Errore allocazione buffer"); return 1; }

    int bpp = vinfo.bits_per_pixel / 8;

    snd_pcm_t *handle;
    snd_pcm_open(&handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 44100, 1, 500000);

    short buffer[SAMPLES];
    double *in = fftw_alloc_real(SAMPLES);
    fftw_complex *out = fftw_alloc_complex(SAMPLES / 2 + 1);
    fftw_plan p = fftw_plan_dft_r2c_1d(SAMPLES, in, out, FFTW_ESTIMATE);

    int step_x = LED_SIZE + LED_GAP;
    int num_visible_bars = vinfo.xres / step_x;
    int mid_y = vinfo.yres / 2;

    while (1) {
        if (snd_pcm_readi(handle, buffer, SAMPLES) != SAMPLES) {
            snd_pcm_prepare(handle);
            continue;
        }

        for (int i = 0; i < SAMPLES; i++) {
            double window = 0.5 * (1 - cos(2 * M_PI * i / (SAMPLES - 1)));
            in[i] = (buffer[i] / 32768.0) * window;
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
                        int final_x = start_x + dx;
                        if (final_x >= vinfo.xres) continue;

                        if (y_up + dy >= 0) {
                            long loc = (final_x * bpp) + ((y_up + dy) * finfo.line_length);
                            back_buffer[loc+0] = b; back_buffer[loc+1] = g; back_buffer[loc+2] = r;
                        }
                        if (y_down + dy < vinfo.yres) {
                            long loc = (final_x * bpp) + ((y_down + dy) * finfo.line_length);
                            back_buffer[loc+0] = b; back_buffer[loc+1] = g; back_buffer[loc+2] = r;
                        }
                    }
                }
            }
        }
        
        memcpy(fbp, back_buffer, screensize);
        usleep(16000); 
    }

    free(back_buffer);
    return 0;
}
