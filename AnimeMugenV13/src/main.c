/* Anime MUGEN V13 - PSP edition, v0.1 (software-rendered 2D fighter) */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("AnimeMugenV13", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define FB_SIZE (STRIDE * SH * 4)
#define VRAM ((unsigned char *)0x44000000)
#define MAXCH 512
#define MAXST 64
#define GROUND 220

typedef unsigned int u32c;
typedef struct { char name[32]; u32c color; int hp, power, speed; } Char;
typedef struct { char name[32]; u32c sky, ground; } Stage;
typedef struct {
    float x, y, vy; int dir, hp, maxhp, power, atk, cd, guard, hurt;
    float en, speed, px; int pon, pdir, pdmg, pw; u32c color;
} Fighter;

enum { S_CHAR, S_STAGE, S_VS, S_FIGHT, S_RESULT };

static Char chars[MAXCH]; static int nchars = 0;
static Stage stages[MAXST]; static int nstages = 0;
static char basedir[256];
static int running = 1, cur = 0, draw = 1;
static unsigned int back_off, front_off;

static int exit_cb(int a, int b, void *c) { running = 0; sceKernelExitGame(); return 0; }
static int cb_thread(SceSize args, void *argp) {
    int id = sceKernelCreateCallback("exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_cb(void) {
    int t = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (t >= 0) sceKernelStartThread(t, 0, NULL);
}

/* ---------- drawing ---------- */
static void rect(int x, int y, int w, int h, u32c col) {
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SW) w = SW - x;
    if (y + h > SH) h = SH - y;
    if (w <= 0 || h <= 0) return;
    for (j = 0; j < h; j++) {
        u32c *p = (u32c *)(VRAM + back_off) + (y + j) * STRIDE + x;
        for (i = 0; i < w; i++) p[i] = col;
    }
}
static void text(int cx, int cy, const char *s) { /* cx,cy in 7px cells */
    pspDebugScreenSetXY(cx, cy);
    pspDebugScreenPrintf("%s", s);
}
static void flip(void) {
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf(VRAM + back_off, STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    { unsigned int t = back_off; back_off = front_off; front_off = t; }
    pspDebugScreenSetOffset(back_off);
}
static void clear(u32c c) { rect(0, 0, SW, SH, c); }

/* ---------- data loading ---------- */
static u32c hex2abgr(const char *s) {
    unsigned v = (unsigned)strtoul(s, NULL, 16);
    return 0xFF000000u | ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);
}
static int load_kv(const char *path, char keys[][32], char vals[][64], int max) {
    FILE *f = fopen(path, "r"); char line[128]; int n = 0;
    if (!f) return 0;
    while (n < max && fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='); char *nl;
        if (!eq) continue;
        *eq = 0; eq++;
        nl = strpbrk(eq, "\r\n"); if (nl) *nl = 0;
        strncpy(keys[n], line, 31); keys[n][31] = 0;
        strncpy(vals[n], eq, 63); vals[n][63] = 0;
        n++;
    }
    fclose(f);
    return n;
}
static const char *kv(char keys[][32], char vals[][64], int n, const char *k, const char *def) {
    int i; for (i = 0; i < n; i++) if (!strcmp(keys[i], k)) return vals[i];
    return def;
}
static int list_dirs(const char *path, char out[][64], int max) {
    SceUID d = sceIoDopen(path); SceIoDirent e; int n = 0;
    if (d < 0) return 0;
    memset(&e, 0, sizeof e);
    while (n < max && sceIoDread(d, &e) > 0) {
        if (FIO_S_ISDIR(e.d_stat.st_mode) && e.d_name[0] != '.') {
            strncpy(out[n], e.d_name, 63); out[n][63] = 0; n++;
        }
        memset(&e, 0, sizeof e);
    }
    sceIoDclose(d);
    return n;
}
static void scan_all(void) {
    static char names[MAXCH][64]; char path[320], k[8][32], v[8][64]; int i, n, c;
    nchars = 0; nstages = 0;
    snprintf(path, sizeof path, "%scharacters", basedir);
    n = list_dirs(path, names, MAXCH);
    for (i = 0; i < n; i++) {
        snprintf(path, sizeof path, "%scharacters/%s/char.txt", basedir, names[i]);
        c = load_kv(path, k, v, 8);
        if (!c) continue;
        strncpy(chars[nchars].name, kv(k, v, c, "name", names[i]), 31);
        chars[nchars].name[31] = 0;
        chars[nchars].color = hex2abgr(kv(k, v, c, "color", "FFFFFF"));
        chars[nchars].hp = atoi(kv(k, v, c, "hp", "1000"));
        chars[nchars].power = atoi(kv(k, v, c, "power", "10"));
        chars[nchars].speed = atoi(kv(k, v, c, "speed", "3"));
        if (chars[nchars].hp < 100) chars[nchars].hp = 100;
        if (chars[nchars].speed < 1) chars[nchars].speed = 1;
        nchars++;
    }
    snprintf(path, sizeof path, "%sstages", basedir);
    n = list_dirs(path, names, MAXST);
    for (i = 0; i < n; i++) {
        snprintf(path, sizeof path, "%sstages/%s/stage.txt", basedir, names[i]);
        c = load_kv(path, k, v, 8);
        if (!c) continue;
        strncpy(stages[nstages].name, kv(k, v, c, "name", names[i]), 31);
        stages[nstages].name[31] = 0;
        stages[nstages].sky = hex2abgr(kv(k, v, c, "sky", "3A6EA5"));
        stages[nstages].ground = hex2abgr(kv(k, v, c, "ground", "553311"));
        nstages++;
    }
    if (nchars == 0) { /* built-in fallback */
        strcpy(chars[0].name, "Fallback"); chars[0].color = 0xFF40C0FF;
        chars[0].hp = 1000; chars[0].power = 10; chars[0].speed = 3; nchars = 1;
    }
    if (nstages == 0) {
        strcpy(stages[0].name, "Fallback"); stages[0].sky = 0xFFA56E3A;
        stages[0].ground = 0xFF113355; nstages = 1;
    }
}

/* ---------- fighting ---------- */
static Fighter f[2]; static int ch[2], stg = 0, winner = 0, timer = 0;

static void init_fighter(Fighter *p, const Char *c, float x, int dir) {
    memset(p, 0, sizeof *p);
    p->x = x; p->y = GROUND; p->dir = dir; p->hp = p->maxhp = c->hp;
    p->power = c->power; p->speed = (float)c->speed; p->color = c->color;
}
static void damage(Fighter *t, Fighter *a, int dmg) {
    if (t->guard) dmg /= 5;
    t->hp -= dmg; if (t->hp < 0) t->hp = 0;
    if (!t->guard) t->hurt = 10;
    t->x += a->dir * (t->guard ? 3 : 10);
    a->en += 6; if (a->en > 100) a->en = 100;
}
static void fire(Fighter *p, int dmg, int w) {
    if (p->pon) return;
    p->pon = 1; p->px = p->x + p->dir * 20; p->pdir = p->dir; p->pdmg = dmg; p->pw = w;
}
static void act(Fighter *p, Fighter *o, int left, int right, int jump, int hit, int guard, int spec, int ult) {
    if (p->hurt > 0) { p->hurt--; } else {
        p->guard = guard && p->y >= GROUND;
        if (!p->guard) {
            if (left) { p->x -= p->speed; p->dir = -1; }
            if (right) { p->x += p->speed; p->dir = 1; }
            if (jump && p->y >= GROUND) p->vy = -9;
            if (hit && p->atk == 0 && p->cd == 0) { p->atk = 12; p->cd = 18; }
            if (spec && p->en >= 25 && !p->pon) { p->en -= 25; fire(p, p->power * 2, 10); }
            if (ult && p->en >= 100 && !p->pon) { p->en = 0; fire(p, p->power * 6, 40); }
        }
    }
    p->y += p->vy; p->vy += 0.6f;
    if (p->y >= GROUND) { p->y = GROUND; p->vy = 0; }
    if (p->x < 12) p->x = 12;
    if (p->x > SW - 12) p->x = SW - 12;
    if (p->cd > 0) p->cd--;
    if (p->atk > 0) {
        p->atk--;
        if (p->atk == 8 && (o->x - p->x) * p->dir > 0 && (o->x - p->x) * p->dir < 46 &&
            (o->y - p->y) < 30 && (o->y - p->y) > -30) damage(o, p, p->power);
    }
    if (p->pon) {
        p->px += p->pdir * 6;
        if (p->px < 0 || p->px > SW) p->pon = 0;
        else if (abs((int)(p->px - o->x)) < 14 + p->pw / 2 && abs((int)(p->y - o->y)) < 40) {
            damage(o, p, p->pdmg); p->pon = 0;
        }
    }
    p->en += 0.05f; if (p->en > 100) p->en = 100;
}
static void draw_fighter(const Fighter *p) {
    int x = (int)p->x, y = (int)p->y;
    u32c c = p->hurt > 0 ? 0xFFFFFFFF : p->color;
    rect(x - 10, y - 44, 20, 44, c);
    rect(x - 6, y - 58, 12, 12, c);
    rect(x + p->dir * 4 - 2, y - 54, 4, 4, 0xFF000000);
    if (p->guard) rect(x + p->dir * 14 - 2, y - 46, 4, 40, 0xFFFFFF40);
    if (p->atk > 4) rect(x + (p->dir > 0 ? 10 : -34), y - 34, 24, 8, c);
    if (p->pon) rect((int)p->px - p->pw / 2, y - 30 - p->pw / 4, p->pw, p->pw / 2 + 6, 0xFF40FFFF);
}
static void bar(int x, int y, int w, int h, float frac, u32c col, int right) {
    int fw = (int)(w * (frac < 0 ? 0 : frac));
    rect(x - 1, y - 1, w + 2, h + 2, 0xFFFFFFFF);
    rect(x, y, w, h, 0xFF202020);
    if (right) rect(x + w - fw, y, fw, h, col); else rect(x, y, fw, h, col);
}

/* ---------- main ---------- */
int main(int argc, char *argv[]) {
    SceCtrlData pad; unsigned prev = 0, pressed; int state = S_CHAR, pick = 0, i, vst = 0;
    int cols = 24, cw = 19, ch_ = 19;
    setup_cb();
    pspDebugScreenInit();
    front_off = 0; back_off = FB_SIZE;
    pspDebugScreenSetOffset(back_off);
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    strncpy(basedir, argc > 0 && argv[0] ? argv[0] : "", sizeof basedir - 1);
    basedir[sizeof basedir - 1] = 0;
    { char *s = strrchr(basedir, '/'); if (s) s[1] = 0; else basedir[0] = 0; }
    scan_all();
    srand(sceKernelLibcTime(NULL));

    while (running) {
        int L, R, U, D;
        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev; prev = pad.Buttons;
        L = (pad.Buttons & PSP_CTRL_LEFT) || pad.Lx < 64;
        R = (pad.Buttons & PSP_CTRL_RIGHT) || pad.Lx > 192;
        U = (pad.Buttons & PSP_CTRL_UP); D = (pad.Buttons & PSP_CTRL_DOWN);

        if (state == S_CHAR) {
            int rows, scroll, first, r, c;
            if (pressed & PSP_CTRL_LEFT && cur > 0) cur--;
            if (pressed & PSP_CTRL_RIGHT && cur < nchars - 1) cur++;
            if (pressed & PSP_CTRL_UP && cur >= cols) cur -= cols;
            if (pressed & PSP_CTRL_DOWN && cur + cols < nchars) cur += cols;
            if (pressed & PSP_CTRL_CROSS) {
                ch[pick] = cur;
                if (pick == 0) { pick = 1; } else { pick = 0; state = S_STAGE; }
            }
            if ((pressed & PSP_CTRL_CIRCLE) && pick == 1) pick = 0;
            clear(0xFF101018);
            rect(0, 0, SW, 22, 0xFF301060);
            rows = 9; scroll = (cur / cols) - rows / 2; if (scroll < 0) scroll = 0;
            first = scroll * cols;
            for (i = first; i < nchars && i < first + rows * cols; i++) {
                r = (i - first) / cols; c = (i - first) % cols;
                rect(8 + c * cw, 28 + r * ch_, cw - 2, ch_ - 2, chars[i].color);
                if (i == cur) rect(8 + c * cw - 2, 28 + r * ch_ - 2, cw + 2, 3, 0xFFFFFFFF),
                              rect(8 + c * cw - 2, 28 + r * ch_ + ch_ - 3, cw + 2, 3, 0xFFFFFFFF);
            }
            rect(10, 215, 100, 50, chars[cur].color);
            text(0, 0, "ANIME MUGEN V13  -  CHON NHAN VAT");
            text(0, 1, pick == 0 ? "" : "");
            { char b[96]; snprintf(b, sizeof b, "%s  (%d/%d)", chars[cur].name, cur + 1, nchars); text(18, 27, b);
              snprintf(b, sizeof b, "HP %d  ATK %d  SPD %d", chars[cur].hp, chars[cur].power, chars[cur].speed); text(18, 29, b);
              text(18, 31, pick == 0 ? "P1: chon bang X" : "P2 (CPU): chon bang X, O = quay lai"); }
            flip();
        } else if (state == S_STAGE) {
            if (pressed & PSP_CTRL_LEFT && stg > 0) stg--;
            if (pressed & PSP_CTRL_RIGHT && stg < nstages - 1) stg++;
            if (pressed & PSP_CTRL_CIRCLE) state = S_CHAR;
            if (pressed & PSP_CTRL_CROSS) { state = S_VS; vst = 0; }
            clear(stages[stg].sky); rect(0, 180, SW, 92, stages[stg].ground);
            text(0, 0, "CHON SAN DAU  (trai/phai, X = OK, O = quay lai)");
            { char b[64]; snprintf(b, sizeof b, "%s (%d/%d)", stages[stg].name, stg + 1, nstages); text(2, 30, b); }
            flip();
        } else if (state == S_VS) {
            int slide = vst < 30 ? (30 - vst) * 10 : 0;
            clear(0xFF000000);
            rect(-slide, 60, 200, 150, chars[ch[0]].color);
            rect(SW - 200 + slide, 60, 200, 150, chars[ch[1]].color);
            rect(SW / 2 - 4, 40, 8, 190, 0xFFFFFFFF);
            text(8, 5, chars[ch[0]].name); text(40, 5, chars[ch[1]].name);
            text(31, 16, "VS");
            vst++;
            if (vst > 90 || (pressed & PSP_CTRL_START)) {
                init_fighter(&f[0], &chars[ch[0]], 120, 1);
                init_fighter(&f[1], &chars[ch[1]], 360, -1);
                state = S_FIGHT; timer = 99 * 60;
            }
            flip();
        } else if (state == S_FIGHT) {
            int cl = 0, cr = 0, cj = 0, ch2 = 0, cg = 0, cs = 0, cu = 0, dx;
            act(&f[0], &f[1], L, R, (pad.Buttons & PSP_CTRL_CROSS) != 0, (pressed & PSP_CTRL_SQUARE) != 0,
                (pad.Buttons & PSP_CTRL_CIRCLE) != 0, (pressed & PSP_CTRL_TRIANGLE) != 0, (pressed & PSP_CTRL_RTRIGGER) != 0);
            dx = (int)(f[0].x - f[1].x);
            if (abs(dx) > 55) { if (dx > 0) cl = 1; else cr = 1; }
            else { int r = rand() % 60; if (r < 4) ch2 = 1; else if (r < 8) cg = 1; else if (r < 9) cj = 1; }
            if (abs(dx) > 140 && rand() % 90 == 0) cs = 1;
            if (f[1].en >= 100 && rand() % 40 == 0) cu = 1;
            act(&f[1], &f[0], cl, cr, cj, ch2, cg, cs, cu);
            if (!(f[0].hurt > 0)) f[0].dir = (f[1].x > f[0].x) ? 1 : -1;
            if (!(f[1].hurt > 0)) f[1].dir = (f[0].x > f[1].x) ? 1 : -1;
            timer--;
            if (f[0].hp <= 0 || f[1].hp <= 0 || timer <= 0) {
                winner = f[0].hp >= f[1].hp ? 0 : 1; state = S_RESULT;
            }
            clear(stages[stg].sky); rect(0, GROUND, SW, SH - GROUND, stages[stg].ground);
            draw_fighter(&f[0]); draw_fighter(&f[1]);
            bar(10, 8, 200, 10, (float)f[0].hp / f[0].maxhp, 0xFF40E040, 0);
            bar(270, 8, 200, 10, (float)f[1].hp / f[1].maxhp, 0xFF40E040, 1);
            bar(10, 24, 120, 6, f[0].en / 100.0f, 0xFF40C0FF, 0);
            bar(350, 24, 120, 6, f[1].en / 100.0f, 0xFF40C0FF, 1);
            { char b[16]; snprintf(b, sizeof b, "%02d", timer / 60); text(33, 0, b); }
            text(0, 3, chars[ch[0]].name); text(48, 3, chars[ch[1]].name);
            flip();
        } else { /* S_RESULT */
            clear(0xFF000000);
            { char b[64]; snprintf(b, sizeof b, "%s THANG!", chars[ch[winner]].name); text(22, 14, b); }
            text(16, 17, "Nhan X de choi lai");
            flip();
            if (pressed & PSP_CTRL_CROSS) { state = S_CHAR; pick = 0; scan_all(); }
        }
        (void)U; (void)D; (void)draw;
    }
    sceKernelExitGame();
    return 0;
}
