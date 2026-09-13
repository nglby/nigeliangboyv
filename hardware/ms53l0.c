
#include "ms53l0.h"

/* ===== private variables ===== */
static uint8_t stop_var;   /* StopVariable, read during DataInit */
uint8_t ms53l0_ready = 0;  /* 1 = init succeeded, safe to read */

/* ===== I2C helpers (use hi2c2) ===== */
static void wr(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    HAL_I2C_Master_Transmit(&hi2c2, MS53L0_ADDR, buf, 2, 100);
}

static uint8_t rd(uint8_t reg)
{
    uint8_t val = 0;
    HAL_I2C_Master_Transmit(&hi2c2, MS53L0_ADDR, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c2, MS53L0_ADDR, &val, 1, 100);
    return val;
}

static void rd_multi(uint8_t reg, uint8_t *buf, uint16_t len)
{
    HAL_I2C_Master_Transmit(&hi2c2, MS53L0_ADDR, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c2, MS53L0_ADDR, buf, len, 100);
}

static uint16_t rd_word(uint8_t reg)
{
    uint8_t buf[2];
    rd_multi(reg, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static uint32_t rd_dword(uint8_t reg)
{
    uint8_t buf[4];
    rd_multi(reg, buf, 4);
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

/* ===== Default tuning settings (from ST vl53l0x_tuning.h) ===== */
static const uint8_t DefaultTuningSettings[] = {
    0x01, 0xFF, 0x01,
    0x01, 0x00, 0x00,
    0x01, 0xFF, 0x00,
    0x01, 0x09, 0x00,
    0x01, 0x10, 0x00,
    0x01, 0x11, 0x00,
    0x01, 0x24, 0x01,
    0x01, 0x25, 0xff,
    0x01, 0x75, 0x00,
    0x01, 0xFF, 0x01,
    0x01, 0x4e, 0x2c,
    0x01, 0x48, 0x00,
    0x01, 0x30, 0x20,
    0x01, 0xFF, 0x00,
    0x01, 0x30, 0x09,
    0x01, 0x54, 0x00,
    0x01, 0x31, 0x04,
    0x01, 0x32, 0x03,
    0x01, 0x40, 0x83,
    0x01, 0x46, 0x25,
    0x01, 0x60, 0x00,
    0x01, 0x27, 0x00,
    0x01, 0x50, 0x06,
    0x01, 0x51, 0x00,
    0x01, 0x52, 0x96,
    0x01, 0x56, 0x08,
    0x01, 0x57, 0x30,
    0x01, 0x61, 0x00,
    0x01, 0x62, 0x00,
    0x01, 0x64, 0x00,
    0x01, 0x65, 0x00,
    0x01, 0x66, 0xa0,
    0x01, 0xFF, 0x01,
    0x01, 0x22, 0x32,
    0x01, 0x47, 0x14,
    0x01, 0x49, 0xff,
    0x01, 0x4a, 0x00,
    0x01, 0xFF, 0x00,
    0x01, 0x7a, 0x0a,
    0x01, 0x7b, 0x00,
    0x01, 0x78, 0x21,
    0x01, 0xFF, 0x01,
    0x01, 0x23, 0x34,
    0x01, 0x42, 0x00,
    0x01, 0x44, 0xff,
    0x01, 0x45, 0x26,
    0x01, 0x46, 0x05,
    0x01, 0x40, 0x40,
    0x01, 0x0E, 0x06,
    0x01, 0x20, 0x1a,
    0x01, 0x43, 0x40,
    0x01, 0xFF, 0x00,
    0x01, 0x34, 0x03,
    0x01, 0x35, 0x44,
    0x01, 0xFF, 0x01,
    0x01, 0x31, 0x04,
    0x01, 0x4b, 0x09,
    0x01, 0x4c, 0x05,
    0x01, 0x4d, 0x04,
    0x01, 0xFF, 0x00,
    0x01, 0x44, 0x00,
    0x01, 0x45, 0x20,
    0x01, 0x47, 0x08,
    0x01, 0x48, 0x28,
    0x01, 0x67, 0x00,
    0x01, 0x70, 0x04,
    0x01, 0x71, 0x01,
    0x01, 0x72, 0xfe,
    0x01, 0x76, 0x00,
    0x01, 0x77, 0x00,
    0x01, 0xFF, 0x01,
    0x01, 0x0d, 0x01,
    0x01, 0xFF, 0x00,
    0x01, 0x80, 0x01,
    0x01, 0x01, 0xF8,
    0x01, 0xFF, 0x01,
    0x01, 0x8e, 0x01,
    0x01, 0x00, 0x01,
    0x01, 0xFF, 0x00,
    0x01, 0x80, 0x00,
    0x00, 0x00, 0x00   /* end marker */
};

/* ===== SPAD helpers ===== */
static const uint32_t ref_quadrants[4] = {10, 5, 0, 5};

static uint8_t is_aperture(uint32_t idx)
{
    uint32_t q = idx >> 6;
    return (q < 4 && ref_quadrants[q] == 0) ? 0 : 1;
}

static void get_next_good_spad(uint8_t *map, uint32_t size,
                                uint32_t curr, int32_t *next)
{
    uint32_t c, f;
    uint8_t data;
    *next = -1;
    for (c = curr / 8; c < size; c++) {
        data = map[c];
        f = (c == curr / 8) ? (curr % 8) : 0;
        data >>= f;
        for (; f < 8; f++) {
            if (data & 1) { *next = c * 8 + f; return; }
            data >>= 1;
        }
    }
}

static void enable_spad_bit(uint8_t *arr, uint32_t idx)
{
    arr[idx / 8] |= (1 << (idx % 8));
}

/* ===== NVM read strobe ===== */
static void read_strobe(void)
{
    wr(0x83, 0x00);
    for (uint32_t i = 0; i < 2000; i++) {
        if (rd(0x83) != 0x00) return;
    }
}

/* ===== Read NVM SPAD info (count, type, good SPAD map) ===== */
static void read_nvm_spad_info(uint8_t *count, uint8_t *type, uint8_t *good_map)
{
    uint32_t tmp;

    wr(0x80, 0x01);
    wr(0xFF, 0x01);
    wr(0x00, 0x00);
    wr(0xFF, 0x06);
    uint8_t b = rd(0x83);
    wr(0x83, b | 0x04);
    wr(0xFF, 0x07);
    wr(0x81, 0x01);
    HAL_Delay(1);
    wr(0x80, 0x01);

    /* SPAD count + type */
    wr(0x94, 0x6b);
    read_strobe();
    tmp = rd_dword(0x90);
    *count = (uint8_t)((tmp >> 8) & 0x7F);
    *type  = (uint8_t)((tmp >> 15) & 0x01);

    /* Good SPAD map [0..3] */
    wr(0x94, 0x24);
    read_strobe();
    tmp = rd_dword(0x90);
    good_map[0] = (uint8_t)(tmp >> 24);
    good_map[1] = (uint8_t)(tmp >> 16);
    good_map[2] = (uint8_t)(tmp >> 8);
    good_map[3] = (uint8_t)(tmp);

    /* Good SPAD map [4..5] */
    wr(0x94, 0x25);
    read_strobe();
    tmp = rd_dword(0x90);
    good_map[4] = (uint8_t)(tmp >> 24);
    good_map[5] = (uint8_t)(tmp >> 16);

    /* Cleanup */
    wr(0x81, 0x00);
    wr(0xFF, 0x06);
    b = rd(0x83);
    wr(0x83, b & 0xFB);
    wr(0xFF, 0x01);
    wr(0x00, 0x01);
    wr(0xFF, 0x00);
    wr(0x80, 0x00);
}

/* ===== Set reference SPADs from NVM good SPAD map ===== */
static void set_ref_spads(uint8_t count, uint8_t is_apt, uint8_t *good_map)
{
    uint8_t spad_en[6] = {0};
    uint8_t start_select = 0xB4;
    uint32_t max_spads = 44;
    uint32_t curr = 0;
    int32_t next;

    wr(0xFF, 0x01);
    wr(0x4F, 0x00);
    wr(0x4E, 0x2C);
    wr(0xFF, 0x00);
    wr(0xB6, start_select);

    /* If aperture, skip to first aperture SPAD */
    if (is_apt) {
        while (!is_aperture(start_select + curr) && curr < max_spads)
            curr++;
    }

    /* Enable 'count' good SPADs of the correct type */
    for (uint32_t i = 0; i < count; i++) {
        get_next_good_spad(good_map, 6, curr, &next);
        if (next < 0) break;
        if (is_aperture(start_select + (uint32_t)next) != is_apt) break;
        curr = (uint32_t)next;
        enable_spad_bit(spad_en, curr);
        curr++;
    }

    /* Write SPAD enable map to 0xB0..0xB5 */
    for (int i = 0; i < 6; i++)
        wr(0xB0 + i, spad_en[i]);
}

/* ===== Single reference calibration (VHV or phase) ===== */
static void perform_single_ref_cal(uint8_t vhv_init_byte)
{
    wr(0x00, 0x01 | vhv_init_byte);   /* SYSRANGE_START */

    /* Poll RESULT_RANGE_STATUS bit0 until data ready */
    for (uint32_t i = 0; i < 2000; i++) {
        if (rd(0x14) & 0x01) break;
    }

    /* Clear interrupt */
    wr(0x0B, 0x01);
    wr(0x0B, 0x00);
    wr(0x00, 0x00);   /* stop */
}

/* ===== Reference calibration: VHV + Phase ===== */
static void perform_ref_cal(uint8_t *vhv, uint8_t *phase)
{
    uint8_t seq_save = rd(0x01);   /* save SEQUENCE_CONFIG (0xF8 from tuning) */

    /* --- VHV calibration --- */
    wr(0x01, 0x01);                /* SEQUENCE_CONFIG = VHV only */
    perform_single_ref_cal(0x40);

    /* Read VHV result from 0xCB */
    wr(0xFF, 0x01); wr(0x00, 0x00); wr(0xFF, 0x00);
    *vhv = rd(0xCB);
    wr(0xFF, 0x01); wr(0x00, 0x01); wr(0xFF, 0x00);

    /* --- Phase calibration --- */
    wr(0x01, 0x02);                /* SEQUENCE_CONFIG = phase only */
    perform_single_ref_cal(0x00);

    /* Read phase result from 0xEE */
    wr(0xFF, 0x01); wr(0x00, 0x00); wr(0xFF, 0x00);
    *phase = rd(0xEE) & 0xEF;
    wr(0xFF, 0x01); wr(0x00, 0x01); wr(0xFF, 0x00);

    /* Restore SEQUENCE_CONFIG */
    wr(0x01, seq_save);
}

/* ===== Public API: Init ===== */
uint8_t MS53L0_Init(void)
{
    HAL_Delay(10);

    /* 1. Verify Model ID (0xC0 should be 0xEEAA) */
    uint16_t id = rd_word(0xC0);
    if (id != 0xEEAA) return 1;

    /* 2. DataInit: I2C standard mode + read StopVariable */
    wr(0x88, 0x00);

    wr(0x80, 0x01);
    wr(0xFF, 0x01);
    wr(0x00, 0x00);
    stop_var = rd(0x91);
    wr(0x00, 0x01);
    wr(0xFF, 0x00);
    wr(0x80, 0x00);

    wr(0x01, 0xFF);   /* SEQUENCE_CONFIG = 0xFF (all enabled) */

    /* 3. Apply DefaultTuningSettings */
    for (uint16_t i = 0; DefaultTuningSettings[i] != 0x00; i += 3) {
        if (DefaultTuningSettings[i] == 0x01)
            wr(DefaultTuningSettings[i + 1], DefaultTuningSettings[i + 2]);
    }

    /* 4. Read NVM SPAD info */
    uint8_t spad_count = 0, spad_type = 0;
    uint8_t good_map[6] = {0};
    read_nvm_spad_info(&spad_count, &spad_type, good_map);

    /* 5. Set reference SPADs */
    if (spad_type <= 1 &&
        ((spad_type == 1 && spad_count <= 32) ||
         (spad_type == 0 && spad_count <= 12))) {
        /* NVM values valid */
        set_ref_spads(spad_count, spad_type, good_map);
    } else {
        /* NVM invalid, use 5 non-aperture SPADs as fallback */
        set_ref_spads(5, 0, good_map);
    }

    /* 6. Reference calibration (VHV + phase) */
    uint8_t vhv = 0, phase = 0;
    perform_ref_cal(&vhv, &phase);

    /* 7. Single-shot mode */
    wr(0x00, 0x00);

    HAL_Delay(10);
    ms53l0_ready = 1;
    return 0;
}

/* ===== Public API: Read distance (single-shot) ===== */
uint16_t MS53L0_ReadMM(void)
{
    if (!ms53l0_ready) return 0xFFFF;

    /* StartMeasurement sequence (restore StopVariable) */
    wr(0x80, 0x01);
    wr(0xFF, 0x01);
    wr(0x00, 0x00);
    wr(0x91, stop_var);
    wr(0x00, 0x01);
    wr(0xFF, 0x00);
    wr(0x80, 0x00);

    /* Trigger single measurement */
    wr(0x00, 0x01);

    /* Wait for start bit to clear */
    for (uint32_t i = 0; i < 2000; i++) {
        if (!(rd(0x00) & 0x01)) break;
    }

    /* Poll for data ready (RESULT_RANGE_STATUS bit0) */
    for (uint32_t i = 0; i < 2000; i++) {
        if (rd(0x14) & 0x01) {
            /* Read 12 bytes from 0x14 */
            uint8_t buf[12];
            rd_multi(0x14, buf, 12);
            /* Distance in buf[10]:buf[11] (big endian) */
            uint16_t dist = ((uint16_t)buf[10] << 8) | buf[11];

            /* Clear interrupt */
            wr(0x0B, 0x01);
            wr(0x0B, 0x00);
            return dist;
        }
    }

    /* Timeout */
    wr(0x0B, 0x01);
    wr(0x0B, 0x00);
    return 0xFFFF;
}
