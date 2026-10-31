/* stubs.h - fake Win32 + fake memory regions for native tests (include after stack_tool.c) */
typedef struct { uptr base; u64 size; DWORD type, prot; } FR;
static FR fr[8]; static int nfr;
u64 VirtualQuery(const void *a, MBI *m, u64 n) {
    (void)n; uptr x = (uptr)a; memset(m, 0, sizeof *m);
    for (int i = 0; i < nfr; i++) if (x >= fr[i].base && x < fr[i].base + fr[i].size) {
        m->BaseAddress = (void *)fr[i].base; m->RegionSize = fr[i].size; m->State = MEM_COMMIT; m->Type = fr[i].type; m->Protect = fr[i].prot; return 1; }
    uptr next = ~(uptr)0; for (int i = 0; i < nfr; i++) if (fr[i].base > x && fr[i].base < next) next = fr[i].base;
    if (next == ~(uptr)0) return 0;
    m->BaseAddress = (void *)x; m->RegionSize = next - x; m->State = 0x10000; return 1;
}
void *VirtualAlloc(void *a, u64 n, DWORD t, DWORD p) { (void)a; (void)t; (void)p; fr[nfr].base = 0; return calloc(1, n); }
HANDLE GetCurrentProcess(void) { return 0; }
static int in_fr(uptr a, u64 n) { for (int i = 0; i < nfr; i++) if (a >= fr[i].base && a + n <= fr[i].base + fr[i].size) return 1; return 0; }
BOOL ReadProcessMemory(HANDLE h, const void *s, void *d, u64 n, u64 *got) { (void)h; if (!in_fr((uptr)s, n)) { *got = 0; return 0; } memcpy(d, s, n); *got = n; return 1; }
BOOL WriteProcessMemory(HANDLE h, void *d, const void *s, u64 n, u64 *w) { (void)h; if (!in_fr((uptr)d, n)) { *w = 0; return 0; } memcpy(d, s, n); *w = n; return 1; }
void Sleep(DWORD x) { (void)x; }
/* unused in this test */
HANDLE CreateFileW(const WCHAR *a, DWORD b, DWORD c, void *d, DWORD e, DWORD f, HANDLE g) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return (HANDLE)1; }
BOOL WriteFile(HANDLE a, const void *b, DWORD c, DWORD *d, void *e) { (void)a;(void)d;(void)e; fwrite(b, 1, c, stdout); return 1; }
BOOL CloseHandle(HANDLE a) { (void)a; return 1; }
DWORD GetModuleFileNameW(void *a, WCHAR *b, DWORD c) { (void)a;(void)b;(void)c; return 0; }
HANDLE CreateThread(void *a, u64 b, DWORD (*c)(void *), void *d, DWORD e, DWORD *f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 0; }
DWORD GetPrivateProfileIntW(const WCHAR *a, const WCHAR *b, int c, const WCHAR *d) { (void)a;(void)b;(void)d; return (DWORD)c; }
DWORD GetFileAttributesW(const WCHAR *a) { (void)a; return 0; }
BOOL DisableThreadLibraryCalls(void *a) { (void)a; return 1; }
BOOL SetThreadPriority(HANDLE a, int b) { (void)a;(void)b; return 1; }
HANDLE GetCurrentThread(void) { return 0; }
DWORD GetTickCount(void) { return 0; }

DWORD GetCurrentProcessId(void) { return 4242; }
