/*
 * ligui-input: low-level Windows desktop input for Ligui.
 *
 * The primary backend is the Interception kernel filter driver
 * (oblitum/Interception), loaded dynamically from interception.dll. It sends
 * scan-code key strokes and raw mouse strokes through the keyboard/mouse class
 * stack, which is what applications that ignore SendInput accept. When the
 * driver or DLL is unavailable, every action falls back to SendInput.
 *
 * This is user-mode code: no driver is built, installed, or patched here.
 * ABI definitions are from the Interception public header (LGPL-3.0); the DLL
 * is not linked or redistributed by this source file.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Interception ABI                                                     */
/* ------------------------------------------------------------------ */

typedef void *InterceptionContext;
typedef int InterceptionDevice;

typedef struct {
    unsigned short state;
    unsigned short flags;
    short rolling;
    int x;
    int y;
    unsigned int information;
} InterceptionMouseStroke;

typedef struct {
    unsigned short code;
    unsigned short state;
    unsigned int information;
} InterceptionKeyStroke;

#define INTERCEPTION_KEY_DOWN 0x00
#define INTERCEPTION_KEY_UP 0x01
#define INTERCEPTION_KEY_E0 0x02

#define INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN 0x001
#define INTERCEPTION_MOUSE_LEFT_BUTTON_UP 0x002
#define INTERCEPTION_MOUSE_RIGHT_BUTTON_DOWN 0x004
#define INTERCEPTION_MOUSE_RIGHT_BUTTON_UP 0x008
#define INTERCEPTION_MOUSE_MIDDLE_BUTTON_DOWN 0x010
#define INTERCEPTION_MOUSE_MIDDLE_BUTTON_UP 0x020
#define INTERCEPTION_MOUSE_BUTTON_4_DOWN 0x040
#define INTERCEPTION_MOUSE_BUTTON_4_UP 0x080
#define INTERCEPTION_MOUSE_BUTTON_5_DOWN 0x100
#define INTERCEPTION_MOUSE_BUTTON_5_UP 0x200
#define INTERCEPTION_MOUSE_WHEEL 0x400
#define INTERCEPTION_MOUSE_HWHEEL 0x800

#define INTERCEPTION_MOUSE_MOVE_RELATIVE 0x000
#define INTERCEPTION_MOUSE_MOVE_ABSOLUTE 0x001

typedef InterceptionContext(__cdecl *PInterceptionCreateContext)(void);
typedef void(__cdecl *PInterceptionDestroyContext)(InterceptionContext);
typedef int(__cdecl *PInterceptionIsKeyboard)(InterceptionDevice);
typedef int(__cdecl *PInterceptionIsMouse)(InterceptionDevice);
typedef int(__cdecl *PInterceptionSend)(InterceptionContext, InterceptionDevice, const void *, unsigned int);
typedef unsigned int(__cdecl *PInterceptionGetHardwareId)(InterceptionContext, InterceptionDevice, void *, unsigned int);

static HMODULE g_dll = NULL;
static InterceptionContext g_ctx = NULL;
static int g_keyboard = 0;
static int g_mouse = 0;
static wchar_t g_dll_path[MAX_PATH * 2] = L"";
static PInterceptionDestroyContext g_destroy = NULL;
static PInterceptionIsKeyboard g_is_keyboard = NULL;
static PInterceptionIsMouse g_is_mouse = NULL;
static PInterceptionSend g_send = NULL;
static PInterceptionGetHardwareId g_get_hwid = NULL;

/* ------------------------------------------------------------------ */
/* JSON buffer                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_init(Buf *b) {
    b->cap = 4096;
    b->len = 0;
    b->data = (char *)malloc(b->cap);
    if (b->data) b->data[0] = 0;
}

static void buf_free(Buf *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static void buf_reserve(Buf *b, size_t extra) {
    if (!b->data) return;
    if (b->len + extra + 1 <= b->cap) return;
    while (b->len + extra + 1 > b->cap) b->cap *= 2;
    b->data = (char *)realloc(b->data, b->cap);
}

static void buf_addn(Buf *b, const char *s, size_t n) {
    buf_reserve(b, n);
    if (!b->data) return;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

static void buf_add(Buf *b, const char *s) {
    buf_addn(b, s, strlen(s));
}

static void buf_printf(Buf *b, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) buf_addn(b, tmp, (size_t)n < sizeof(tmp) ? (size_t)n : sizeof(tmp) - 1);
}

static void json_escape_utf8(Buf *b, const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        switch (*p) {
            case '"': buf_add(b, "\\\""); break;
            case 0x5C: { char e[3] = {0x5C, 0x5C, 0}; buf_add(b, e); break; }
            case 0x08: { char e[3] = {0x5C, 'b', 0}; buf_add(b, e); break; }
            case 0x0C: { char e[3] = {0x5C, 'f', 0}; buf_add(b, e); break; }
            case 0x0A: { char e[3] = {0x5C, 'n', 0}; buf_add(b, e); break; }
            case 0x0D: { char e[3] = {0x5C, 'r', 0}; buf_add(b, e); break; }
            case 0x09: { char e[3] = {0x5C, 't', 0}; buf_add(b, e); break; }
            default:
                if (*p < 0x20) buf_printf(b, "%cu%04x", 0x5C, *p);
                else buf_addn(b, (const char *)p, 1);
        }
    }
}

static void json_w(Buf *b, const wchar_t *w) {
    if (!w) { buf_add(b, "\"\""); return; }
    int bytes = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) { buf_add(b, "\"\""); return; }
    char *utf8 = (char *)malloc((size_t)bytes);
    if (!utf8) { buf_add(b, "\"\""); return; }
    WideCharToMultiByte(CP_UTF8, 0, w, -1, utf8, bytes, NULL, NULL);
    buf_add(b, "\"");
    json_escape_utf8(b, utf8);
    buf_add(b, "\"");
    free(utf8);
}

static int failf(const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":false,\"error\":\"");
    json_escape_utf8(&b, msg);
    buf_add(&b, "\"}\n");
    fputs(b.data ? b.data : "{\"ok\":false}\n", stdout);
    buf_free(&b);
    return 1;
}

static int succeed(Buf *b) {
    fputs(b->data ? b->data : "{\"ok\":true}\n", stdout);
    buf_free(b);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Driver loading                                                       */
