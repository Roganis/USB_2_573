/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The firmware's core (firmware/core) on the PC: the switches' debounce and latch, the timed
 * output FIFO, the failsafe, the coin counters' limit, both interfaces' reports. */
#include <stdio.h>
#include <string.h>

#include "u573_core.h"

static int failures = 0;
#define CHECK(c)                                                            \
    do {                                                                    \
        if (!(c)) {                                                         \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c); \
            failures++;                                                     \
        }                                                                   \
    } while (0)

static uint32_t in_switches(const uint8_t *r) {
    return (uint32_t)r[4] | (uint32_t)r[5] << 8 | (uint32_t)r[6] << 16 | (uint32_t)r[7] << 24;
}
static uint16_t in_applied(const uint8_t *r) { return (uint16_t)(r[12] | r[13] << 8); }

/* An output report of `n` entries: delay[k] (10 us units), state[k] */
static size_t outputs_report(uint8_t *r, int n, const uint16_t *delay, const uint64_t *state, uint8_t flags) {
    memset(r, 0, U573_REPORT_SIZE);
    r[0] = U573_OUT_MAGIC, r[1] = U573_CMD_OUTPUTS, r[2] = (uint8_t)n, r[3] = flags;
    for (int k = 0; k < n; k++) {
        uint8_t *e = r + 8 + k * 8;
        e[0] = (uint8_t)delay[k], e[1] = (uint8_t)(delay[k] >> 8);
        for (int b = 0; b < 6; b++) e[2 + b] = (uint8_t)(state[k] >> (8 * b));
    }
    return U573_REPORT_SIZE;
}

static void keepalive(u573_t *s, uint64_t now) {
    uint8_t r[U573_REPORT_SIZE] = {U573_OUT_MAGIC, U573_CMD_KEEPALIVE};
    u573_raw_out(s, now, r, sizeof r);
}

static void test_switches(void) {
    u573_t s;
    uint8_t r[U573_REPORT_SIZE];
    u573_init(&s, 1000);

    /* a switch held at power-on is there at the first sample */
    u573_sample(&s, 1000, U573_SW_TEST);
    CHECK(s.switches == U573_SW_TEST);

    /* a press is taken at once; its bounce within the debounce time is not seen */
    u573_sample(&s, 10000, U573_SW_TEST | U573_SW_P1_LEFT);
    CHECK(s.switches == (U573_SW_TEST | U573_SW_P1_LEFT));
    u573_sample(&s, 10250, U573_SW_TEST);
    u573_sample(&s, 10500, U573_SW_TEST | U573_SW_P1_LEFT);
    u573_sample(&s, 11750, U573_SW_TEST);
    CHECK(s.switches == (U573_SW_TEST | U573_SW_P1_LEFT));
    /* once that time has passed, the release counts */
    u573_sample(&s, 12000, U573_SW_TEST);
    CHECK(s.switches == U573_SW_TEST);
    /* ... and each switch keeps its own time: another one pressed meanwhile isn't held up */
    u573_sample(&s, 12100, U573_SW_TEST | U573_SW_P2_B3);
    CHECK(s.switches == (U573_SW_TEST | U573_SW_P2_B3));

    /* the report: the switches as they are, with what closed since the last report */
    u573_raw_in(&s, 12200, r);
    CHECK(r[0] == U573_IN_MAGIC && r[1] == U573_PROTO_VERSION);
    CHECK(in_switches(r) == (U573_SW_TEST | U573_SW_P1_LEFT | U573_SW_P2_B3));   /* P1 left: let go since, still told once */
    u573_raw_in(&s, 13200, r);
    CHECK(in_switches(r) == (U573_SW_TEST | U573_SW_P2_B3));
    CHECK(r[3] == 1);   /* the report counter */

    /* a tap between two reports (a drum pad's pulse) is in the next report, and in the gamepad's */
    uint8_t pad[U573_PAD_IN_SIZE];
    u573_pad_in(&s, pad);
    u573_sample(&s, 20000, U573_SW_TEST | U573_SW_P2_B3 | U573_SW_P1_UP);
    u573_sample(&s, 22500, U573_SW_TEST | U573_SW_P2_B3);
    CHECK(!(s.switches & U573_SW_P1_UP));
    u573_raw_in(&s, 23000, r);
    CHECK(in_switches(r) & U573_SW_P1_UP);
    u573_raw_in(&s, 24000, r);
    CHECK(!(in_switches(r) & U573_SW_P1_UP));
    u573_pad_in(&s, pad);
    CHECK(pad[1] & 0x04);   /* button 11: P1 up */
    u573_pad_in(&s, pad);
    CHECK(!(pad[1] & 0x04));
    CHECK((pad[3] & 0x02) && !(pad[3] & 0xfc));   /* Test is button 26; the option switches aren't buttons */

    /* bits that are no switch are never set; no debounce at all when it is set to 0 */
    uint8_t cfg[U573_REPORT_SIZE] = {U573_OUT_MAGIC, U573_CMD_CONFIG, 0, 0, 0, 0, 0, 0, 0, 0, 0xe8, 0x03, 0xf4, 0x01};
    u573_raw_out(&s, 30000, cfg, sizeof cfg);
    CHECK(s.debounce_us == 0 && s.failsafe_ms == 1000 && s.counter_max_ms == 500);
    u573_sample(&s, 30010, 0xffffffffu);
    CHECK(s.switches == U573_SW_ALL);
    u573_sample(&s, 30020, 0);
    CHECK(s.switches == 0);

    /* the analog channels and the board's description */
    const uint8_t ch[4] = {0, 128, 255, 7};
    u573_set_analog(&s, ch);
    u573_raw_in(&s, 31000, r);
    CHECK(!memcmp(r + 8, ch, 4));
    u573_pad_in(&s, pad);
    CHECK(!memcmp(pad + 4, ch, 4));
    u573_info(&s, r);
    CHECK(!memcmp(r, "U573", 4) && r[4] == U573_PROTO_VERSION && r[9] == U573_FIFO_SIZE);
    CHECK(r[12] == 0 && r[13] == 0 && r[14] == 0xe8 && r[15] == 0x03);
}

