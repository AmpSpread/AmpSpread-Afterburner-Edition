/*
 * AmpSpread Afterburner Edition v1.0.4 - MSI Afterburner 32-bit monitoring plugin.
 * Read-only. No direct PSU IO, drivers, services or AmpSpread desktop app dependency.
 *
 * MSI Afterburner SDK interface references:
 *   MSIAfterburnerMonitoringSourceDesc.h
 *   MAHMSharedMemory.h
 * from the Afterburner SDK; this source uses only compatible structures.
 * Copyright (c) 2026 AmpSpread contributors. SPDX-License-Identifier: MIT
 */

/* Minimal Win32 declarations: builds without an MSVC SDK or CRT. */
typedef unsigned int DWORD;              /* Win32 DWORD is 32-bit */
typedef unsigned int UINT;
typedef unsigned long SIZE_T;  /* 32-bit Win32 target */
typedef long LONG;             /* Windows LONG is always 32-bit */
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HWND;
typedef const char *LPCSTR;
typedef void *LPVOID;
typedef unsigned char BYTE;
#define WINAPI __stdcall
#define EXPORT __declspec(dllexport)
#define TRUE 1
#define FALSE 0
#define FILE_MAP_READ 0x0004u
#define HOTKEY_SHIFT 1u
#define HOTKEY_CONTROL 2u
#define HOTKEY_ALT 4u
#define HOTKEY_EXTENDED 8u
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define KEY_DOWN 0x8000
#define INFINITE 0xFFFFFFFFu
#define MB_YESNO 0x00000004u
#define MB_ICONQUESTION 0x00000020u
#define IDYES 6
#define MAX_PATH 260
#define FLOAT_UNAVAILABLE 3.402823466e+38f
#define ID_PLUGIN_PSU 0x000000F6u

__declspec(dllimport) HANDLE WINAPI OpenFileMappingA(DWORD, BOOL, LPCSTR);
__declspec(dllimport) LPVOID WINAPI MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, unsigned long);
__declspec(dllimport) BOOL WINAPI UnmapViewOfFile(const void*);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) DWORD WINAPI GetTickCount(void);
__declspec(dllimport) int WINAPI MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
__declspec(dllimport) DWORD WINAPI GetModuleFileNameA(HMODULE, char*, DWORD);
__declspec(dllimport) DWORD WINAPI GetPrivateProfileStringA(LPCSTR, LPCSTR, LPCSTR, char*, DWORD, LPCSTR);
__declspec(dllimport) short WINAPI GetAsyncKeyState(int);
__declspec(dllimport) HANDLE WINAPI CreateThread(void*, SIZE_T, DWORD(WINAPI*)(void*), void*, DWORD, DWORD*);
__declspec(dllimport) void WINAPI Sleep(DWORD);
__declspec(dllimport) DWORD WINAPI WaitForSingleObject(HANDLE, DWORD);
__declspec(dllimport) LONG WINAPI InterlockedExchange(volatile LONG*, LONG);

/* This variable satisfies MSVC-compatible float support without a CRT. */
int _fltused = 0;

typedef struct MONITORING_SOURCE_DESC {
    DWORD dwVersion;
    char szName[MAX_PATH];
    char szUnits[MAX_PATH];
    char szFormat[MAX_PATH];
    char szGroup[MAX_PATH];
    DWORD dwID;
    DWORD dwInstance;
    float fltMaxLimit;
    float fltMinLimit;
    char szNameTemplate[MAX_PATH];
    char szGroupTemplate[MAX_PATH];
} MONITORING_SOURCE_DESC;

typedef struct MAHM_HEADER {
    DWORD signature, version, headerSize, numEntries, entrySize;
    DWORD lastPollTime, gpuEntries, gpuEntrySize;
} MAHM_HEADER;

typedef struct BANK {
    float a[6];
    unsigned char found[6];
    unsigned int foundCount;
    float total;
} BANK;

/* Time-weighted mean of reported spread, independent of callback count.
 * MAHM does not expose an individual PSU hardware-sample sequence number. */