/* ------------------------------------------------------------------ */

static int load_driver(void) {
    if (g_ctx) return 1;

    const wchar_t *env = _wgetenv(L"LIGUI_INTERCEPTION_DLL");
    if (env && *env) {
        g_dll = LoadLibraryW(env);
        if (g_dll) wcsncpy(g_dll_path, env, MAX_PATH * 2 - 1);
    }

    if (!g_dll) {
        wchar_t exe[MAX_PATH * 2];
        DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH * 2);
        if (n > 0 && n < MAX_PATH * 2) {
            wchar_t *slash = wcsrchr(exe, (wchar_t)0x5C);
            if (slash) {
                *slash = 0;
                wchar_t candidate[MAX_PATH * 2];
                _snwprintf(candidate, MAX_PATH * 2, L"%s%cinterception.dll", exe, (wchar_t)0x5C);
                g_dll = LoadLibraryW(candidate);
                if (g_dll) wcsncpy(g_dll_path, candidate, MAX_PATH * 2 - 1);
            }
        }
    }

    if (!g_dll) {
        g_dll = LoadLibraryW(L"interception.dll");
        if (g_dll) wcsncpy(g_dll_path, L"interception.dll", MAX_PATH * 2 - 1);
    }

    if (!g_dll) return 0;

    PInterceptionCreateContext create = (PInterceptionCreateContext)GetProcAddress(g_dll, "interception_create_context");
    g_destroy = (PInterceptionDestroyContext)GetProcAddress(g_dll, "interception_destroy_context");
    g_is_keyboard = (PInterceptionIsKeyboard)GetProcAddress(g_dll, "interception_is_keyboard");
    g_is_mouse = (PInterceptionIsMouse)GetProcAddress(g_dll, "interception_is_mouse");
    g_send = (PInterceptionSend)GetProcAddress(g_dll, "interception_send");
    g_get_hwid = (PInterceptionGetHardwareId)GetProcAddress(g_dll, "interception_get_hardware_id");

    if (!create || !g_destroy || !g_is_keyboard || !g_is_mouse || !g_send) {
        FreeLibrary(g_dll);
        g_dll = NULL;
        return 0;
    }

    g_ctx = create();
    if (!g_ctx) {
        FreeLibrary(g_dll);
        g_dll = NULL;
        return 0;
    }

    unsigned char hwid[512];
    for (int d = 1; d <= 20; ++d) {
        unsigned int n = g_get_hwid ? g_get_hwid(g_ctx, d, hwid, sizeof(hwid)) : 1;
        if (n == 0) continue;
        if (!g_keyboard && g_is_keyboard(d)) g_keyboard = d;
        if (!g_mouse && g_is_mouse(d)) g_mouse = d;
    }

    const wchar_t *kb = _wgetenv(L"LIGUI_INTERCEPTION_KEYBOARD");
    const wchar_t *ms = _wgetenv(L"LIGUI_INTERCEPTION_MOUSE");
    if (kb && *kb) g_keyboard = _wtoi(kb);
    if (ms && *ms) g_mouse = _wtoi(ms);

    if (!g_keyboard && !g_mouse) {
        g_destroy(g_ctx);
        g_ctx = NULL;
        FreeLibrary(g_dll);
        g_dll = NULL;
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Input primitives                                                     */
/* ------------------------------------------------------------------ */

static int send_key_stroke(unsigned short scan, int up, int e0) {
    if (scan == 0) return 0;
    if (g_ctx && g_keyboard) {
        InterceptionKeyStroke s;
        s.code = scan;
        s.state = (unsigned short)((up ? INTERCEPTION_KEY_UP : INTERCEPTION_KEY_DOWN) | (e0 ? INTERCEPTION_KEY_E0 : 0));
        s.information = 0;
        return g_send(g_ctx, g_keyboard, &s, 1) > 0;
    }
    INPUT in;
    ZeroMemory(&in, sizeof(in));
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0) | (e0 ? KEYEVENTF_EXTENDEDKEY : 0);
    return SendInput(1, &in, sizeof(in)) == 1;
}

