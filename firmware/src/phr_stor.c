/* SPDX-License-Identifier: GPL-3.0-only */
/* Skiff (phrase-arp): the phrase pool's flash object (phr.c is the RAM half).
 *
 * The pool rides storage.c's A/B object (OBJ_PHRASES, FL_PHRASE: the pattern
 * bank's last two sectors of before): an erase, the payload, the commit header
 * last, so a write torn anywhere leaves the other copy in charge. phr_boot reads
 * the newest valid copy at power-on; phr_persist writes a changed pool at the
 * end of a recording pass (seq.c) or after a phrase is cleared.
 *
 * Without the flash driver (FELUCCA_FLASH 0) the pool is RAM only, as the host
 * tests want; the arp plays whatever the test puts in it.
 *
 * Included after storage.c, so it sees st_load / st_save / flash_ok. */

static void phr_boot(void)
{
#if FELUCCA_FLASH
    if (flash_ok && st_load(OBJ_PHRASES, phr_pool, sizeof phr_pool) < 0)
        memset(phr_pool, 0, sizeof phr_pool);
#else
    memset(phr_pool, 0, sizeof phr_pool);
#endif
    {   /* the lens from the pool */
        uint32_t i, k;
        for (i = 0; i < PHR_N; i++) {
            uint32_t l = 0;
            for (k = 0; k < PHR_STEPS; k++)
                if (phr_pool[i * PHR_STEPS + k])
                    l = k + 1u;
            phr_len[i] = (uint8_t)l;
        }
    }
    phr_dirty = 0;
}

static void phr_persist(void)
{
#if FELUCCA_FLASH
    if (phr_dirty && flash_ok && st_save(OBJ_PHRASES, phr_pool, sizeof phr_pool) == 0)
        phr_dirty = 0;
#else
    (void)phr_dirty;
#endif
}