typedef struct STATS {
    double integralAmpMs;
    unsigned long long measuredMs;
    DWORD lastTick;
    int initialized;
    int interrupted;
    float current;
    float average;
    float maximum;
} STATS;

#define MAX_ACCOUNTED_GAP_MS 2000u
#define CALLBACK_CACHE_MS 25u

static STATS stats[2];
static DWORD lastSampleTick;
static int haveSampleTick;
static int activeBank = -1;
static int dataValid;

/* The hotkey watcher only sets a reset flag; monitoring thread owns statistics. */
static volatile LONG resetPending;
static volatile LONG hotkeyWatcherRunning;
static int hotkeyWatcherStarted;
static HANDLE hotkeyWatcherThread;
static volatile LONG hotkeyConfigured;
static volatile LONG hotkeySeenCount;
static volatile LONG hotkeyResetAppliedCount;
static volatile LONG lastHotkeyMainDown;
static volatile LONG lastHotkeyModifierMatch;
static volatile LONG watcherConfigRead;
static volatile LONG watcherConfigError;
static volatile LONG watcherState;
static DWORD lastDecodedHotkey;
static char lastRawHotkey[40];
static char lastConfigPath[MAX_PATH];


/* Parse ClearHistoryHotkey, e.g. 00020076h (Ctrl+F7).
   Upper word is HOTKEYF_SHIFT/CONTROL/ALT/EXTENDED, lower is virtual key. */
static DWORD parse_hotkey(const char* value) {
    DWORD x = 0;
    unsigned int i, digits = 0;
    if (!value) return 0;
    for (i = 0; i < 20 && value[i] == ' '; ++i) { }
    if (value[i] == '0' && (value[i+1] == 'x' || value[i+1] == 'X')) i += 2;
    while (i < 20) {
        int v;
        char c = value[i++];
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else if ((c == 'h' || c == 'H') && digits > 0) break;
        else if (c == 0 || c == ' ' || c == '\r' || c == '\n') break;
        else return 0;
        if (digits++ >= 8) return 0;
        x = (x << 4) | (DWORD)v;
    }
    return x;
}

/* Strict modifier match; held keys trigger at most once until released. */
static int hotkey_is_down(DWORD binding, int shift, int control, int alt, int key) {
    DWORD flags = binding >> 16;
    DWORD vk = binding & 0xFFFFu;
    if (!vk || vk > 0xFEu || (flags & ~15u)) return 0;
    if (!!(flags & HOTKEY_SHIFT) != !!shift) return 0;
    if (!!(flags & HOTKEY_CONTROL) != !!control) return 0;
    if (!!(flags & HOTKEY_ALT) != !!alt) return 0;
    return !!key;
}
static int hotkey_pressed(DWORD binding, int shift, int control, int alt, int key, int *wasDown) {
    int down = hotkey_is_down(binding, shift, control, alt, key);
    int newPress = down && !*wasDown;
    *wasDown = down;
    return newPress;
}

/* Existing declaration below the watcher is used to build config paths. */
static void put_text(char *out, unsigned int outLen, const char *src);

/* Prefer active Profiles config, fall back to installation-root template. */
static int config_paths(char profile[MAX_PATH], char root[MAX_PATH]) {
    char exe[MAX_PATH];
    DWORD length = GetModuleFileNameA((HMODULE)0, exe, MAX_PATH);
    unsigned int sep = MAX_PATH, i, j;
    const char *profileName = "Profiles\\MSIAfterburner.cfg";
    const char *rootName = "MSIAfterburner.cfg";
    if (!length || length >= MAX_PATH) return 0;
    for (i = 0; i < length; ++i) if (exe[i] == '\\' || exe[i] == '/') sep = i + 1;
    if (sep >= MAX_PATH) return 0;
    for (i = 0; i < sep; ++i) { profile[i] = exe[i]; root[i] = exe[i]; }
    for (j = 0; profileName[j] && sep + j + 1 < MAX_PATH; ++j) profile[sep+j] = profileName[j];
    if (profileName[j]) return 0;
    profile[sep+j] = 0;
    for (j = 0; rootName[j] && sep + j + 1 < MAX_PATH; ++j) root[sep+j] = rootName[j];
    if (rootName[j]) return 0;
    root[sep+j] = 0;
    return 1;
}