static int send_mouse_stroke(unsigned short state, unsigned short flags, short rolling, int x, int y) {
    if (g_ctx && g_mouse) {
        InterceptionMouseStroke s;
        s.state = state;
        s.flags = flags;
        s.rolling = rolling;
        s.x = x;
        s.y = y;
        s.information = 0;
        return g_send(g_ctx, g_mouse, &s, 1) > 0;
    }

    INPUT in;
    ZeroMemory(&in, sizeof(in));
    in.type = INPUT_MOUSE;
    if (flags & INTERCEPTION_MOUSE_MOVE_ABSOLUTE) {
        in.mi.dwFlags |= MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        in.mi.dx = x;
        in.mi.dy = y;
    } else if (x || y) {
        in.mi.dwFlags |= MOUSEEVENTF_MOVE;
        in.mi.dx = x;
        in.mi.dy = y;
    }
    if (state & INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN) in.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
    if (state & INTERCEPTION_MOUSE_LEFT_BUTTON_UP) in.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
    if (state & INTERCEPTION_MOUSE_RIGHT_BUTTON_DOWN) in.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
    if (state & INTERCEPTION_MOUSE_RIGHT_BUTTON_UP) in.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
    if (state & INTERCEPTION_MOUSE_MIDDLE_BUTTON_DOWN) in.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
    if (state & INTERCEPTION_MOUSE_MIDDLE_BUTTON_UP) in.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
    if (state & INTERCEPTION_MOUSE_BUTTON_4_DOWN) { in.mi.dwFlags |= MOUSEEVENTF_XDOWN; in.mi.mouseData = XBUTTON1; }
    if (state & INTERCEPTION_MOUSE_BUTTON_4_UP) { in.mi.dwFlags |= MOUSEEVENTF_XUP; in.mi.mouseData = XBUTTON1; }
    if (state & INTERCEPTION_MOUSE_BUTTON_5_DOWN) { in.mi.dwFlags |= MOUSEEVENTF_XDOWN; in.mi.mouseData = XBUTTON2; }
    if (state & INTERCEPTION_MOUSE_BUTTON_5_UP) { in.mi.dwFlags |= MOUSEEVENTF_XUP; in.mi.mouseData = XBUTTON2; }
    if (state & INTERCEPTION_MOUSE_WHEEL) {
        in.mi.dwFlags |= MOUSEEVENTF_WHEEL;
        in.mi.mouseData = (DWORD)(short)rolling;
    }
    if (state & INTERCEPTION_MOUSE_HWHEEL) {
        in.mi.dwFlags |= MOUSEEVENTF_HWHEEL;
        in.mi.mouseData = (DWORD)(short)rolling;
    }
    if (in.mi.dwFlags == 0) return 0;
    return SendInput(1, &in, sizeof(in)) == 1;
}

