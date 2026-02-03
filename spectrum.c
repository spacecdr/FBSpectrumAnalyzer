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
#include <curl/curl.h>

#define PCM_DEVICE "default"
#define SAMPLES 2048 
#define LED_SIZE 8      
#define LED_GAP 2        
#define MAX_HEIGHT_SCALE 50.0 
#define ART_SIZE 150 
#define RC_FILE ".spectrumrc"

#define CLIENT_ID "IL_TUO_CLIENT_ID"
#define CLIENT_SECRET "IL_TUO_CLIENT_SECRET"
#define REFRESH_TOKEN "IL_TUO_REFRESH_TOKEN"

struct termios orig_termios;
unsigned char art_pixels[ART_SIZE * ART_SIZE * 3];
char access_token[1024] = "";
char last_art_url[512] = "";
int spotify_active = 0;
int show_spotify = 0;
int show_clock = 0;
int font_scale = 4;

unsigned char font_ascii[128][8] = {
    [32]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, [45]={0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00},
    [46]={0x00,0x00,0x00,0x00,0x00,0x60,0x60,0x00}, [48]={0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x3C},
    [49]={0x08,0x18,0x28,0x08,0x08,0x08,0x08,0x3E}, [50]={0x3E,0x42,0x02,0x02,0x3E,0x40,0x40,0x3E},
    [51]={0x3E,0x02,0x02,0x3E,0x02,0x02,0x02,0x3E}, [52]={0x08,0x18,0x28,0x48,0x7E,0x08,0x08,0x08},
    [53]={0x7E,0x40,0x40,0x7C,0x02,0x02,0x42,0x3C}, [54]={0x3C,0x40,0x40,0x7C,0x42,0x42,0x42,0x3C},
    [55]={0x7E,0x02,0x04,0x08,0x10,0x20,0x20,0x20}, [56]={0x3C,0x42,0x42,0x3C,0x42,0x42,0x42,0x3C},
    [57]={0x3C,0x42,0x42,0x42,0x3E,0x02,0x02,0x3C}, [58]={0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    [65]={0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x42}, [66]={0x7C,0x42,0x42,0x7C,0x42,0x42,0x42,0x7C},
    [67]={0x3C,0x42,0x40,0x40,0x40,0x40,0x42,0x3C}, [68]={0x78,0x44,0x42,0x42,0x42,0x42,0x44,0x78},
    [69]={0x7E,0x40,0x40,0x78,0x40,0x40,0x40,0x7E}, [70]={0x7E,0x40,0x40,0x78,0x40,0x40,0x40,0x40},
    [71]={0x3C,0x42,0x40,0x4E,0x42,0x42,0x42,0x3C}, [72]={0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42},
    [73]={0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x3E}, [74]={0x02,0x02,0x02,0x02,0x02,0x42,0x42,0x3C},
    [75]={0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x42}, [76]={0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7E},
    [77]={0x42,0x66,0x5A,0x5A,0x42,0x42,0x42,0x42}, [78]={0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x42},
    [79]={0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x3C}, [80]={0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x40},
    [81]={0x3C,0x42,0x42,0x42,0x42,0x4A,0x44,0x3A}, [82]={0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x42},
    [83]={0x3C,0x42,0x40,0x3C,0x02,0x02,0x42,0x3C}, [84]={0x7E,0x08,0x08,0x08,0x08,0x08,0x08,0x08},
    [85]={0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C}, [86]={0x42,0x42,0x42,0x42,0x42,0x24,0x24,0x18},
    [87]={0x42,0x42,0x42,0x5A,0x5A,0x5A,0x66,0x42}, [88]={0x42,0x24,0x18,0x18,0x18,0x24,0x42,0x42},
    [89]={0x42,0x42,0x24,0x18,0x08,0x08,0x08,0x08}, [90]={0x7E,0x02,0x04,0x08,0x10,0x20,0x40,0x7E}
};

struct MemoryStruct { char *memory; size_t size; };

void save_rc() {
    FILE *f = fopen(RC_FILE, "w");
    if (f) { fprintf(f, "show_clock=%d\nshow_spotify=%d\nfont_scale=%d\n", show_clock, show_spotify, font_scale); fclose(f); }
}

void load_rc() {
    FILE *f = fopen(RC_FILE, "r");
    if (!f) return;
    fscanf(f, "show_clock=%d\nshow_spotify=%d\nfont_scale=%d\n", &show_clock, &show_spotify, &font_scale);
    fclose(f);
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(0, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
}

void reset_terminal_mode() { tcsetattr(0, TCSANOW, &orig_termios); printf("\e[?25h\n"); }

void quick_parse(const char *src, const char *key, char *dest) {
    char search[128]; sprintf(search, "\"%s\"", key);
    char *p = strstr(src, search);
    if (p) {
        p = strchr(p, ':'); if (!p) return;
        while(*p != '\"' && *p != '\0') p++;
        if (*p == '\"') {
            p++;
            char *end = strchr(p, '\"');
            if (end) {
                int len = end - p;
                if (len > 255) len = 255;
                strncpy(dest, p, len);
                dest[len] = '\0';
            }
        }
    }
}

void refresh_spotify_token() {
    if (strcmp(CLIENT_ID, "IL_TUO_CLIENT_ID") == 0) return;
    CURL *curl = curl_easy_init();
    if(curl) {
        struct MemoryStruct chunk = {malloc(1), 0};
        char fields[2048]; sprintf(fields, "grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s", REFRESH_TOKEN, CLIENT_ID, CLIENT_SECRET);
        curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        if(curl_easy_perform(curl) == CURLE_OK) quick_parse(chunk.memory, "access_token", access_token);
        free(chunk.memory); curl_easy_cleanup(curl);
    }
}

void update_spotify_api(char *t, char *a, char *al) {
    if (!show_spotify || access_token[0] == '\0' || strcmp(CLIENT_ID, "IL_TUO_CLIENT_ID") == 0) {
        spotify_active = 0; return;
    }
    CURL *curl = curl_easy_init();
    if(curl) {
        struct MemoryStruct chunk = {malloc(1), 0};
        struct curl_slist *headers = NULL;
        char auth[1100]; sprintf(auth, "Authorization: Bearer %s", access_token);
        headers = curl_slist_append(headers, auth);
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.spotify.com/v1/me/player/currently-playing");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        if(curl_easy_perform(curl) == CURLE_OK && chunk.size > 500 && strstr(chunk.memory, "\"item\"")) {
            spotify_active = 1;
            char *item_ptr = strstr(chunk.memory, "\"item\"");
            char *alb_start = strstr(item_ptr, "\"album\"");
            if (alb_start) {
                char *first_n = strstr(alb_start, "\"name\"");
                if (first_n) {
                    char *second_n = strstr(first_n + 6, "\"name\"");
                    if (second_n) quick_parse(second_n - 1, "name", al);
                    else quick_parse(alb_start, "name", al);
                }
                char url[512] = "";
                char *img_ptr = strstr(alb_start, "\"images\"");
                if (img_ptr) {
                    quick_parse(img_ptr, "url", url);
                    if (url[0] != '\0' && strcmp(url, last_art_url) != 0) {
                        strcpy(last_art_url, url);
                        char cmd[1024];
                        sprintf(cmd, "curl -s %s | ffmpeg -loglevel quiet -y -i pipe:0 -vf scale=%d:%d -f rawvideo -pix_fmt rgb24 pipe:1", url, ART_SIZE, ART_SIZE);
                        FILE *p = popen(cmd, "r");
                        if(p) { fread(art_pixels, 1, ART_SIZE*ART_SIZE*3, p); pclose(p); }
                    }
                }
            }
            char *art_ptr = strstr(item_ptr, "\"artists\"");
            if (art_ptr) quick_parse(art_ptr, "name", a);
            char *dur_ptr = strstr(item_ptr, "\"duration_ms\"");
            if (dur_ptr) quick_parse(dur_ptr, "name", t);
        } else { spotify_active = 0; }
        free(chunk.memory); curl_slist_free_all(headers); curl_easy_cleanup(curl);
    }
}