/* Read the configured key, without editing any Afterburner settings. */
static int try_hotkey_file(const char* path, DWORD *hotkey) {
    char value[40] = {0};
    DWORD count = GetPrivateProfileStringA("Settings", "ClearHistoryHotkey", "NOT_FOUND", value, 40, path);
    unsigned int i;
    if (!count || (count == 9u && value[0]=='N' && value[1]=='O' &&
        value[2]=='T' && value[3]=='_' && value[4]=='F' && value[5]=='O' &&
        value[6]=='U' && value[7]=='N' && value[8]=='D')) return 0;
    for (i = 0; i < sizeof(lastRawHotkey)-1 && value[i]; ++i) lastRawHotkey[i] = value[i];
    lastRawHotkey[i] = 0;
    *hotkey = parse_hotkey(value);
    watcherConfigRead = (LONG)count;
    watcherConfigError = 0;
    hotkeyConfigured = *hotkey != 0;
    lastDecodedHotkey = *hotkey;
    put_text(lastConfigPath, MAX_PATH, path);
    return 1;
}
static DWORD read_afterburner_hotkey(const char* profiles, const char* root) {
    DWORD binding = 0;
    if (try_hotkey_file(profiles, &binding) || try_hotkey_file(root, &binding)) return binding;
    watcherConfigRead = 0;
    watcherConfigError = 1;
    hotkeyConfigured = 0;
    lastDecodedHotkey = 0;
    put_text(lastRawHotkey, sizeof(lastRawHotkey), "NOT_FOUND");
    put_text(lastConfigPath, MAX_PATH, profiles);
    return 0;
}

static DWORD WINAPI hotkey_watcher(void *ignored) {
    char profiles[MAX_PATH], root[MAX_PATH];
    DWORD binding = 0, nextReload = 0;
    int wasDown = 0;
    (void)ignored;
    if (!config_paths(profiles, root)) { watcherConfigError = 2; watcherState = 3; return 0; }
    put_text(lastConfigPath, MAX_PATH, profiles);
    watcherState = 1;
    while (hotkeyWatcherRunning) {
        DWORD tick = GetTickCount();
        if (nextReload == 0 || (DWORD)(tick-nextReload) < 0x80000000u) {
            DWORD nextBinding = read_afterburner_hotkey(profiles, root);
            if (nextBinding != binding) { binding = nextBinding; wasDown = 0; }
            nextReload = tick + 2000u;
        }
        if (binding) {
            int vk = (int)(binding & 0xFF);
            int shift = (GetAsyncKeyState(VK_SHIFT) & KEY_DOWN) != 0;
            int ctrl = (GetAsyncKeyState(VK_CONTROL) & KEY_DOWN) != 0;
            int alt = (GetAsyncKeyState(VK_MENU) & KEY_DOWN) != 0;
            int key = (GetAsyncKeyState(vk) & KEY_DOWN) != 0;
            lastHotkeyMainDown = key;
            lastHotkeyModifierMatch = (!!(binding & (HOTKEY_SHIFT << 16)) == !!shift &&
                !!(binding & (HOTKEY_CONTROL << 16)) == !!ctrl &&
                !!(binding & (HOTKEY_ALT << 16)) == !!alt);
            if (hotkey_pressed(binding, shift, ctrl, alt, key, &wasDown)) {
                InterlockedExchange(&resetPending, 1);
                InterlockedExchange(&hotkeySeenCount, hotkeySeenCount + 1);
            }
        } else wasDown = 0;
        Sleep(binding ? 10 : 100);
    }
    watcherState = 2;
    return 0;
}