/* ------------------------------------------------------------------ */
/* Key names and scan codes (set 1)                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    const wchar_t *name;
    unsigned short scan;
    unsigned short e0;
} KeyDef;

static const KeyDef KEYS[] = {
    {L"esc", 0x01, 0}, {L"escape", 0x01, 0},
    {L"1", 0x02, 0}, {L"2", 0x03, 0}, {L"3", 0x04, 0}, {L"4", 0x05, 0}, {L"5", 0x06, 0},
    {L"6", 0x07, 0}, {L"7", 0x08, 0}, {L"8", 0x09, 0}, {L"9", 0x0A, 0}, {L"0", 0x0B, 0},
    {L"-", 0x0C, 0}, {L"=", 0x0D, 0}, {L"backspace", 0x0E, 0}, {L"bksp", 0x0E, 0},
    {L"tab", 0x0F, 0},
    {L"q", 0x10, 0}, {L"w", 0x11, 0}, {L"e", 0x12, 0}, {L"r", 0x13, 0}, {L"t", 0x14, 0},
    {L"y", 0x15, 0}, {L"u", 0x16, 0}, {L"i", 0x17, 0}, {L"o", 0x18, 0}, {L"p", 0x19, 0},
    {L"[", 0x1A, 0}, {L"]", 0x1B, 0}, {L"enter", 0x1C, 0}, {L"return", 0x1C, 0},
    {L"ctrl", 0x1D, 0}, {L"control", 0x1D, 0}, {L"lctrl", 0x1D, 0}, {L"rctrl", 0x1D, 1},
    {L"a", 0x1E, 0}, {L"s", 0x1F, 0}, {L"d", 0x20, 0}, {L"f", 0x21, 0}, {L"g", 0x22, 0},
    {L"h", 0x23, 0}, {L"j", 0x24, 0}, {L"k", 0x25, 0}, {L"l", 0x26, 0},
    {L";", 0x27, 0}, {L"'", 0x28, 0}, {L"`", 0x29, 0}, {L"grave", 0x29, 0},
    {L"shift", 0x2A, 0}, {L"lshift", 0x2A, 0}, {L"rshift", 0x36, 0},
    {L"\\", 0x2B, 0},
    {L"z", 0x2C, 0}, {L"x", 0x2D, 0}, {L"c", 0x2E, 0}, {L"v", 0x2F, 0}, {L"b", 0x30, 0},
    {L"n", 0x31, 0}, {L"m", 0x32, 0}, {L",", 0x33, 0}, {L".", 0x34, 0}, {L"/", 0x35, 0},
    {L"numpad*", 0x37, 0}, {L"numpad_multiply", 0x37, 0},
    {L"alt", 0x38, 0}, {L"lalt", 0x38, 0}, {L"ralt", 0x38, 1},
    {L"space", 0x39, 0}, {L"capslock", 0x3A, 0}, {L"caps", 0x3A, 0},
    {L"f1", 0x3B, 0}, {L"f2", 0x3C, 0}, {L"f3", 0x3D, 0}, {L"f4", 0x3E, 0}, {L"f5", 0x3F, 0},
    {L"f6", 0x40, 0}, {L"f7", 0x41, 0}, {L"f8", 0x42, 0}, {L"f9", 0x43, 0}, {L"f10", 0x44, 0},
    {L"numlock", 0x45, 0}, {L"scrolllock", 0x46, 0},
    {L"numpad7", 0x47, 0}, {L"numpad8", 0x48, 0}, {L"numpad9", 0x49, 0},
    {L"numpad-", 0x4A, 0}, {L"numpad_subtract", 0x4A, 0},
    {L"numpad4", 0x4B, 0}, {L"numpad5", 0x4C, 0}, {L"numpad6", 0x4D, 0},
    {L"numpad+", 0x4E, 0}, {L"numpad_add", 0x4E, 0},
    {L"numpad1", 0x4F, 0}, {L"numpad2", 0x50, 0}, {L"numpad3", 0x51, 0},
    {L"numpad0", 0x52, 0}, {L"numpad.", 0x53, 0}, {L"numpad_decimal", 0x53, 0},
    {L"f11", 0x57, 0}, {L"f12", 0x58, 0},
    {L"insert", 0x52, 1}, {L"ins", 0x52, 1},
    {L"delete", 0x53, 1}, {L"del", 0x53, 1},
    {L"home", 0x47, 1}, {L"end", 0x4F, 1},
    {L"pageup", 0x49, 1}, {L"pgup", 0x49, 1},
    {L"pagedown", 0x51, 1}, {L"pgdn", 0x51, 1},
    {L"left", 0x4B, 1}, {L"right", 0x4D, 1}, {L"up", 0x48, 1}, {L"down", 0x50, 1},
    {L"numpadenter", 0x1C, 1}, {L"numpad/", 0x35, 1}, {L"numpad_divide", 0x35, 1},
    {L"lwin", 0x5B, 1}, {L"win", 0x5B, 1}, {L"rwin", 0x5C, 1},
    {L"menu", 0x5D, 1}, {L"apps", 0x5D, 1},
    {L"printscreen", 0x37, 1}, {L"prtsc", 0x37, 1},
};

static const KeyDef *lookup_key(const wchar_t *name) {
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); ++i) {
        if (_wcsicmp(KEYS[i].name, name) == 0) return &KEYS[i];
    }
    return NULL;
}

static int key_for_char(wchar_t ch, unsigned short *scan, unsigned short *e0, int *shift, int *ctrl, int *alt) {
    SHORT vk = VkKeyScanW(ch);
    if (vk == -1) return 0;
    BYTE v = LOBYTE(vk);
    BYTE mods = HIBYTE(vk);
    UINT sc = MapVirtualKeyW(v, MAPVK_VK_TO_VSC);
    if (!sc) return 0;
    *scan = (unsigned short)(sc & 0xFF);
    *e0 = 0;
    *shift = (mods & 1) != 0;
    *ctrl = (mods & 2) != 0;
    *alt = (mods & 4) != 0;
    return 1;
}

static int key_token(const wchar_t *name, unsigned short *scan, unsigned short *e0) {
    const KeyDef *k = lookup_key(name);
    if (k) {
        *scan = k->scan;
        *e0 = k->e0;
        return 1;
    }
    if (wcslen(name) == 1) {
        int sh, ct, al;
        return key_for_char(name[0], scan, e0, &sh, &ct, &al);
    }
    return 0;
}

static int modifier_down(const wchar_t *name) {
    if (!_wcsicmp(name, L"ctrl") || !_wcsicmp(name, L"control") || !_wcsicmp(name, L"lctrl")) return send_key_stroke(0x1D, 0, 0);
    if (!_wcsicmp(name, L"rctrl")) return send_key_stroke(0x1D, 0, 1);
    if (!_wcsicmp(name, L"shift") || !_wcsicmp(name, L"lshift")) return send_key_stroke(0x2A, 0, 0);
    if (!_wcsicmp(name, L"rshift")) return send_key_stroke(0x36, 0, 0);
    if (!_wcsicmp(name, L"alt") || !_wcsicmp(name, L"lalt")) return send_key_stroke(0x38, 0, 0);
    if (!_wcsicmp(name, L"ralt")) return send_key_stroke(0x38, 0, 1);
    if (!_wcsicmp(name, L"win") || !_wcsicmp(name, L"lwin")) return send_key_stroke(0x5B, 0, 1);
    if (!_wcsicmp(name, L"rwin")) return send_key_stroke(0x5C, 0, 1);
    return 0;
}

static int modifier_up(const wchar_t *name) {
    if (!_wcsicmp(name, L"ctrl") || !_wcsicmp(name, L"control") || !_wcsicmp(name, L"lctrl")) return send_key_stroke(0x1D, 1, 0);
    if (!_wcsicmp(name, L"rctrl")) return send_key_stroke(0x1D, 1, 1);
    if (!_wcsicmp(name, L"shift") || !_wcsicmp(name, L"lshift")) return send_key_stroke(0x2A, 1, 0);
    if (!_wcsicmp(name, L"rshift")) return send_key_stroke(0x36, 1, 0);
    if (!_wcsicmp(name, L"alt") || !_wcsicmp(name, L"lalt")) return send_key_stroke(0x38, 1, 0);
    if (!_wcsicmp(name, L"ralt")) return send_key_stroke(0x38, 1, 1);
    if (!_wcsicmp(name, L"win") || !_wcsicmp(name, L"lwin")) return send_key_stroke(0x5B, 1, 1);
    if (!_wcsicmp(name, L"rwin")) return send_key_stroke(0x5C, 1, 1);
    return 0;
}

static int press_named(const wchar_t *name) {
    unsigned short scan, e0;
    int sh, ct, al;
    const KeyDef *k = lookup_key(name);
    if (k) {
        scan = k->scan;
        e0 = k->e0;
        sh = ct = al = 0;
    } else if (wcslen(name) == 1 && key_for_char(name[0], &scan, &e0, &sh, &ct, &al)) {
        /* ready */
    } else {
        return 0;
    }
    if (ct) send_key_stroke(0x1D, 0, 0);
    if (sh) send_key_stroke(0x2A, 0, 0);
    if (al) send_key_stroke(0x38, 0, 0);
    send_key_stroke(scan, 0, e0);
    Sleep(25);
    send_key_stroke(scan, 1, e0);
    if (al) send_key_stroke(0x38, 1, 0);
    if (sh) send_key_stroke(0x2A, 1, 0);
    if (ct) send_key_stroke(0x1D, 1, 0);
    return 1;
}

