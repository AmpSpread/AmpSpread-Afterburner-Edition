#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define __declspec(x)
#define __stdcall
#define __cdecl
#include "../src/ampspread.c"

static unsigned char mapped[32 + 12 * 1324];
static DWORD tick = 1000;
static int mapEnabled = 1;
HANDLE WINAPI OpenFileMappingA(DWORD flags, BOOL inherit, LPCSTR name) {
    (void)inherit;
    if (flags != FILE_MAP_READ || strcmp(name, "MAHMSharedMemory") != 0 || !mapEnabled) return 0;
    return (HANDLE)1;
}
LPVOID WINAPI MapViewOfFile(HANDLE h, DWORD flags, DWORD a, DWORD b, unsigned long requested) {
    (void)a; (void)b;
    if (h != (HANDLE)1 || flags != FILE_MAP_READ || requested > sizeof(mapped)) return 0;
    return mapped;
}
BOOL WINAPI UnmapViewOfFile(const void *p) { return (p == mapped); }
BOOL WINAPI CloseHandle(HANDLE h) { return h == (HANDLE)1; }
DWORD WINAPI GetTickCount(void) { return tick; }
int WINAPI MessageBoxA(HWND h, LPCSTR text, LPCSTR title, UINT flags) {
    (void)h; (void)text; (void)title; (void)flags;
    return IDYES;
}

static void entry(int b, int pin, float value) {
    char *p = (char *)mapped + 32 + (b * 6 + pin) * 1324;
    snprintf(p, 260, "PSU 12VHPWR%d_%d current", b + 1, pin + 1);
    strcpy(p + 260, "A");
    *(float *)(p + 1300) = value;
}
static void setup(void) {
    MAHM_HEADER *h = (MAHM_HEADER *)mapped;
    memset(mapped, 0, sizeof(mapped));
    h->signature = 0x4D41484Du; h->version = 0x00020003u;
    h->headerSize = 32; h->numEntries = 12; h->entrySize = 1324;
    for (int b=0; b<2; b++) for (int i=0; i<6; i++) entry(b,i,b ? 0.1f : 1.0f);
    clear_stats();
}
static void assert_near(float actual, float expected) {
    if (fabsf(actual-expected) > 0.00001f) {
        fprintf(stderr, "FAIL got %.7f expected %.7f\n", actual, expected);
        exit(1);
    }
}
int main(void) {
    MONITORING_SOURCE_DESC d = {0};
    d.dwVersion = 0x10000;
    setup();
    if (GetSourcesNum() != 4 || !GetSourceDesc(0,&d) || strcmp(d.szFormat,"%.3f")) return 1;
    assert_near(d.fltMinLimit,0.0f); assert_near(d.fltMaxLimit,1.0f);
    if(!GetSourceDesc(1,&d)) return 9;
    assert_near(d.fltMinLimit,0.0f); assert_near(d.fltMaxLimit,0.5f);
    if(!GetSourceDesc(2,&d)) return 10;
    assert_near(d.fltMinLimit,0.0f); assert_near(d.fltMaxLimit,1.0f);
    if(!GetSourceDesc(3,&d)) return 11;
    assert_near(d.fltMinLimit,0.0f); assert_near(d.fltMaxLimit,2.0f);
    if (GetSourceDesc(999,&d)) return 2;
    for(int i=0;i<6;i++) entry(0,i,1.1f);
    entry(0,0,1.2f); entry(0,1,1.1f); entry(0,2,1.3f);
    assert_near(GetSourceData(0),0.2f);
    assert_near(GetSourceData(1),0.2f);
    assert_near(GetSourceData(2),0.2f);
    assert_near(GetSourceData(3),1.0f);
    if(stats[0].samples!=1) return 3;
    tick+=250;
    entry(0,0,1.0f); entry(0,1,1.0f); entry(0,2,1.4f);
    assert_near(GetSourceData(0),0.4f);
    assert_near(GetSourceData(1),0.3f);
    assert_near(GetSourceData(2),0.4f);
    if(stats[0].samples!=2) return 4;
    tick+=250;
    for(int i=0;i<6;i++) entry(1,i,2.0f);
    entry(1,3,2.6f);
    assert_near(GetSourceData(0),0.6f);
    assert_near(GetSourceData(1),0.6f);
    assert_near(GetSourceData(2),0.6f);
    assert_near(GetSourceData(3),2.0f);
    tick+=250;
    if(!SetupSource(0xFFFFFFFFu,(HWND)1)) return 5;
    assert_near(GetSourceData(1),0.6f);
    if(stats[1].samples!=1) return 6;
    tick+=250;
    mapEnabled=0;
    if(GetSourceData(0)!=FLOAT_UNAVAILABLE) return 7;
    mapEnabled=1;
    tick+=250;
    ((MAHM_HEADER*)mapped)->numEntries=9999;
    if(GetSourceData(0)!=FLOAT_UNAVAILABLE) return 8;
    puts("PASS: ABI descriptions and default graph limits, 6-pin spread, average, max, port selection, reset, missing map, bounds check");
    return 0;
}