static void test_outputs(void) {
    u573_t s;
    uint8_t r[U573_REPORT_SIZE], in[U573_REPORT_SIZE];
    uint64_t drive = 0;
    u573_init(&s, 0);

    /* nothing is driven before a host says so */
    CHECK(!u573_task(&s, 100, &drive) && drive == 0);
    CHECK(!u573_host_active(&s, 100));
    CHECK(u573_next_due(&s, 100) == UINT64_MAX);

    /* entries with no delay go out one at a time, in order: every state reaches the pins */
    const uint64_t st[4] = {U573_OUT_A(0), U573_OUT_A(0) | U573_OUT_A(4), U573_OUT_A(0), U573_OUT_A(0) | U573_OUT_D(3) | U573_OUT_EXT(7)};
    const uint16_t d0[4] = {0, 0, 0, 0};
    u573_raw_out(&s, 1000, r, outputs_report(r, 4, d0, st, 0));
    CHECK(u573_host_active(&s, 1000));
    for (int k = 0; k < 4; k++) {
        CHECK(u573_task(&s, 1000, &drive));
        CHECK(drive == st[k]);
    }
    CHECK(!u573_task(&s, 1000, &drive));
    u573_raw_in(&s, 1100, in);
    CHECK(in_applied(in) == 4 && in[14] == 0 && in[15] == U573_FIFO_SIZE);
    CHECK((in[2] & U573_STATUS_HOST_ACTIVE) && !(in[2] & U573_STATUS_OVERFLOW));
    CHECK(in[20] == 0x01 && in[23] == 0x08 && in[24] == 0x80);   /* the outputs as driven: A0, D3, OUT7 */

    /* a stage's clock and data: each edge its delay after the one before, however they arrive */
    const uint64_t e[3] = {U573_OUT_A(4), U573_OUT_A(4) | U573_OUT_A(5), U573_OUT_A(5)};
    const uint16_t d[3] = {50, 50, 100};   /* 500 us, 500 us, 1 ms */
    u573_raw_out(&s, 2000, r, outputs_report(r, 3, d, e, 0));
    CHECK(u573_task(&s, 2000, &drive) && drive == e[0]);   /* long after the last entry: at once */
    CHECK(!u573_task(&s, 2400, &drive));
    CHECK(u573_next_due(&s, 2400) == 2500);
    CHECK(u573_task(&s, 2500, &drive) && drive == e[1]);
    CHECK(!u573_task(&s, 3400, &drive));
    CHECK(u573_task(&s, 3600, &drive) && drive == e[2]);   /* (the loop came late: it goes out then) */
    /* the next report's first entry counts from that one */
    u573_raw_out(&s, 3700, r, outputs_report(r, 1, d, st, 0));
    CHECK(!u573_task(&s, 4000, &drive));
    CHECK(u573_task(&s, 4100, &drive) && drive == st[0]);

    /* a flush drops what waits: only the new entries go out */
    const uint16_t slow[4] = {5000, 5000, 5000, 5000};
    u573_raw_out(&s, 5000, r, outputs_report(r, 2, slow, e, 0));
    u573_raw_out(&s, 5100, r, outputs_report(r, 1, d0, &st[3], U573_OUTFLAG_FLUSH));
    CHECK(u573_task(&s, 5100, &drive) && drive == st[3]);
    CHECK(!u573_task(&s, 200000, &drive) && s.count == 0);

    /* more than the FIFO holds: the rest is dropped and the status says so, until a reset */
    for (int k = 0; k < U573_FIFO_SIZE / 4 + 1; k++) u573_raw_out(&s, 200000, r, outputs_report(r, 4, slow, st, 0));
    CHECK(s.count == U573_FIFO_SIZE);
    u573_raw_in(&s, 200000, in);
    CHECK((in[2] & U573_STATUS_OVERFLOW) && in[14] == U573_FIFO_SIZE);
    const uint8_t reset[U573_REPORT_SIZE] = {U573_OUT_MAGIC, U573_CMD_RESET};
    u573_raw_out(&s, 200100, reset, sizeof reset);
    CHECK(u573_task(&s, 200100, &drive) && drive == 0);
    u573_raw_in(&s, 200100, in);
    CHECK(!(in[2] & U573_STATUS_OVERFLOW) && in_applied(in) == 0 && in[14] == 0);

    /* what isn't the protocol's is ignored, and is no sign of a host */
    u573_t t;
    uint64_t td = 0;
    u573_init(&t, 0);
    uint8_t bad[U573_REPORT_SIZE] = {0x00, U573_CMD_OUTPUTS, 1};
    u573_raw_out(&t, 10, bad, sizeof bad);
    bad[0] = U573_OUT_MAGIC, bad[1] = 0x7f;
    u573_raw_out(&t, 10, bad, sizeof bad);
    bad[1] = U573_CMD_OUTPUTS, bad[2] = 8;   /* more entries than a report holds */
    u573_raw_out(&t, 10, bad, sizeof bad);
    u573_raw_out(&t, 10, r, 4);              /* cut short */
    CHECK(!u573_host_active(&t, 10) && t.count == 0 && !u573_task(&t, 10, &td));
    /* bits that are no output are never driven */
    const uint64_t all = UINT64_MAX;
    u573_raw_out(&t, 20, r, outputs_report(r, 1, d0, &all, 0));
    CHECK(u573_task(&t, 20, &td) && td == U573_OUT_ALL);
}