static int press_chord(const wchar_t *chord) {
    wchar_t buf[256];
    wcsncpy(buf, chord, 255);
    buf[255] = 0;
    wchar_t *parts[8];
    int n = 0;
    wchar_t *p = buf;
    while (p && n < 8) {
        parts[n++] = p;
        p = wcschr(p, L'+');
        if (p) { *p = 0; ++p; }
    }
    if (n == 0) return 0;
    if (n == 1) return press_named(parts[0]);

    int mods = n - 1;
    for (int i = 0; i < mods; ++i) {
        if (!modifier_down(parts[i])) return 0;
    }
    unsigned short scan, e0;
    if (!key_token(parts[n - 1], &scan, &e0)) {
        for (int i = mods - 1; i >= 0; --i) modifier_up(parts[i]);
        return 0;
    }
    send_key_stroke(scan, 0, e0);
    Sleep(25);
    send_key_stroke(scan, 1, e0);
    for (int i = mods - 1; i >= 0; --i) modifier_up(parts[i]);
    return 1;
}

static int type_text(const wchar_t *text) {
    for (const wchar_t *p = text; *p; ++p) {
        wchar_t ch = *p;
        if (ch == L'\r') continue;
        if (ch == L'\n') { press_named(L"enter"); Sleep(8); continue; }
        if (ch == L'\t') { press_named(L"tab"); Sleep(8); continue; }

        unsigned short scan, e0;
        int sh, ct, al;
        if (key_for_char(ch, &scan, &e0, &sh, &ct, &al)) {
            if (ct) send_key_stroke(0x1D, 0, 0);
            if (sh) send_key_stroke(0x2A, 0, 0);
            if (al) send_key_stroke(0x38, 0, 0);
            send_key_stroke(scan, 0, e0);
            send_key_stroke(scan, 1, e0);
            if (al) send_key_stroke(0x38, 1, 0);
            if (sh) send_key_stroke(0x2A, 1, 0);
            if (ct) send_key_stroke(0x1D, 1, 0);
        } else {
            INPUT in[2];
            ZeroMemory(in, sizeof(in));
            in[0].type = INPUT_KEYBOARD;
            in[0].ki.wScan = ch;
            in[0].ki.dwFlags = KEYEVENTF_UNICODE;
            in[1] = in[0];
            in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            SendInput(2, in, sizeof(INPUT));
        }
        Sleep(2);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Mouse                                                               */
/* ------------------------------------------------------------------ */

static int button_bits(const wchar_t *name, int down, unsigned short *state) {
    if (!_wcsicmp(name, L"left") || !_wcsicmp(name, L"l")) {
        *state = down ? INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN : INTERCEPTION_MOUSE_LEFT_BUTTON_UP;
        return 1;
    }
    if (!_wcsicmp(name, L"right") || !_wcsicmp(name, L"r")) {
        *state = down ? INTERCEPTION_MOUSE_RIGHT_BUTTON_DOWN : INTERCEPTION_MOUSE_RIGHT_BUTTON_UP;
        return 1;
    }
    if (!_wcsicmp(name, L"middle") || !_wcsicmp(name, L"m")) {
        *state = down ? INTERCEPTION_MOUSE_MIDDLE_BUTTON_DOWN : INTERCEPTION_MOUSE_MIDDLE_BUTTON_UP;
        return 1;
    }
    if (!_wcsicmp(name, L"mouse4") || !_wcsicmp(name, L"x1")) {
        *state = down ? INTERCEPTION_MOUSE_BUTTON_4_DOWN : INTERCEPTION_MOUSE_BUTTON_4_UP;
        return 1;
    }
    if (!_wcsicmp(name, L"mouse5") || !_wcsicmp(name, L"x2")) {
        *state = down ? INTERCEPTION_MOUSE_BUTTON_5_DOWN : INTERCEPTION_MOUSE_BUTTON_5_UP;
        return 1;
    }
    return 0;
}

static int move_absolute(int x, int y) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    if (w < 2 || h < 2) return 0;
    int nx = (int)((double)x * 65535.0 / (double)(w - 1) + 0.5);
    int ny = (int)((double)y * 65535.0 / (double)(h - 1) + 0.5);
    return send_mouse_stroke(0, INTERCEPTION_MOUSE_MOVE_ABSOLUTE, 0, nx, ny);
}

static int move_relative(int dx, int dy) {
    int ax = abs(dx), ay = abs(dy);
    int maxd = ax > ay ? ax : ay;
    int steps = (maxd + 23) / 24;
    if (steps < 1) steps = 1;
    int px = 0, py = 0;
    for (int i = 1; i <= steps; ++i) {
        int tx = dx * i / steps;
        int ty = dy * i / steps;
        if (!send_mouse_stroke(0, INTERCEPTION_MOUSE_MOVE_RELATIVE, 0, tx - px, ty - py)) return 0;
        px = tx;
        py = ty;
        if (i < steps) Sleep(2);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Windows                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    Buf *b;
    int first;
    HWND foreground;
    const wchar_t *title_match;
    HWND found;
} EnumCtx;

static void window_json(Buf *b, HWND hwnd, HWND foreground) {
    wchar_t title[512];
    wchar_t cls[256];
    GetWindowTextW(hwnd, title, 512);
    GetClassNameW(hwnd, cls, 256);
    RECT r;
    GetWindowRect(hwnd, &r);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    int minimized = IsIconic(hwnd);

    buf_printf(b, "{\"hwnd\":\"0x%llX\",\"title\":", (unsigned long long)(ULONG_PTR)hwnd);
    json_w(b, title);
    buf_add(b, ",\"class\":");
    json_w(b, cls);
    buf_printf(b, ",\"pid\":%lu,\"x\":%ld,\"y\":%ld,\"w\":%ld,\"h\":%ld,\"minimized\":%s,\"foreground\":%s}",
               (unsigned long)pid, r.left, r.top, r.right - r.left, r.bottom - r.top,
               minimized ? "true" : "false", hwnd == foreground ? "true" : "false");
}

static BOOL CALLBACK enum_proc(HWND hwnd, LPARAM lparam) {
    EnumCtx *ctx = (EnumCtx *)lparam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return TRUE;
    if (ctx->title_match) {
        wchar_t title[512];
        GetWindowTextW(hwnd, title, 512);
        if (wcsstr(title, ctx->title_match) == NULL) return TRUE;
        ctx->found = hwnd;
        return FALSE;
    }
    if (!ctx->first) buf_add(ctx->b, ",");
    ctx->first = 0;
    window_json(ctx->b, hwnd, ctx->foreground);
    return TRUE;
}

static HWND find_window_by_title(const wchar_t *title) {
    EnumCtx ctx;
    ZeroMemory(&ctx, sizeof(ctx));
    ctx.title_match = title;
    ctx.foreground = GetForegroundWindow();
    EnumWindows(enum_proc, (LPARAM)&ctx);
    return ctx.found;
}

static int focus_window(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return 0;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    DWORD fg_thread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    DWORD target_thread = GetWindowThreadProcessId(hwnd, NULL);
    int attached = 0;
    if (fg_thread && target_thread && fg_thread != target_thread) {
        attached = AttachThreadInput(fg_thread, target_thread, TRUE);
    }
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    if (attached) AttachThreadInput(fg_thread, target_thread, FALSE);
    Sleep(80);
    return GetForegroundWindow() == hwnd;
}

/* ------------------------------------------------------------------ */
/* Options                                                             */
/* ------------------------------------------------------------------ */

static const wchar_t *opt_w(int argc, wchar_t **argv, const wchar_t *name) {
    for (int i = 2; i < argc - 1; ++i) {
        if (_wcsicmp(argv[i], name) == 0) return argv[i + 1];
    }
    return NULL;
}

static int opt_i(int argc, wchar_t **argv, const wchar_t *name, int def) {
    const wchar_t *v = opt_w(argc, argv, name);
    return v ? _wtoi(v) : def;
}

static const wchar_t *opt_str(int argc, wchar_t **argv, const wchar_t *name, const wchar_t *def) {
    const wchar_t *v = opt_w(argc, argv, name);
    return v ? v : def;
}

static HWND parse_hwnd(const wchar_t *s) {
    if (!s) return NULL;
    wchar_t *end = NULL;
    unsigned long long v = wcstoull(s, &end, 0);
    return (HWND)(ULONG_PTR)v;
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */

static int action_status(void) {
    int loaded = load_driver();
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":true,\"backend\":");
    buf_add(&b, loaded ? "\"interception\"" : "\"sendinput\"");
    buf_printf(&b, ",\"driverLoaded\":%s,\"keyboard\":%d,\"mouse\":%d,\"dll\":",
               loaded ? "true" : "false", g_keyboard, g_mouse);
    json_w(&b, g_dll_path[0] ? g_dll_path : L"");
    buf_printf(&b, ",\"screen\":{\"width\":%d,\"height\":%d}}", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    return succeed(&b);
}

static int action_display(void) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    HDC dc = GetDC(NULL);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(NULL, dc);
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":true,\"primary\":{\"width\":%d,\"height\":%d},\"virtual\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d},\"dpi\":%d}",
               w, h, vx, vy, vw, vh, dpi);
    return succeed(&b);
}

static int action_cursor(void) {
    POINT p;
    GetCursorPos(&p);
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":true,\"x\":%ld,\"y\":%ld}", p.x, p.y);
    return succeed(&b);
}

