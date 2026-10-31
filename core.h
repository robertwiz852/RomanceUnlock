/* core.h - platform-independent record finder for Crimson Desert's item table.
 *
 * A raw item record (as found in iteminfo*.staticinfobody) looks like:
 *     u32 name_len | name bytes | u32 item_key | 00 ff ff | u32 max_stack | ...
 * The stack value is the u32 right after the 3-byte marker 00 ff ff.
 * We find records by looking for the marker and checking backwards that a valid
 * length-prefixed identifier + plausible key precede it.
 */
#ifndef CORE_H
#define CORE_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

#define MAX_KEY      0x0FFFFFFFu /* item keys seen are well below this          */
#define MAX_NAME     160         /* longest item name we accept                 */
#define SANE_STACK   100000u     /* vanilla limits above this: leave alone      */
#define BACK_MARGIN  180         /* bytes of lookback a scan buffer must carry  */
#define FWD_MARGIN   160         /* bytes of lookahead (dump context + stack)   */

static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

static int is_name_char(u8 c, int first) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') return 1;
    return !first && (c >= '0' && c <= '9');
}

/* i = index of the 0x00 of "00 ff ff". Returns 1 if a raw record's stack field is at buf+i+3. */
static int match_record(const u8 *buf, u64 len, u64 i, u32 *key_out, u32 *namelen_out) {
    if (i < 12 || i + 7 > len) return 0;
    if (buf[i] != 0x00 || buf[i + 1] != 0xff || buf[i + 2] != 0xff) return 0;
    u32 key = rd32(buf + i - 4);
    if (key == 0 || key > MAX_KEY) return 0;
    for (u32 nl = 3; nl <= MAX_NAME; nl++) {
        if (i < 8 + nl) break;
        u64 ns = i - 4 - nl;
        if (rd32(buf + ns - 4) != nl) continue;
        int ok = 1;
        for (u32 k = 0; k < nl; k++) if (!is_name_char(buf[ns + k], k == 0)) { ok = 0; break; }
        if (ok) { *key_out = key; *namelen_out = nl; return 1; }
    }
    return 0;
}

/* New stack value for an old one, or the old value if it must be left alone. */
static u32 new_stack(u32 v, u32 mult, u32 cap) {
    if (v <= 1 || v > SANE_STACK) return v;
    u64 n = (u64)v * mult;
    u64 limit = cap > v ? cap : v;
    return (u32)(n < limit ? n : limit);
}

#endif