/* Avoid doing config I/O or creating a thread during plugin loading. */
static void start_hotkey_watcher(void) {
    if (hotkeyWatcherStarted) return;
    hotkeyWatcherStarted = 1;
    InterlockedExchange(&hotkeyWatcherRunning, 1);
    hotkeyWatcherThread = CreateThread(0, 0, hotkey_watcher, 0, 0, 0);
    if (!hotkeyWatcherThread) {
        InterlockedExchange(&hotkeyWatcherRunning, 0);
        watcherState = 4;
    }
}
static void stop_hotkey_watcher(void) {
    InterlockedExchange(&hotkeyWatcherRunning, 0);
    if (hotkeyWatcherThread) {
        WaitForSingleObject(hotkeyWatcherThread, INFINITE);
        CloseHandle(hotkeyWatcherThread);
        hotkeyWatcherThread = 0;
    }
    hotkeyWatcherStarted = 0;
}

static void clear_stats(void) {
    int b;
    for (b = 0; b < 2; ++b) {
        stats[b].integralAmpMs = 0.0;
        stats[b].measuredMs = 0;
        stats[b].lastTick = 0;
        stats[b].initialized = 0;
        stats[b].interrupted = 0;
        stats[b].current = 0.0f;
        stats[b].average = 0.0f;
        stats[b].maximum = 0.0f;
    }
    haveSampleTick = 0;
    activeBank = -1;
    dataValid = 0;
}

static int upper(int ch) {
    if (ch >= 'a' && ch <= 'z') return ch - ('a' - 'A');
    return ch;
}

static int contains_token(const char *text, unsigned int size, const char *token) {
    unsigned int i, j;
    for (i = 0; i < size && text[i]; ++i) {
        for (j = 0; token[j] && i + j < size && text[i + j]
             && upper((unsigned char)text[i + j]) == upper((unsigned char)token[j]); ++j) {}
        if (!token[j]) return (int)i;
    }
    return -1;
}

static void put_text(char *out, unsigned int outLen, const char *src) {
    unsigned int i;
    for (i = 0; i < outLen; ++i) out[i] = 0;
    for (i = 0; i + 1 < outLen && src[i]; ++i) out[i] = src[i];
}

/* Parses only individual connector pin current entries, never aggregated current. */
static int classify_entry(const char *name, const char *unit, int *bank, int *pin) {
    int at = contains_token(name, MAX_PATH, "12VHPWR");
    if (at < 0) at = contains_token(name, MAX_PATH, "12V-2X6");
    if (at < 0 || at + 10 >= MAX_PATH) return 0;
    if (contains_token(name, MAX_PATH, "CURRENT") < 0) return 0;
    if (upper((unsigned char)unit[0]) != 'A') return 0;
    if (name[at+7] != '1' && name[at+7] != '2') return 0;
    if (name[at+8] != '_' && name[at+8] != '-') return 0;
    if (name[at+9] < '1' || name[at+9] > '6') return 0;
    if (name[at+10] >= '0' && name[at+10] <= '9') return 0;
    *bank = name[at+7] - '1';
    *pin = name[at+9] - '1';
    return 1;
}