static int action_windows(void) {
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":true,\"windows\":[");
    EnumCtx ctx;
    ctx.b = &b;
    ctx.first = 1;
    ctx.foreground = GetForegroundWindow();
    ctx.title_match = NULL;
    ctx.found = NULL;
    EnumWindows(enum_proc, (LPARAM)&ctx);
    buf_add(&b, "]}");
    return succeed(&b);
}

static int action_active(void) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return failf("no foreground window");
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":true,\"window\":");
    window_json(&b, hwnd, hwnd);
    buf_add(&b, "}");
    return succeed(&b);
}

static int action_focus(int argc, wchar_t **argv) {
    HWND hwnd = parse_hwnd(opt_w(argc, argv, L"--hwnd"));
    const wchar_t *title = opt_w(argc, argv, L"--title");
    if (!hwnd && title) hwnd = find_window_by_title(title);
    if (!hwnd) return failf("window not found");
    int ok = focus_window(hwnd);
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":%s,\"hwnd\":\"0x%llX\"}", ok ? "true" : "false", (unsigned long long)(ULONG_PTR)hwnd);
    return succeed(&b);
}

static int action_launch(int argc, wchar_t **argv) {
    const wchar_t *target = opt_w(argc, argv, L"--target");
    const wchar_t *args = opt_w(argc, argv, L"--args");
    if (!target || !*target) return failf("--target is required");
    HINSTANCE r = ShellExecuteW(NULL, L"open", target, args, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) return failf("ShellExecuteW failed (%lld)", (long long)(INT_PTR)r);
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":true,\"target\":");
    json_w(&b, target);
    buf_add(&b, "}");
    return succeed(&b);
}

