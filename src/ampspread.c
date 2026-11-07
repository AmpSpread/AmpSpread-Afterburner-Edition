/*
 * AmpSpread Afterburner Edition v1.0.2 - MSI Afterburner 32-bit monitoring plugin.
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
typedef int BOOL;
typedef void *HANDLE;
typedef void *HWND;
typedef const char *LPCSTR;
typedef void *LPVOID;
typedef unsigned char BYTE;
#define WINAPI __stdcall
#define EXPORT __declspec(dllexport)
#define TRUE 1
#define FALSE 0
#define FILE_MAP_READ 0x0004u
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

typedef struct STATS {
    double sum;
    DWORD samples;
    float current;
    float maximum;
} STATS;

static STATS stats[2];
static DWORD lastSampleTick;
static int haveSampleTick;
static int activeBank = -1;
static int dataValid;

static void clear_stats(void) {
    int b;
    for (b = 0; b < 2; ++b) {
        stats[b].sum = 0.0;
        stats[b].samples = 0;
        stats[b].current = 0.0f;
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
    DWORD totalSize, i;
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

    for (i = 0; i < h->numEntries; ++i) {
        const BYTE *entry = p + h->headerSize + i * h->entrySize;
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
    UnmapViewOfFile(p);
    CloseHandle(mapping);
    return 1;
}

static void update_spread(void) {
    DWORD now = GetTickCount();
    BANK bank[2] = {0};
    int b, i;
    float minVal, maxVal, spread;
    if (haveSampleTick && ((DWORD)(now - lastSampleTick)) < 50u) return;
    lastSampleTick = now;
    haveSampleTick = 1;
    if (!snapshot(bank)) { dataValid = 0; activeBank = -1; return; }

    /* Select the complete set of six pins drawing the most current.
       If both ports are populated, this is the predominant GPU load. */
    b = -1;
    if (bank[0].foundCount == 6 && bank[0].total > 0.01f) b = 0;
    if (bank[1].foundCount == 6 && bank[1].total > 0.01f
        && (b < 0 || bank[1].total > bank[0].total)) b = 1;
    if (b < 0) { dataValid = 0; activeBank = -1; return; }

    minVal = bank[b].a[0]; maxVal = bank[b].a[0];
    for (i = 1; i < 6; ++i) {
        if (bank[b].a[i] < minVal) minVal = bank[b].a[i];
        if (bank[b].a[i] > maxVal) maxVal = bank[b].a[i];
    }
    spread = maxVal - minVal;
    activeBank = b;
    dataValid = 1;
    stats[b].current = spread;
    if (stats[b].samples == 0 || spread > stats[b].maximum) stats[b].maximum = spread;
    /* Maintain a bounded running mean over the whole monitoring session. */
    if (stats[b].samples != 0xFFFFFFFFu) {
        stats[b].sum += (double)spread;
        stats[b].samples++;
    } else {
        stats[b].sum = (stats[b].sum / (double)stats[b].samples) * 0.999999;
        stats[b].samples = 1;
    }
}

EXPORT BOOL __cdecl SetupSource(DWORD index, HWND window) {
    if (!window) return (index == 0xFFFFFFFFu) ? TRUE : FALSE;
    if (MessageBoxA(window,
            "Reset 12V-2x6 spread average and maximum for BOTH PSU ports?\n\n"
            "Use Reset between cable comparison tests.\n"
            "Current readings will remain read-only.",
            "AmpSpread Afterburner Edition 1.0.2 - Reset statistics", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        clear_stats();
    }
    return TRUE;
}

EXPORT DWORD __cdecl GetSourcesNum(void) { return 4u; }

EXPORT BOOL __cdecl GetSourceDesc(DWORD index, MONITORING_SOURCE_DESC *desc) {
    if (!desc || index > 3 || desc->dwVersion < 0x00010000u) return FALSE;
    put_text(desc->szName, MAX_PATH,
         index == 0 ? "12V-2x6 current spread" :
         index == 1 ? "12V-2x6 average spread" :
         index == 2 ? "12V-2x6 maximum spread" :
                      "12V-2x6 active port");
    put_text(desc->szUnits, MAX_PATH, index == 3 ? "" : "A");
    put_text(desc->szFormat, MAX_PATH, index == 3 ? "%.0f" : "%.3f");
    put_text(desc->szGroup, MAX_PATH, "AmpSpread Afterburner Edition");
    desc->dwID = ID_PLUGIN_PSU;
    desc->dwInstance = 0;
    desc->fltMinLimit = 0.0f;
    /* Default visual ranges; Afterburner may retain a user's previously saved graph limits. */
    desc->fltMaxLimit = index == 0 ? 1.0f :
                        index == 1 ? 0.5f :
                        index == 2 ? 1.0f : 2.0f;
    put_text(desc->szNameTemplate, MAX_PATH, "");
    put_text(desc->szGroupTemplate, MAX_PATH, "");
    return TRUE;
}

EXPORT float __cdecl GetSourceData(DWORD index) {
    STATS *s;
    if (index > 3) return FLOAT_UNAVAILABLE;
    update_spread();
    if (!dataValid || activeBank < 0) return FLOAT_UNAVAILABLE;
    s = &stats[activeBank];
    if (index == 3) return (float)(activeBank + 1);
    if (index == 0) return s->current;
    if (index == 1) return s->samples ? (float)(s->sum / (double)s->samples) : FLOAT_UNAVAILABLE;
    return s->maximum;
}

EXPORT void __cdecl Uninit(void) { clear_stats(); }