void hsv_to_rgb(float h, unsigned char *r, unsigned char *g, unsigned char *b) {
    float s=1.0, v=1.0;
    int i = floor(h*6); float f = h*6-i, p = v*(1-s), q = v*(1-f*s), t = v*(1-(1-f)*s);
    switch(i%6){
        case 0: *r=v*255;*g=t*255;*b=p*255;break; case 1: *r=q*255;*g=v*255;*b=p*255;break;
        case 2: *r=p*255;*g=v*255;*b=t*255;break; case 3: *r=p*255;*g=q*255;*b=v*255;break;
        case 4: *r=t*255;*g=p*255;*b=v*255;break; case 5: *r=v*255;*g=p*255;*b=q*255;break;
    }
}

void draw_str(unsigned char *buf, int x, int y, const char *s, int xr, int yr, int ll, int bpp, int sc, unsigned char r, unsigned char g, unsigned char b) {
    while (*s) {
        char c = (*s >= 'a' && *s <= 'z') ? *s - 32 : *s;
        if (c >= 0 && c < 128) {
            for (int i=0; i<8; i++) for (int j=0; j<8; j++) if (font_ascii[(int)c][i] & (1<<(7-j)))
                for (int yy=0; yy<sc; yy++) for (int xx=0; xx<sc; xx++) {
                    int px = x+(j*sc)+xx, py = y+(i*sc)+yy;
                    if (px>=0 && px<xr && py>=0 && py<yr) {
                        long lo = (px*bpp)+(py*ll); buf[lo+0]=b; buf[lo+1]=g; buf[lo+2]=r;
                    }
                }
        }
        x += (8*sc+sc); s++;
    }
}