static int action_key(int argc, wchar_t **argv, int mode) {
    const wchar_t *key = opt_w(argc, argv, L"--key");
    if (!key || !*key) return failf("--key is required");
    load_driver();
    int ok = 0;
    if (mode == 0) {
        ok = press_chord(key);
    } else {
        unsigned short scan, e0;
        if (!key_token(key, &scan, &e0)) return failf("unknown key: %ls", key);
        ok = send_key_stroke(scan, mode == 2, e0);
    }
    if (!ok) return failf("key injection failed");
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":true,\"key\":");
    json_w(&b, key);
    buf_add(&b, "}");
    return succeed(&b);
}

static int action_type(int argc, wchar_t **argv) {
    const wchar_t *text = opt_w(argc, argv, L"--text");
    if (!text) return failf("--text is required");
    load_driver();
    if (!type_text(text)) return failf("typing failed");
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":true,\"chars\":");
    buf_printf(&b, "%zu", wcslen(text));
    buf_add(&b, "}");
    return succeed(&b);
}

static int action_move(int argc, wchar_t **argv) {
    int x = opt_i(argc, argv, L"--x", 0);
    int y = opt_i(argc, argv, L"--y", 0);
    load_driver();
    if (!move_absolute(x, y)) return failf("absolute move failed");
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":true,\"x\":%d,\"y\":%d}", x, y);
    return succeed(&b);
}

static int action_rawmove(int argc, wchar_t **argv) {
    int dx = opt_i(argc, argv, L"--dx", 0);
    int dy = opt_i(argc, argv, L"--dy", 0);
    load_driver();
    if (!move_relative(dx, dy)) return failf("relative move failed");
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":true,\"dx\":%d,\"dy\":%d}", dx, dy);
    return succeed(&b);
}

static int action_click(int argc, wchar_t **argv) {
    const wchar_t *button = opt_str(argc, argv, L"--button", L"left");
    int count = opt_i(argc, argv, L"--count", 1);
    int has_x = opt_w(argc, argv, L"--x") != NULL;
    int has_y = opt_w(argc, argv, L"--y") != NULL;
    load_driver();
    if (has_x && has_y) {
        if (!move_absolute(opt_i(argc, argv, L"--x", 0), opt_i(argc, argv, L"--y", 0))) return failf("absolute move failed");
        Sleep(30);
    }
    unsigned short down, up;
    if (!button_bits(button, 1, &down) || !button_bits(button, 0, &up)) return failf("unknown button: %ls", button);
    if (count < 1) count = 1;
    if (count > 10) count = 10;
    for (int i = 0; i < count; ++i) {
        send_mouse_stroke(down, INTERCEPTION_MOUSE_MOVE_RELATIVE, 0, 0, 0);
        Sleep(20);
        send_mouse_stroke(up, INTERCEPTION_MOUSE_MOVE_RELATIVE, 0, 0, 0);
        if (i + 1 < count) Sleep(80);
    }
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":true,\"button\":\"%ls\",\"count\":%d}", button, count);
    return succeed(&b);
}

