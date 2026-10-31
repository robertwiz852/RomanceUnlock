/* romance_unlock.c - "OurStackTool" 0.3: an ASI plugin for Crimson Desert (build 2.04 + DLC).
 *
 * What we learned (see logs of stage 1/2): the game does not keep the raw item table in memory.
 * It keeps one object per item:   [u64 ptr to the item's name node][u32 key][00 00 ff ff][u32 max_stack] ...
 * where the name node is   [u64 ptr to chars][u32 len][ff ff ff ff][u32 1][u32 0] + chars.
 * We find those objects by that shape (validating the name node), and - only if Patch=1 - raise max_stack.
 *
 * Safety: ReadProcessMemory/WriteProcessMemory on our own process (they fail instead of crashing);
 * code regions are never touched; each address is patched at most once; Patch defaults to 1 in a freshly written ini.
 * Runs only inside CrimsonDesert.exe. Nothing on disk is changed.
 * Build: build.sh (clang + lld, no CRT, no Windows SDK).
 */
#include <stddef.h>
#include "core.h"

typedef u64 uptr;
typedef void *HANDLE;
typedef int BOOL;
typedef unsigned long DWORD;
typedef wchar_t WCHAR;
#ifndef API
#define API __declspec(dllimport) __stdcall
#endif

typedef struct {
    void *BaseAddress; void *AllocationBase; DWORD AllocationProtect; u16 PartitionId;
    u64 RegionSize; DWORD State; DWORD Protect; DWORD Type;
} MBI;