int main() {
    printf("\e[?25l");
    fflush(stdout);
    load_rc();
    curl_global_init(CURL_GLOBAL_ALL); refresh_spotify_token();
    int fb = open("/dev/fb0", O_RDWR);
    struct fb_var_screeninfo vi; struct fb_fix_screeninfo fi;
    ioctl(fb, FBIOGET_VSCREENINFO, &vi); ioctl(fb, FBIOGET_FSCREENINFO, &fi);
    long sz = vi.yres_virtual * fi.line_length;
    unsigned char *fbp = mmap(0, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fb, 0);
    unsigned char *bb = malloc(sz);
    int bpp = vi.bits_per_pixel/8;
    snd_pcm_t *h; snd_pcm_open(&h, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    snd_pcm_set_params(h, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 44100, 1, 100000);
    short audio_buf[SAMPLES]; double *fft_in = fftw_alloc_real(SAMPLES);
    fftw_complex *fft_out = fftw_alloc_complex(SAMPLES/2+1);
    fftw_plan fft_p = fftw_plan_dft_r2c_1d(SAMPLES, fft_in, fft_out, FFTW_ESTIMATE);
    set_conio_terminal_mode(); atexit(reset_terminal_mode);
    struct pollfd pfd = {0, POLLIN, 0};
    char t[256]="-", a[256]="-", al[256]="-";
    int st=0, tt=0; float hue=0;
    while (1) {
        if (poll(&pfd, 1, 0) > 0) {
            char ch; if (read(0, &ch, 1) > 0) {
                if (ch == 27) break;
                if (ch == '+') { font_scale++; save_rc(); }
                if (ch == '-' && font_scale > 1) { font_scale--; save_rc(); }
                if (ch == 'h' || ch == 'H') { show_clock = !show_clock; save_rc(); }
                if (ch == 's' || ch == 'S') { show_spotify = !show_spotify; save_rc(); }
            }
        }
        if (snd_pcm_readi(h, audio_buf, SAMPLES) != SAMPLES) { snd_pcm_prepare(h); continue; }
        for (int i=0; i<SAMPLES; i++) fft_in[i] = (audio_buf[i]/32768.0) * (0.5*(1-cos(2*M_PI*i/(SAMPLES-1))));
        fftw_execute(fft_p); memset(bb, 0, sz);
        int mid_y = vi.yres / 2;
        int num_bars = vi.xres / (LED_SIZE + LED_GAP);
        for (int i=0; i<num_bars && i<SAMPLES/2; i++) {
            double mag = sqrt(fft_out[i][0]*fft_out[i][0] + fft_out[i][1]*fft_out[i][1]);
            int b_h = (int)(log10(mag+1.0)*MAX_HEIGHT_SCALE*(mid_y/100.0));
            float hr = (float)i/num_bars;
            unsigned char rb, gb, bb_c;
            if (hr<0.5) { float l=hr/0.5; rb=255; gb=(unsigned char)(255*(1.0-l)); bb_c=0; }
            else { float l=(hr-0.5)/0.5; rb=(unsigned char)(255*(1.0-l)); gb=0; bb_c=(unsigned char)(255*l); }
            int sx = i*(LED_SIZE+LED_GAP);
            for (int h_idx=0; h_idx<b_h; h_idx+=(LED_SIZE+LED_GAP)) {
                int yu = mid_y-h_idx-LED_SIZE, yd = mid_y+h_idx;
                for (int dy=0; dy<LED_SIZE; dy++) for (int dx=0; dx<LED_SIZE; dx++) {
                    int fx = sx+dx; if (fx>=vi.xres) continue;
                    if (yu+dy>=0) { long lo=(fx*bpp)+((yu+dy)*fi.line_length); bb[lo+0]=bb_c; bb[lo+1]=gb; bb[lo+2]=rb; }
                    if (yd+dy<vi.yres) { long lo=(fx*bpp)+((yd+dy)*fi.line_length); bb[lo+0]=bb_c; bb[lo+1]=gb; bb[lo+2]=rb; }
                }
            }
        }
        if (tt++ > 4000) { refresh_spotify_token(); tt=0; }
        if (st++ > 150) { update_spotify_api(t, a, al); st=0; }
        if (show_spotify && spotify_active) {
            for (int y=0; y<150; y++) for (int x=0; x<150; x++) {
                long lo = ((x+20)*bpp)+((y+20)*fi.line_length);
                int idx = (y*150+x)*3;
                bb[lo+0]=art_pixels[idx+2]; bb[lo+1]=art_pixels[idx+1]; bb[lo+2]=art_pixels[idx+0];
            }
            draw_str(bb, 20+150+20, 30, t, vi.xres, vi.yres, fi.line_length, bpp, 2, 255, 255, 255);
            draw_str(bb, 20+150+20, 65, a, vi.xres, vi.yres, fi.line_length, bpp, 1, 200, 200, 200);
            draw_str(bb, 20+150+20, 90, al, vi.xres, vi.yres, fi.line_length, bpp, 1, 150, 150, 150);
        }
        if (show_clock) {
            time_t raw; time(&raw); struct tm *ti = localtime(&raw);
            char clk[10]; sprintf(clk, "%02d:%02d", ti->tm_hour, ti->tm_min);
            unsigned char r,g,b; hsv_to_rgb(hue, &r, &g, &b); hue+=0.002; if(hue>1) hue=0;
            int cw = 5 * (8 * font_scale + font_scale); 
            draw_str(bb, vi.xres - cw - 20, 20, clk, vi.xres, vi.yres, fi.line_length, bpp, font_scale, r, g, b);
        }
        memcpy(fbp, bb, sz);
    }
    return 0;
}