static void test_failsafe(void) {
    u573_t s;
    uint8_t r[U573_REPORT_SIZE];
    uint64_t drive = 0;
    const uint16_t d0[2] = {0, 20000};
    u573_init(&s, 0);

    /* lamps and a coin counter on, another entry waiting; the host keeps saying it is there */
    const uint64_t st[2] = {U573_OUT_C(2) | U573_OUT_EXT(0) | U573_OUT_COUNTER1, U573_OUT_C(3)};
    u573_raw_out(&s, 0, r, outputs_report(r, 2, d0, st, 0));
    CHECK(u573_task(&s, 0, &drive) && drive == st[0]);
    for (uint64_t t = 100000; t <= 900000; t += 100000) {
        keepalive(&s, t);
        if (t == 200000) CHECK(u573_task(&s, t, &drive) && drive == st[1]);
        else CHECK(!u573_task(&s, t, &drive));
    }
    /* then stops: a second later every output is off, and what waited is gone */
    u573_raw_out(&s, 900000, r, outputs_report(r, 2, d0, st, 0));
    CHECK(u573_task(&s, 900000, &drive) && drive == st[0]);
    CHECK(u573_next_due(&s, 900000) == 1100000);
    CHECK(u573_task(&s, 1100000, &drive) && drive == st[1]);
    CHECK(u573_next_due(&s, 1100000) == 1900000);
    CHECK(!u573_task(&s, 1899999, &drive));
    CHECK(u573_task(&s, 1900000, &drive) && drive == 0);
    CHECK(!u573_host_active(&s, 1900000) && s.count == 0);

    /* a coin counter held too long is let go by the board, lamps staying as they are ... */
    const uint64_t coin = U573_OUT_COUNTER2 | U573_OUT_B(1);
    u573_raw_out(&s, 2000000, r, outputs_report(r, 1, d0, &coin, 0));
    CHECK(u573_task(&s, 2000000, &drive) && drive == coin);
    for (uint64_t t = 2100000; t < 3000000; t += 100000) {
        keepalive(&s, t);
        CHECK(!u573_task(&s, t, &drive));
    }
    keepalive(&s, 3000000);
    CHECK(u573_task(&s, 3000000, &drive) && drive == U573_OUT_B(1));
    u573_raw_in(&s, 3000000, r);
    CHECK(r[2] & U573_STATUS_COUNTER_CUT);
    /* ... and stays off until the host has let go of it itself */
    uint8_t out[U573_REPORT_SIZE];
    u573_raw_out(&s, 3100000, out, outputs_report(out, 1, d0, &coin, 0));
    CHECK(!u573_task(&s, 3100000, &drive) && drive == U573_OUT_B(1));
    const uint64_t off = U573_OUT_B(1);
    u573_raw_out(&s, 3200000, out, outputs_report(out, 1, d0, &off, 0));
    u573_task(&s, 3200000, &drive);
    u573_raw_out(&s, 3300000, out, outputs_report(out, 1, d0, &coin, 0));
    CHECK(u573_task(&s, 3300000, &drive) && drive == coin);
    /* a coin's own pulse (a tenth of a second) is never cut */
    u573_raw_out(&s, 3400000, out, outputs_report(out, 1, d0, &off, 0));
    CHECK(u573_task(&s, 3400000, &drive) && drive == off);

    /* the bus gone: everything off at once */
    u573_bus_idle(&s);
    CHECK(u573_task(&s, 3400001, &drive) && drive == 0);

    /* the way to the bootloader needs its word, and turns everything off first */
    u573_raw_out(&s, 3500000, out, outputs_report(out, 1, d0, &coin, 0));
    u573_task(&s, 3500000, &drive);
    uint8_t boot[U573_REPORT_SIZE] = {U573_OUT_MAGIC, U573_CMD_BOOTLOADER, 0, 0, 0, 0, 0, 0, 'B', 'O', 'O', 'M'};
    u573_raw_out(&s, 3500100, boot, sizeof boot);
    CHECK(!s.bootloader);
    boot[11] = 'T';
    u573_raw_out(&s, 3500200, boot, sizeof boot);
    CHECK(s.bootloader);
    CHECK(u573_task(&s, 3500200, &drive) && drive == 0);
}

