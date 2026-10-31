#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NATIVE_TEST
#define API
#define __stdcall
#define DllMain DllMain_unused
#include "romance_unlock.c"
#include "stubs.h"
static void w64(u8 *p, u64 v) { wr32(p, (u32)v); wr32(p + 4, (u32)(v >> 32)); }
int main(void) {
    u64 cap = 16u << 20; u8 *h = aligned_alloc(4096, cap); memset(h, 0, cap);
    const char *names[6] = { "NHW_Unique_Joan_652", "NHW_Citizen_1", "NHW_Child_Citizen_1", "Riding_Horse_01", "NHM_Slave_5", "NHW_Unique_Tanya_3001" };
    u8 init[6] = { 0, 1, 0, 0, 0, 0 }; u8 *ob[6]; u64 pos = 4096;
    for (int i = 0; i < 6; i++) {
        u32 nl = strlen(names[i]); u8 *n = h + pos; pos += 128;
        w64(n, (u64)(uptr)n + 24); wr32(n + 8, nl); wr32(n + 12, 0xFFFFFFFFu); wr32(n + 16, 1); memcpy(n + 24, names[i], nl);
        u8 *o = h + (8u << 20) + i * 1024 + (i == 5 ? (3u << 20) : 0); w64(o, (u64)(uptr)n); wr32(o + 8, 1002980 + i);
        static const u8 fp[] = { 0, 1, 1, 0, 1, 1, 0 }; memcpy(o + 12, fp, 7); o[36] = init[i]; ob[i] = o;
    }
    nfr = 0; fr[nfr++] = (FR){ (uptr)h, cap, 0x20000, 0x04 }; g_buf = VirtualAlloc(0, BUFSZ, 0, 0);
    int bad = 0;
    for (g_mode = 1; g_mode <= 2; g_mode++) {
        for (int i = 0; i < 6; i++) ob[i][36] = init[i]; memset(g_done, 0, sizeof g_done); g_fast = 0; g_nhot = 0;
        Stats s; memset(&s, 0, sizeof s); scan_all(&s);
        printf("mode %d: set=%llu joan=%u citizen=%u child=%u horse=%u slave=%u tanya=%u\n", g_mode, s.set, ob[0][36], ob[1][36], ob[2][36], ob[3][36], ob[4][36], ob[5][36]);
        if (g_mode == 1) bad |= !(ob[0][36] == 1 && !ob[2][36] && !ob[3][36] && !ob[4][36] && !ob[5][36]);
        else bad |= !(ob[0][36] && ob[1][36] && !ob[2][36] && !ob[3][36] && ob[4][36] && ob[5][36]);
        ob[0][36] = 0; g_fast = 1; Stats q; memset(&q, 0, sizeof q); memset(g_done, 0, sizeof g_done); scan_all(&q);
        printf("  quick scan re-unlocked: %llu (joan=%u)\n", q.set, ob[0][36]); bad |= ob[0][36] != 1;
    }
    g_mode = 3; for (int i = 0; i < 6; i++) ob[i][36] = (i == 1 || i == 4) ? 1 : 0; memset(g_done, 0, sizeof g_done); g_fast = 0; g_nhot = 0;
    { Stats s; memset(&s, 0, sizeof s); scan_all(&s); printf("mode 3: set=%llu citizen=%u slave=%u (want 0 0)\n", s.set, ob[1][36], ob[4][36]); bad |= ob[1][36] != 0 || ob[4][36] != 0 || s.set != 2; }
    g_mode = 1; g_npoke = 2; g_poke_off[0] = 34; g_poke_val[0] = 1; g_poke_off[1] = 39; g_poke_val[1] = 3;
    ob[0][36] = 0; ob[0][34] = 0; ob[0][39] = 0; memset(g_done, 0, sizeof g_done); g_fast = 0; g_nhot = 0;
    { Stats s; memset(&s, 0, sizeof s); scan_all(&s); printf("pokes: flag=%u +34=%u +39=%u (want 1 1 3); tanya +34=%u\n", ob[0][36], ob[0][34], ob[0][39], ob[5][34]); bad |= ob[0][36] != 1 || ob[0][34] != 1 || ob[0][39] != 3 || ob[5][34] != 0; }
    /* mode 4: camp signature (34=0, 36=0, 39=0, 62=4, name has _Unique_) */
    {
        const char *nn[4] = { "NHW_Unique_Joan_652", "NOM_Unique_Kamu_707", "NHW_Unique_Tessie_3627", "NTM_Unique_Rhett_441" };
        u8 *o[4]; u64 p2 = 40000;
        for (int i = 0; i < 4; i++) {
            u32 nl = strlen(nn[i]); u8 *n = h + p2; p2 += 128;
            w64(n, (u64)(uptr)n + 24); wr32(n + 8, nl); wr32(n + 12, 0xFFFFFFFFu); wr32(n + 16, 1); memcpy(n + 24, nn[i], nl);
            u8 *ob2 = h + (12u << 20) + i * 1024; memset(ob2, 0, 1024); w64(ob2, (u64)(uptr)n); wr32(ob2 + 8, 5000 + i);
            static const u8 fp[] = { 0, 1, 1, 0, 1, 1, 0 }; memcpy(ob2 + 12, fp, 7);
            ob2[36] = 0; ob2[39] = (i >= 2) ? 3 : 0; ob2[62] = (i >= 2) ? 3 : 4; ob2[34] = (i >= 2); if (i == 3) { ob2[39] = 3; ob2[62] = 3; ob2[34] = 1; } o[i] = ob2;
        }
        g_mode = 4; g_npoke = 0; g_poke_off[g_npoke] = 62; g_poke_val[g_npoke] = 3; g_npoke++;
        memset(g_done, 0, sizeof g_done); g_fast = 0; g_nhot = 0;
        Stats s; memset(&s, 0, sizeof s); scan_all(&s);
        printf("mode 4: joan flag=%u +62=%u | smith flag=%u +62=%u | tessie flag=%u +62=%u (want 1 3 | 1 3 | 0 3)  set=%llu\n", o[0][36], o[0][62], o[1][36], o[1][62], o[2][36], o[2][62], s.set);
        bad |= !(o[0][36] == 1 && o[0][62] == 3 && o[1][36] == 1 && o[1][62] == 3 && o[2][36] == 0 && o[3][36] == 0);
    }
    return bad;
}