static int action_button(int argc, wchar_t **argv, int down) {
    const wchar_t *button = opt_str(argc, argv, L"--button", L"left");
    unsigned short state;
    if (!button_bits(button, down, &state)) return failf("unknown button: %ls", button);
    load_driver();
    if (!send_mouse_stroke(state, INTERCEPTION_MOUSE_MOVE_RELATIVE, 0, 0, 0)) return failf("button injection failed");
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":true,\"button\":\"%ls\",\"down\":%s}", button, down ? "true" : "false");
    return succeed(&b);
}

static int action_scroll(int argc, wchar_t **argv) {
    int amount = opt_i(argc, argv, L"--amount", 1);
    load_driver();
    if (amount == 0) return failf("--amount must be non-zero");
    int dir = amount > 0 ? 1 : -1;
    int n = abs(amount);
    for (int i = 0; i < n; ++i) {
        if (!send_mouse_stroke(INTERCEPTION_MOUSE_WHEEL, INTERCEPTION_MOUSE_MOVE_RELATIVE, (short)(120 * dir), 0, 0))
            return failf("scroll failed");
        Sleep(20);
    }
    Buf b;
    buf_init(&b);
    buf_printf(&b, "{\"ok\":true,\"amount\":%d}", amount);
    return succeed(&b);
}

static int action_releaseall(void) {
    load_driver();
    const wchar_t *keys[] = {
        L"w", L"a", L"s", L"d", L"f", L"shift", L"ctrl", L"alt", L"space", L"enter", L"tab", L"esc",
        L"up", L"down", L"left", L"right", L"1", L"2", L"3", L"4", L"5"
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        unsigned short scan, e0;
        if (key_token(keys[i], &scan, &e0)) send_key_stroke(scan, 1, e0);
    }
    const unsigned short ups[] = {
        INTERCEPTION_MOUSE_LEFT_BUTTON_UP, INTERCEPTION_MOUSE_RIGHT_BUTTON_UP,
        INTERCEPTION_MOUSE_MIDDLE_BUTTON_UP, INTERCEPTION_MOUSE_BUTTON_4_UP, INTERCEPTION_MOUSE_BUTTON_5_UP
    };
    for (size_t i = 0; i < sizeof(ups) / sizeof(ups[0]); ++i) {
        send_mouse_stroke(ups[i], INTERCEPTION_MOUSE_MOVE_RELATIVE, 0, 0, 0);
    }
    Buf b;
    buf_init(&b);
    buf_add(&b, "{\"ok\":true}");
    return succeed(&b);
}

static int action_help(void) {
    printf("ligui-input: low-level Windows input\n"
           "actions:\n"
           "  status\n"
           "  display | cursor | windows | active\n"
           "  focus --title <substr> | --hwnd 0x...\n"
           "  launch --target <exe|url> [--args <args>]\n"
           "  key --key <name|chord>        e.g. f, ctrl+shift+esc\n"
           "  keydown --key <name> | keyup --key <name>\n"
           "  type --text <text>\n"
           "  move --x <px> --y <px>        absolute primary-screen point\n"
           "  rawmove --dx <px> --dy <px>   relative mouse (camera/raw input)\n"
           "  click [--x --y] [--button left|right|middle|mouse4|mouse5] [--count N]\n"
           "  down --button <name> | up --button <name>\n"
           "  scroll --amount <notches>     positive up, negative down\n"
           "  releaseall\n"
           "env: LIGUI_INTERCEPTION_DLL, LIGUI_INTERCEPTION_KEYBOARD, LIGUI_INTERCEPTION_MOUSE\n");
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) return action_help();

    const wchar_t *action = argv[1];
    if (!_wcsicmp(action, L"status")) return action_status();
    if (!_wcsicmp(action, L"display")) return action_display();
    if (!_wcsicmp(action, L"cursor")) return action_cursor();
    if (!_wcsicmp(action, L"windows")) return action_windows();
    if (!_wcsicmp(action, L"active")) return action_active();
    if (!_wcsicmp(action, L"focus")) return action_focus(argc, argv);
    if (!_wcsicmp(action, L"launch")) return action_launch(argc, argv);
    if (!_wcsicmp(action, L"key")) return action_key(argc, argv, 0);
    if (!_wcsicmp(action, L"keydown")) return action_key(argc, argv, 1);
    if (!_wcsicmp(action, L"keyup")) return action_key(argc, argv, 2);
    if (!_wcsicmp(action, L"type")) return action_type(argc, argv);
    if (!_wcsicmp(action, L"move")) return action_move(argc, argv);
    if (!_wcsicmp(action, L"rawmove") || !_wcsicmp(action, L"rmove")) return action_rawmove(argc, argv);
    if (!_wcsicmp(action, L"click")) return action_click(argc, argv);
    if (!_wcsicmp(action, L"down")) return action_button(argc, argv, 1);
    if (!_wcsicmp(action, L"up")) return action_button(argc, argv, 0);
    if (!_wcsicmp(action, L"scroll")) return action_scroll(argc, argv);
    if (!_wcsicmp(action, L"releaseall")) return action_releaseall();
    if (!_wcsicmp(action, L"help") || !_wcsicmp(action, L"--help")) return action_help();

    return failf("unknown action: %ls", action);
}
