#include <stdio.h>
#include <string.h>
#include "tlog_stream.c"

#define BUF_SIZE    32

static bool global_failed = false;

struct test {
    const uint8_t   ibuf_in[BUF_SIZE];
    size_t          ilen_in;
    const uint8_t   obuf_out[BUF_SIZE];
    size_t          orem_in;
    size_t          orem_out;
    size_t          olen_in;
    size_t          olen_out;
    size_t          irun_in;
    size_t          irun_out;
    size_t          idig_in;
    size_t          idig_out;
    bool            fit_out;
};

#define BOOL_STR(_b) ((_b) ? "true" : "false")

static void
diff_side(FILE *stream, const char *name,
          const uint8_t *out, const uint8_t *exp, size_t len)
{
    size_t col;
    size_t i;
    const uint8_t *o;
    const uint8_t *e;
    uint8_t c;

    fprintf(stream, "%s str:\n", name);
    for (o = out, i = len; i > 0; o++, i--) {
        c = *o;
        fputc(((c >= 0x20 && c < 0x7f) ? c : ' '), stream);
    }
    fputc('\n', stream);
    for (o = out, e = exp, i = len; i > 0; o++, e++, i--)
        fprintf(stream, "%c", ((*o == *e) ? ' ' : '^'));

    fprintf(stream, "\n%s hex:\n", name);
    for (o = out, e = exp, i = len, col = 0; i > 0; o++, e++, i--) {
        fprintf(stream, " %c%02x", ((*o == *e) ? ' ' : '!'), *o);
        col++;
        if (col > 0xf) {
            col = 0;
            fprintf(stream, "\n");
        }
    }
    if (col != 0)
        fprintf(stream, "\n");
}

static void
diff(FILE *stream, const uint8_t *res, const uint8_t *exp, size_t len)
{
    diff_side(stream, "expected", exp, res, len);
    fprintf(stream, "\n");
    diff_side(stream, "result", res, exp, len);
}

static void
test(const char *n, const struct test t)
{
    bool failed = false;
    uint8_t obuf[BUF_SIZE] = {0,};
    size_t orem = t.orem_in;
    size_t olen = t.olen_in;
    size_t irun = t.irun_in;
    size_t idig = t.idig_in;
    bool fit;

#define FAIL(_fmt, _args...) \
    do {                                                \
        fprintf(stderr, "%s: " _fmt "\n", n, ##_args);  \
        failed = true;                                  \
    } while (0)

#define CMP_SIZE(_name) \
    do {                                                        \
        if (_name != t._name##_out)                            \
            FAIL(#_name " %zu != %zu", _name, t._name##_out);  \
    } while (0)

#define CMP_BOOL(_name) \
    do {                                                        \
        if (_name != t._name##_out)                            \
            FAIL(#_name " %s != %s",                            \
                 BOOL_STR(_name), BOOL_STR(t._name##_out));    \
    } while (0)

    fit = tlog_stream_enc_bin(obuf, &orem, &olen, &irun, &idig,
                              t.ibuf_in, t.ilen_in);
    CMP_BOOL(fit);
    CMP_SIZE(orem);
    CMP_SIZE(olen);
    CMP_SIZE(irun);
    CMP_SIZE(idig);

    if (memcmp(obuf, t.obuf_out, BUF_SIZE) != 0) {
        fprintf(stderr, "%s: obuf mismatch:\n", n);
        diff(stderr, obuf, t.obuf_out, BUF_SIZE);
        failed = true;
    }

#undef CMP_SIZE
#undef CMP_BOOL
#undef FAIL
    fprintf(stderr, "%s: %s\n", n, (failed ? "FAIL" : "PASS"));
    global_failed = global_failed || failed;
}

#define TEST(_name_token, _struct_init_args...) \
    test(#_name_token, (struct test){_struct_init_args})

int
main(void)
{
    /* No input producing no output */
    TEST(zero,          .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Output for one byte input */
    TEST(one,           .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 3,
                        .olen_out   = 3,
                        .irun_out   = 1,
                        .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* One byte input, output short of one byte */
    TEST(one_out_one,   .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "25",
                        .orem_in    = 2,
                        .orem_out   = 2,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /* One byte input, output short of two bytes */
    TEST(one_out_two,   .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "2",
                        .orem_in    = 1,
                        .orem_out   = 1,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /* One byte input, output short of three (all) bytes */
    TEST(one_out_three, .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /* Output for two bytes input */
    TEST(two,           .ibuf_in    = {0x01, 0x02},
                        .ilen_in    = 2,
                        .obuf_out   = "1,2",
                        .orem_in    = 3,
                        .olen_out   = 3,
                        .irun_out   = 2,
                        .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Two byte input, output short of one byte */
    TEST(two_out_one,   .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254,25",
                        .orem_in    = 6,
                        .orem_out   = 6,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /* Two byte input, output short of two bytes */
    TEST(two_out_two,   .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254,2",
                        .orem_in    = 5,
                        .orem_out   = 5,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /*
     * Two byte input, output short of three bytes
     * (no space for second input byte in the output at all)
     */
    TEST(two_out_three, .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254,",
                        .orem_in    = 4,
                        .orem_out   = 4,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /*
     * Two byte input, output short of four bytes (no space for comma and
     * second input byte in the output)
     */
    TEST(two_out_four,  .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254",
                        .orem_in    = 3,
                        .orem_out   = 3,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /*
     * Two byte input, output short of five bytes
     * (no space even for the first complete byte)
     */
    TEST(two_out_five,  .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "25",
                        .orem_in    = 2,
                        .orem_out   = 2,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /* Output for three bytes input */
    TEST(three,         .ibuf_in    = {0x01, 0x02, 0x03},
                        .ilen_in    = 3,
                        .obuf_out   = "1,2,3",
                        .orem_in    = 5,
                        .olen_out   = 5,
                        .irun_out   = 3,
                        .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Non-zero input olen */
    TEST(non_zero_olen, .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = ",255",
                        .orem_in    = 4,
                        .olen_in    = 100,
                        .olen_out   = 104,
                        .irun_out   = 1,
                        .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Non-zero input irun */
    TEST(non_zero_irun, .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 3,
                        .olen_out   = 3,
                        .irun_in    = 1,
                        .irun_out   = 2,
                        .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Rolling over to second digit */
    TEST(second_digit,  .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 4,
                        .olen_out   = 3,
                        .irun_in    = 9,
                        .irun_out   = 10,
                        .idig_in    = 10,
                        .idig_out   = 100,
                        .fit_out    = true);

    /* Rolling over to third digit */
    TEST(third_digit,   .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 4,
                        .olen_out   = 3,
                        .irun_in    = 99,
                        .irun_out   = 100,
                        .idig_in    = 100,
                        .idig_out   = 1000,
                        .fit_out    = true);

    return global_failed;
}