API HANDLE CreateFileW(const WCHAR *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
API BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
API BOOL CloseHandle(HANDLE);
API DWORD GetModuleFileNameW(void *, WCHAR *, DWORD);
API HANDLE CreateThread(void *, u64, DWORD(__stdcall *)(void *), void *, DWORD, DWORD *);
API void Sleep(DWORD);
API u64 VirtualQuery(const void *, MBI *, u64);
API void *VirtualAlloc(void *, u64, DWORD, DWORD);
API HANDLE GetCurrentProcess(void);
API BOOL ReadProcessMemory(HANDLE, const void *, void *, u64, u64 *);
API BOOL WriteProcessMemory(HANDLE, void *, const void *, u64, u64 *);
API DWORD GetPrivateProfileIntW(const WCHAR *, const WCHAR *, int, const WCHAR *);
API DWORD GetFileAttributesW(const WCHAR *);
API BOOL DisableThreadLibraryCalls(void *);
API BOOL SetThreadPriority(HANDLE, int);
API HANDLE GetCurrentThread(void);
API DWORD GetTickCount(void);
API DWORD GetCurrentProcessId(void);

#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_IMAGE 0x1000000
#define PAGE_READWRITE 0x04
#define PAGE_NOACCESS 0x01
#define PAGE_GUARD 0x100
#define PAGE_EXEC_ANY 0xF0
#define INVALID_HANDLE ((HANDLE)(long long)-1)

/* ---- tiny runtime (no CRT) ---- */
#ifndef NATIVE_TEST
void *memcpy(void *d, const void *s, size_t n) __attribute__((optnone));
void *memcpy(void *d, const void *s, size_t n) { u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, size_t n) __attribute__((optnone));
void *memset(void *d, int c, size_t n) { u8 *a = d; while (n--) *a++ = (u8)c; return d; }
#endif

/* ---- logging ---- */
static WCHAR g_log_path[560], g_ini_path[560];
static char g_lb[4096];
static int g_ll;

static void lflush(void) {
    if (!g_ll) return;
    HANDLE h = CreateFileW(g_log_path, 0x00000004 /*FILE_APPEND_DATA*/, 1 /*SHARE_READ*/, 0, 4 /*OPEN_ALWAYS*/, 0x80, 0);
    if (h != INVALID_HANDLE) { DWORD w; WriteFile(h, g_lb, (DWORD)g_ll, &w, 0); CloseHandle(h); }
    g_ll = 0;
}
static void lc(char c) { if (g_ll >= (int)sizeof g_lb - 1) lflush(); g_lb[g_ll++] = c; }
static void ls(const char *s) { while (*s) lc(*s++); }
static void lu(u64 v) { char t[24]; int n = 0; if (!v) t[n++] = '0'; while (v) { t[n++] = (char)('0' + v % 10); v /= 10; } while (n) lc(t[--n]); }
static void lx(u64 v, int digits) { for (int i = digits - 1; i >= 0; i--) lc("0123456789abcdef"[(v >> (i * 4)) & 15]); }
static void lhex(const u8 *p, int n) { for (int i = 0; i < n; i++) { lx(p[i], 2); lc(' '); } }

static u64 rd64(const u8 *p) { return (u64)rd32(p) | ((u64)rd32(p + 4) << 32); }


static void make_path(WCHAR *out, const WCHAR *mod, const WCHAR *ext) {
    int n = 0, dot = -1;
    while (mod[n]) { out[n] = mod[n]; if (mod[n] == L'.') dot = n; n++; }
    if (dot < 0) dot = n;
    int k = dot; for (const WCHAR *e = ext; *e; e++) out[k++] = *e;
    out[k] = 0;
}

/* ---- RomanceUnlock ----
 * NPC objects in memory look like: [u64 ptr to name node][u32 key][record bytes ...]
 * CharacterInfo._isMarriageable is the byte at object+36 (found by comparing 16 NPC objects). We set it to 1.
 */
typedef struct { u64 regions, bytes, npcs, targets, set, fail, already; } Stats;
static void *g_module;
static u8 *g_buf;
static int g_mode;
#define NPOKE 8
static int g_poke_off[NPOKE], g_poke_val[NPOKE]; static int g_npoke;
#define CHUNK (1u << 20)
#define BUFSZ (CHUNK + 1100 + 64)
#define DONE_N 16384
static uptr g_done[DONE_N];
#define HOT_N 4096
static u64 g_hot[HOT_N]; static u32 g_nhot; static int g_fast;
static u8 g_nb[24 + MAX_NAME + 8];

static int done_has(uptr a) { u32 h = (u32)((a >> 2) * 2654435761u) & (DONE_N - 1); while (g_done[h]) { if (g_done[h] == a) return 1; h = (h + 1) & (DONE_N - 1); } return 0; }
static void done_add(uptr a) { static u32 n; if (n > DONE_N * 3 / 4) return; u32 h = (u32)((a >> 2) * 2654435761u) & (DONE_N - 1); while (g_done[h]) h = (h + 1) & (DONE_N - 1); g_done[h] = a; n++; }
static void hot_add(u64 id) { for (u32 i = 0; i < g_nhot; i++) if (g_hot[i] == id) return; if (g_nhot < HOT_N) g_hot[g_nhot++] = id; }
static int hot_near(u64 start) { u64 lo = start >> 20, hi = (start + CHUNK + 1100) >> 20; for (u32 i = 0; i < g_nhot; i++) if (g_hot[i] + 1 >= lo && g_hot[i] <= hi + 1) return 1; return 0; }
static int is_user_ptr(u64 p) { return p >= 0x10000 && p < 0x7FFFFFFF0000ULL && (p & 7) == 0; }
static int eqstr(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == 0 && *b == 0; }
static int hasstr(const char *h, const char *n) { for (; *h; h++) { const char *x = h, *y = n; while (*x && *y && *x == *y) { x++; y++; } if (!*y) return 1; } return 0; }

static u32 read_name_node(HANDLE self, u64 p, char *out) {
    u64 got = 0;
    if (!ReadProcessMemory(self, (void *)p, g_nb, 24, &got) || got != 24) return 0;
    if (rd64(g_nb) != p + 24 || rd32(g_nb + 12) != 0xFFFFFFFFu || rd32(g_nb + 16) != 1) return 0;
    u32 len = rd32(g_nb + 8);
    if (len < 3 || len > MAX_NAME) return 0;
    if (!ReadProcessMemory(self, (void *)(p + 24), g_nb + 24, len + 1, &got) || got != len + 1) return 0;
    if (g_nb[24 + len] != 0) return 0;
    for (u32 k = 0; k < len; k++) { u8 c = g_nb[24 + k]; if (!is_name_char(c, k == 0)) return 0; out[k] = (char)c; }
    out[len] = 0;
    return len;
}

static void scan_region(uptr base, u64 size, Stats *st) {
    HANDLE self = GetCurrentProcess();
    char name[MAX_NAME + 1];
    for (u64 off = 0; off < size; off += CHUNK) {
        if (g_fast && !hot_near(base + off)) continue;
        u64 hi = off + CHUNK + 1100; if (hi > size) hi = size;
        u64 got = 0;
        if (!ReadProcessMemory(self, (void *)(base + off), g_buf, hi - off, &got) || got != hi - off) continue;
        u64 blen = hi - off, end = off + CHUNK < size ? off + CHUNK : size;
        st->bytes += end - off;
        for (u64 a = off; a < end && a - off + 40 <= blen; a += 8) {
            const u8 *o = g_buf + (a - off);
            if (!(o[12] == 0 && o[13] == 1 && o[14] == 1 && o[15] == 0 && o[16] == 1 && o[17] == 1 && o[18] == 0 && o[36] <= 1)) continue;
            u64 np = rd64(o); u32 nk = rd32(o + 8);
            if (!is_user_ptr(np) || nk == 0 || nk > MAX_KEY) continue;
            u32 nl = read_name_node(self, np, name);
            if (!nl || name[0] != 'N' || (name[2] != 'W' && name[2] != 'M') || name[3] != '_') continue;
            if (name[1] != 'H' && !((g_mode == 4 || g_mode == 5) && (name[1] == 'O' || name[1] == 'D'))) continue;  /* camp mode also covers NOM_/NDM_ camp characters such as Kamu */
            st->npcs++; hot_add((base + a) >> 20);
            int want = g_mode == 1 ? eqstr(name, "NHW_Unique_Joan_652") : g_mode == 3 ? 1 : g_mode == 5 ? !hasstr(name, "Child") : g_mode == 4 ? (o[34] == 0 && o[39] == 0 && o[62] == 4 && hasstr(name, "_Unique_")) : !hasstr(name, "Child");
            u8 tv = g_mode == 3 ? 0 : 1; /* mode 3 = DIAGNOSTIC: turn the flag OFF for everyone */
            if (!want) continue;
            st->targets++;
            if ((g_mode == 4 || g_mode == 5) && (eqstr(name, "NHW_Unique_Joan_652") || eqstr(name, "NHM_Unique_GreyMane_Smithy") || eqstr(name, "NHM_Unique_Oliver_649")) && a - off + 520 <= blen && !done_has(base + a + 1)) {
                done_add(base + a + 1); ls("  DUMP "); ls(name); ls("\r\n");
                for (int q = 0; q < 512; q += 32) { ls("   "); lu((u64)q); ls(":"); for (int z = 0; z < 32; z++) { ls(" "); lx(o[q + z], 2); } ls("\r\n"); }
            }
            if (g_mode == 1 || g_mode == 4 || g_mode == 5) for (int pk = 0; pk < g_npoke; pk++) {   /* experimental extra bytes for the single target NPC */
                int po = g_poke_off[pk]; if (a - off + (u64)po + 1 > blen) continue;
                if (o[po] != (u8)g_poke_val[pk]) {
                    u8 pv = (u8)g_poke_val[pk]; u64 w2 = 0;
                    if (WriteProcessMemory(self, (void *)(base + a + po), &pv, 1, &w2) && w2 == 1) { ls("  poke +"); lu((u64)po); ls(" = "); lu(pv); ls(" (was "); lu(o[po]); ls(") on "); ls(name); ls("\r\n"); }
                }
            }
            if (o[36] == tv) { st->already++; continue; }
            uptr addr = base + a + 36; u8 one = tv; u64 w = 0;
            if (done_has(addr)) continue;
            if (WriteProcessMemory(self, (void *)addr, &one, 1, &w) && w == 1) {
                st->set++; done_add(addr);
                if (g_mode == 1 || st->set <= 40) { ls(g_mode == 3 ? "  locked (test): " : "  unlocked: "); ls(name); ls(" key="); lu(nk); ls(" at 0x"); lx(addr, 12); ls("\r\n"); }
            } else st->fail++;
        }
    }
}

static void scan_all(Stats *st) {
    uptr addr = 0x10000; MBI m;
    while (addr < 0x7FFFFFFF0000ULL) {
        if (!VirtualQuery((void *)addr, &m, sizeof m)) break;
        uptr base = (uptr)m.BaseAddress; u64 size = m.RegionSize;
        if (!size || base + size < addr) break;
        addr = base + size;
        if (m.State != MEM_COMMIT || m.Type == MEM_IMAGE) continue;
        if (m.Protect & (PAGE_GUARD | PAGE_NOACCESS | PAGE_EXEC_ANY)) continue;
        if (!(m.Protect & 0x0C)) continue;
        if (base <= (uptr)g_buf && (uptr)g_buf < base + size) continue;
        st->regions++;
        scan_region(base, size, st);
        Sleep(0);
    }
}

static void write_default_ini(void) {
    static const char ini[] =
        "; RomanceUnlock settings (restart the game after editing)\r\n[Settings]\r\n"
        "; 1 = unlock romance for Joan only (default). 4 = unlock the whole Graymane camp crew (63 named NPCs incl. Joan). 0 = off.\r\n"
        "; 2 = all adult human NPCs (not children), experimental.\r\n"
        "; 3 = DIAGNOSTIC: switch romance OFF for every NPC (to check the game reads this value). Use only to test.\r\n"
        "Mode=1\r\n"
        "InitialDelaySec=30\r\nIntervalSec=15\r\nFullScanEverySec=300\r\n";
    HANDLE h = CreateFileW(g_ini_path, 0x40000000, 0, 0, 1, 0x80, 0);
    if (h != INVALID_HANDLE) { DWORD w; WriteFile(h, ini, sizeof ini - 1, &w, 0); CloseHandle(h); }
}

static DWORD __stdcall worker(void *arg) {
    (void)arg;
    SetThreadPriority(GetCurrentThread(), -2);
    WCHAR mod[560];
    if (!GetModuleFileNameW(g_module, mod, 540)) return 0;
    make_path(g_log_path, mod, L".log");
    make_path(g_ini_path, mod, L".ini");
    {
        WCHAR exe[560]; int n = 0, last = 0, ok = 0;
        if (GetModuleFileNameW(0, exe, 540)) { while (exe[n]) { if (exe[n] == L'\\' || exe[n] == L'/') last = n + 1; n++; }
            static const char want[] = "crimsondesert.exe"; int k = 0; ok = 1;
            for (; want[k]; k++) { WCHAR c = exe[last + k]; if (c >= L'A' && c <= L'Z') c = (WCHAR)(c + 32); if (c != (WCHAR)want[k]) { ok = 0; break; } }
            if (ok && exe[last + k]) ok = 0; }
        if (!ok) return 0;
    }
    if (GetFileAttributesW(g_ini_path) == 0xFFFFFFFFu) write_default_ini();
    g_mode = (int)GetPrivateProfileIntW(L"Settings", L"Mode", 1, g_ini_path);
    for (int pk = 0; pk < NPOKE; pk++) {
        WCHAR ko[16] = L"Poke1Offset", kv[16] = L"Poke1Value"; ko[4] = kv[4] = (WCHAR)(L'1' + pk);
        int off = (int)GetPrivateProfileIntW(L"Settings", ko, -1, g_ini_path), val = (int)GetPrivateProfileIntW(L"Settings", kv, -1, g_ini_path);
        if (off >= 8 && off < 900 && val >= 0 && val <= 255 && off != 36) { g_poke_off[g_npoke] = off; g_poke_val[g_npoke] = val; g_npoke++; }
    }
    { int have = 0; for (int pk = 0; pk < g_npoke; pk++) if (g_poke_off[pk] == 62) have = 1;
      if (!have && g_npoke < NPOKE) { g_poke_off[g_npoke] = 62; g_poke_val[g_npoke] = 3; g_npoke++; } } /* needed for the flirt option on camp NPCs (4 -> 3) */
    u32 delay = GetPrivateProfileIntW(L"Settings", L"InitialDelaySec", 30, g_ini_path);
    u32 interval = GetPrivateProfileIntW(L"Settings", L"IntervalSec", 15, g_ini_path); if (interval < 5) interval = 5;
    u32 fullsec = GetPrivateProfileIntW(L"Settings", L"FullScanEverySec", 300, g_ini_path);
    g_buf = VirtualAlloc(0, BUFSZ, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ls("=== RomanceUnlock 0.6 pid "); lu(GetCurrentProcessId()); ls(" Mode="); lu((u64)g_mode); ls(" pokes="); lu((u64)g_npoke); ls("\r\n"); lflush();
    if (!g_buf || g_mode == 0) { ls("off or no buffer, exiting\r\n"); lflush(); return 0; }
    DWORD t0 = GetTickCount(), last_full = 0; u32 full_no = 0;
    Sleep(delay * 1000);
    for (;;) {
        int full = full_no == 0 || g_nhot == 0 || GetTickCount() - last_full >= fullsec * 1000u;
        Stats st; memset(&st, 0, sizeof st); g_fast = !full;
        DWORD ts = GetTickCount();
        scan_all(&st);
        if (full) {
            full_no++; last_full = GetTickCount();
            ls("[t+"); lu((GetTickCount() - t0) / 1000); ls("s] FULL scan "); lu(full_no); ls(" ("); lu(GetTickCount() - ts); ls(" ms): NPC objects "); lu(st.npcs);
            ls(", in target set "); lu(st.targets); ls(", newly unlocked "); lu(st.set); ls(", already flagged "); lu(st.already); ls(", write failures "); lu(st.fail); ls("\r\n");
        } else if (st.set || st.fail) {
            ls("[t+"); lu((GetTickCount() - t0) / 1000); ls("s] quick scan: newly unlocked "); lu(st.set); ls(", failures "); lu(st.fail); ls("\r\n");
        }
        lflush();
        Sleep(interval * 1000);
    }
    return 0;
}

BOOL __stdcall DllMain(void *inst, DWORD reason, void *reserved) {
    (void)reserved;
    if (reason == 1) { g_module = inst; DisableThreadLibraryCalls(inst); HANDLE t = CreateThread(0, 0, worker, 0, 0, 0); if (t) CloseHandle(t); }
    return 1;
}
