/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the phrase pool's flash object (phr.c + phr_stor.c against storage.c's A/B pair: OBJ_PHRASES at
 * 0xFA000 / 0xFB000, the pattern bank's last two sectors of before): a fresh unit boots an empty pool, a save comes
 * back over a power cycle, a write torn at any point leaves the copy before in charge, a destroyed copy falls back,
 * clearing persists. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t nor[0x100000];
static int fail_after = -1;            /* torn-write injection: stop after N programs */
static uint32_t io_calls;

static int st_read(uint32_t off, void *dst, uint32_t n) { io_calls++; memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    io_calls++;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    io_calls++;
    if (fail_after == 0)
        return -9;
    if (fail_after > 0)
        fail_after--;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint8_t flash_ok = 1;
#define FELUCCA_FLASH 1
#include "../firmware/src/storage.c"
#include "../firmware/src/phr.c"
#include "../firmware/src/phr_stor.c"

static int check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    int bad = 0;
    uint32_t o, c;
    memset(nor, 0xFF, sizeof nor);
    phr_boot();
    bad += check("a fresh unit boots an empty pool", phr_pool[0] == 0 && phr_pool[PHR_LEN - 1u] == 0);
    o = st_sector(OBJ_PHRASES, 0) == 0xFA000u && st_sector(OBJ_PHRASES, 1) == 0xFB000u;
    for (c = 0; o && c < (uint32_t)OBJ_COUNT; c++)
        if (c != OBJ_PHRASES)
            o = st_sector(c, 0) != 0xFA000u && st_sector(c, 0) != 0xFB000u &&
                st_sector(c, 1) != 0xFA000u && st_sector(c, 1) != 0xFB000u;
    bad += check("the pair sits at the bank's last two sectors, apart from every object", o);
    phr_set(0, 0, 0x4F); phr_set(3, 5, 0x29);
    phr_persist();
    bad += check("a save goes out and clears the dirty flag", !phr_dirty);
    memset(phr_pool, 0, sizeof phr_pool);          /* the RAM gone (a power cycle) */
    phr_boot();
    bad += check("the pool comes back from flash", phr_step(0, 0) == 0x4F && phr_step(3, 5) == 0x29);
    phr_set(0, 1, 0x2A); phr_persist();            /* the other copy now */
    phr_set(0, 2, 0x33); phr_persist();            /* back to the first */
    fail_after = 1;                                /* the header write torn off */
    phr_set(0, 3, 0x11);
    phr_persist();
    bad += check("a torn save keeps the dirty flag", phr_dirty);
    phr_boot();
    bad += check("the torn copy loses, the one before stays in charge",
                 phr_step(0, 0) == 0x4F && phr_step(0, 1) == 0x2A && phr_step(0, 2) == 0x33 && phr_step(0, 3) == 0);
    fail_after = -1;
    phr_set(0, 3, 0x11);                           /* the lost edit, again: the boot above cleared the dirty flag */
    phr_persist();                                 /* (both copies valid again) */
    memset(nor + 0xFA000u, 0xFF, 4096);            /* the copy A sector destroyed */
    phr_boot();
    bad += check("a destroyed copy falls back to the other", phr_step(0, 2) == 0x33 && phr_step(0, 3) == 0x11);
    phr_clear(2);
    phr_persist();
    bad += check("a phrase cleared persists", !phr_dirty);
    memset(phr_pool, 0xFF, sizeof phr_pool);
    phr_boot();
    bad += check("  .. and stays empty after a power cycle",
                 phr_step(2, 0) == 0 && phr_step(2, 15) == 0 && phr_step(0, 2) == 0x33);
    return bad;
}
