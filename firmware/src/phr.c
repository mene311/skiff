/* SPDX-License-Identifier: GPL-3.0-only */
/* Skiff (phrase-arp): the shared phrase pool, the RAM half.
 *
 * Eight phrases of 16 steps, one byte a step: the step's note as a signed 4-bit
 * semitone offset from the phrase root (bits 7..4, two's complement, -8..+7) and
 * its gate (bits 3..0) as the fraction of the step the note sounds (0..15, 15 =
 * the whole step). A step of 0 is empty (no note, no gate).
 *
 * The pool is every project's, like the pattern bank: any track's ARP MODE picks
 * OFF or a pool phrase, and any number of tracks can play the same phrase at
 * once, each transposed to its own held notes. It lives in RAM (the audio ISR
 * reads it); phr_stor.c is its flash object (storage.c OBJ_PHRASES at FL_PHRASE,
 * the pattern bank's last two sectors of before). A save happens when a phrase
 * changes: at the end of a recording pass (seq.c) or when a phrase is cleared.
 *
 * Included before seq.c, so the arp reads the pool directly. */

#define PHR_N 8u
#define PHR_STEPS 16u
#define PHR_LEN (PHR_N * PHR_STEPS)         /* the pool: 128 bytes */
#define PHR_STEP_OF(id, s) ((id & (PHR_N - 1u)) * PHR_STEPS + (s & (PHR_STEPS - 1u)))

static uint8_t phr_pool[PHR_LEN] __attribute__((section(".pool")));
static uint8_t phr_len[PHR_N];              /* each phrase's used length (its last nonzero step + 1), 0 = empty */
static uint8_t phr_dirty;                   /* a phrase changed since the last save */
static void phr_persist(void);               /* (phr_stor.c: the pool's flash object, after storage.c) */

static inline uint8_t phr_step(uint32_t id, uint32_t s) { return phr_pool[PHR_STEP_OF(id, s)]; }

/* the phrase's loop length: its last nonzero step + 1 (holes inside stay silence); an empty phrase plays nothing */
static inline uint32_t phr_len_get(uint32_t id)
{
    uint32_t l = phr_len[id & (PHR_N - 1u)];
    return l ? l : PHR_STEPS;
}

static inline void phr_set(uint32_t id, uint32_t s, uint8_t v)
{
    uint32_t i = id & (PHR_N - 1u);
    phr_pool[PHR_STEP_OF(i, s)] = v;
    phr_dirty = 1;
    if (v)
        phr_len[i] = (uint8_t)(phr_len[i] > (s & 15u) ? phr_len[i] : (s & 15u) + 1u);
    else if ((s & 15u) + 1u == phr_len[i]) {          /* the last step cleared: back to the next nonzero one */
        uint32_t k, l = 0;
        for (k = 0; k < PHR_STEPS; k++)
            if (phr_pool[i * PHR_STEPS + k])
                l = k + 1u;
        phr_len[i] = (uint8_t)l;
    }
}

static void phr_clear(uint32_t id)
{
    uint32_t i = id & (PHR_N - 1u);
    memset(&phr_pool[i * PHR_STEPS], 0, PHR_STEPS);
    phr_len[i] = 0;
    phr_dirty = 1;
}

static void phr_pool_clear(void)             /* all of them (a fresh pool: the tests', a new unit's) */
{
    memset(phr_pool, 0, sizeof phr_pool);
    memset(phr_len, 0, sizeof phr_len);
    phr_dirty = 1;
}