/* Read a bounded snapshot of already-published Afterburner sensor values. */
static int snapshot(BANK bank[2]) {
    HANDLE mapping;
    const BYTE *p;
    const MAHM_HEADER *h;
    DWORD totalSize, i, expectedEntries, expectedEntrySize, expectedHeaderSize;
    int b, pin;
    mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "MAHMSharedMemory");
    if (!mapping) return 0;

    /* First map exactly the header. A second map ensures bounds safety. */
    p = (const BYTE *)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(MAHM_HEADER));
    if (!p) { CloseHandle(mapping); return 0; }
    h = (const MAHM_HEADER *)p;
    if ((h->signature != 0x4D41484Du && h->signature != 0x4D48414Du)
        || h->version < 0x00020000u || h->version >= 0x00040000u
        || h->headerSize < sizeof(MAHM_HEADER) || h->headerSize > 4096
        || h->numEntries > 2048 || h->entrySize < 1304 || h->entrySize > 8192
        || h->numEntries == 0) {
        UnmapViewOfFile(p); CloseHandle(mapping); return 0;
    }
    totalSize = h->headerSize + h->numEntries * h->entrySize;
    UnmapViewOfFile(p);
    p = (const BYTE *)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, totalSize);
    if (!p) { CloseHandle(mapping); return 0; }
    h = (const MAHM_HEADER *)p;
    if ((h->signature != 0x4D41484Du && h->signature != 0x4D48414Du)
        || h->version < 0x00020000u || h->version >= 0x00040000u
        || h->headerSize < sizeof(MAHM_HEADER) || h->headerSize > 4096
        || h->numEntries == 0 || h->numEntries > 2048
        || h->entrySize < 1304 || h->entrySize > 8192
        || h->headerSize + h->numEntries * h->entrySize > totalSize) {
        UnmapViewOfFile(p); CloseHandle(mapping); return 0;
    }

    expectedEntries = h->numEntries;
    expectedEntrySize = h->entrySize;
    expectedHeaderSize = h->headerSize;
    for (i = 0; i < expectedEntries; ++i) {
        const BYTE *entry = p + expectedHeaderSize + i * expectedEntrySize;
        float amps;
        if (!classify_entry((const char *)entry, (const char *)(entry+260), &b, &pin)) continue;
        amps = *(const float *)(entry + 1300);
        if (!(amps >= 0.0f && amps <= 100.0f)) continue; /* excludes FLT_MAX and NaN */
        if (bank[b].found[pin]) continue;
        bank[b].found[pin] = 1;
        bank[b].a[pin] = amps;
        bank[b].total += amps;
        bank[b].foundCount++;
    }
    /* Best-effort check that no per-pin reading changed during the scan. */
    if ((h->signature != 0x4D41484Du && h->signature != 0x4D48414Du)
        || h->numEntries != expectedEntries || h->entrySize != expectedEntrySize
        || h->headerSize != expectedHeaderSize) {
        UnmapViewOfFile(p); CloseHandle(mapping); return 0;
    }
    for (i = 0; i < expectedEntries; ++i) {
        const BYTE *entry = p + expectedHeaderSize + i * expectedEntrySize;
        int port, pinIndex;
        if (classify_entry((const char *)entry, (const char *)(entry + 260), &port, &pinIndex)
            && bank[port].found[pinIndex]) {
            float check = *(const float *)(entry + 1300);
            if (check != bank[port].a[pinIndex]) {
                UnmapViewOfFile(p); CloseHandle(mapping); return 0;
            }
        }
    }
    if (h->numEntries != expectedEntries || h->entrySize != expectedEntrySize
        || h->headerSize != expectedHeaderSize) {
        UnmapViewOfFile(p); CloseHandle(mapping); return 0;
    }
    UnmapViewOfFile(p);
    CloseHandle(mapping);
    return 1;
}

static void mark_data_unavailable(void) {
    int b;
    dataValid = 0;
    activeBank = -1;
    for (b = 0; b < 2; ++b) stats[b].interrupted = 1;
}