static void test_gamepad_lamps(void) {
    u573_t s;
    uint64_t drive = 0;
    u573_init(&s, 0);

    /* lamp 0 = A0, 27 = D3, 28 = OUT0, 35 = OUT7, 36 and 37 the coin counters */
    const uint8_t lamps[U573_PAD_OUT_SIZE] = {0x01, 0x00, 0x00, 0x18, 0x28};
    u573_pad_out(&s, 1000, lamps, sizeof lamps);
    CHECK(u573_task(&s, 1000, &drive));
    CHECK(drive == (U573_OUT_A(0) | U573_OUT_D(3) | U573_OUT_EXT(0) | U573_OUT_EXT(7) | U573_OUT_COUNTER2));
    /* they stay as they are for as long as nothing else is written (no failsafe here) ... */
    CHECK(u573_task(&s, 1001000, &drive));   /* ... but for the coin counter, let go after its second */
    CHECK(drive == (U573_OUT_A(0) | U573_OUT_D(3) | U573_OUT_EXT(0) | U573_OUT_EXT(7)));
    CHECK(!u573_task(&s, 60000000, &drive));

    /* a host on the raw interface takes the outputs over: the gamepad's are ignored meanwhile */
    uint8_t r[U573_REPORT_SIZE];
    const uint64_t st = U573_OUT_B(7);
    const uint16_t d0 = 0;
    u573_raw_out(&s, 60000000, r, outputs_report(r, 1, &d0, &st, 0));
    CHECK(u573_task(&s, 60000000, &drive) && drive == st);
    u573_pad_out(&s, 60100000, lamps, sizeof lamps);
    CHECK(!u573_task(&s, 60100000, &drive) && drive == st);
    /* once it is gone (the failsafe), the gamepad's count again */
    CHECK(u573_task(&s, 61000000, &drive) && drive == 0);
    const uint8_t one[U573_PAD_OUT_SIZE] = {0x02};
    u573_pad_out(&s, 61000100, one, sizeof one);
    CHECK(u573_task(&s, 61000100, &drive) && drive == U573_OUT_A(1));
    u573_pad_out(&s, 61000200, one, 3);   /* cut short: ignored */
    CHECK(!u573_task(&s, 61000200, &drive));
}

int main(void) {
    test_switches();
    test_outputs();
    test_failsafe();
    test_gamepad_lamps();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("u573 core: all checks passed\n");
    return 0;
}