/* Refresh from Afterburner's shared memory; never poll the PSU directly. */
static void update_spread(void) {
    DWORD now = GetTickCount();
    BANK bank[2] = {0};
    STATS *st;
    int b, i;
    float minVal, maxVal, spread;
    DWORD delta;

    /* Cache callbacks for the same Afterburner monitoring cycle. */
    if (dataValid && haveSampleTick && (DWORD)(now - lastSampleTick) < CALLBACK_CACHE_MS)
        return;
    lastSampleTick = now;
    haveSampleTick = 1;
    if (!snapshot(bank)) { mark_data_unavailable(); return; }

    b = -1;
    if (bank[0].foundCount == 6 && bank[0].total > 0.01f) b = 0;
    if (bank[1].foundCount == 6 && bank[1].total > 0.01f
        && (b < 0 || bank[1].total > bank[0].total)) b = 1;
    if (b < 0) { mark_data_unavailable(); return; }

    minVal = bank[b].a[0]; maxVal = bank[b].a[0];
    for (i = 1; i < 6; ++i) {
        if (bank[b].a[i] < minVal) minVal = bank[b].a[i];
        if (bank[b].a[i] > maxVal) maxVal = bank[b].a[i];
    }
    spread = maxVal - minVal;
    st = &stats[b];

    if (!st->initialized) {
        st->initialized = 1;
        st->current = spread;
        st->average = spread;
        st->maximum = spread;
        st->lastTick = now;
    } else {
        /* Trapezoidal time weighting prevents callback frequency from
           deciding the average. Gaps/invalid data are excluded. */
        delta = (DWORD)(now - st->lastTick);
        if (!st->interrupted && delta > 0 && delta <= MAX_ACCOUNTED_GAP_MS) {
            st->integralAmpMs += 0.5 * ((double)st->current + (double)spread) * (double)delta;
            st->measuredMs += (unsigned long long)delta;
            st->average = (float)(st->integralAmpMs / (double)st->measuredMs);
        }
        st->lastTick = now;
        st->interrupted = 0;
        st->current = spread;
        if (spread > st->maximum) st->maximum = spread;
    }
    activeBank = b;
    dataValid = 1;
}

EXPORT BOOL __cdecl SetupSource(DWORD index, HWND window) {
    if (!window) return (index == 0xFFFFFFFFu) ? TRUE : FALSE;
    if (MessageBoxA(window,
            "Reset 12V-2x6 spread average and maximum for BOTH PSU ports?\n\n"
            "Use Reset between cable comparison tests.\n"
            "Current readings will remain read-only.",
            "AmpSpread Afterburner Edition 1.0.4 - Reset statistics", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        clear_stats();
    }
    return TRUE;
}

EXPORT DWORD __cdecl GetSourcesNum(void) { return 5u; }

EXPORT BOOL __cdecl GetSourceDesc(DWORD index, MONITORING_SOURCE_DESC *desc) {
    if (!desc || index > 4 || desc->dwVersion < 0x00010000u) return FALSE;
    put_text(desc->szName, MAX_PATH,
         index == 0 ? "12V-2x6 current spread" :
         index == 1 ? "12V-2x6 average spread" :
         index == 2 ? "12V-2x6 maximum spread" :
         index == 3 ? "12V-2x6 active port" :
                      "12V-2x6 data complete");
    put_text(desc->szUnits, MAX_PATH, index >= 3 ? "" : "A");
    put_text(desc->szFormat, MAX_PATH, index >= 3 ? "%.0f" : "%.3f");
    put_text(desc->szGroup, MAX_PATH, "AmpSpread Afterburner Edition");
    desc->dwID = ID_PLUGIN_PSU;
    desc->dwInstance = 0;
    desc->fltMinLimit = 0.0f;
    /* Default visual ranges; Afterburner may retain a user's previously saved graph limits. */
    desc->fltMaxLimit = index == 0 ? 1.0f :
                        index == 1 ? 0.5f :
                        index == 2 ? 1.0f :
                        index == 3 ? 2.0f : 1.0f;
    put_text(desc->szNameTemplate, MAX_PATH, "");
    put_text(desc->szGroupTemplate, MAX_PATH, "");
    return TRUE;
}

EXPORT float __cdecl GetSourceData(DWORD index) {
    STATS *s;
    if (index > 4) return FLOAT_UNAVAILABLE;
    update_spread();
    if (index == 4) return dataValid ? 1.0f : 0.0f;
    if (!dataValid || activeBank < 0) return FLOAT_UNAVAILABLE;
    s = &stats[activeBank];
    if (index == 3) return (float)(activeBank + 1);
    if (index == 0) return s->current;
    if (index == 1) return s->initialized ? s->average : FLOAT_UNAVAILABLE;
    return s->maximum;
}

EXPORT void __cdecl Uninit(void) { clear_stats(); }