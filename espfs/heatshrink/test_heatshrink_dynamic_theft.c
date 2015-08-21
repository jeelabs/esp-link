#include "heatshrink_config.h"
#ifdef HEATSHRINK_HAS_THEFT

#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>

#include "heatshrink_encoder.h"
#include "heatshrink_decoder.h"
#include "greatest.h"
#include "theft.h"
#include "greatest_theft.h"

#if !HEATSHRINK_DYNAMIC_ALLOC
#error Must set HEATSHRINK_DYNAMIC_ALLOC to 1 for this test suite.
#endif

SUITE(properties);

typedef struct {
    int limit;
    int fails;
    int dots;
} test_env;

typedef struct {
    size_t size;
    uint8_t buf[];
} rbuf;

static void *rbuf_alloc_cb(struct theft *t, theft_hash seed, void *env) {
    test_env *te = (test_env *)env;
    //printf("seed is 0x%016llx\n", seed);

    size_t sz = (size_t)(seed % te->limit) + 1;
    rbuf *r = malloc(sizeof(rbuf) + sz);
    if (r == NULL) { return THEFT_ERROR; }
    r->size = sz;

    for (size_t i = 0; i < sz; i += sizeof(theft_hash)) {
        theft_hash s = theft_random(t);
        for (uint8_t b = 0; b < sizeof(theft_hash); b++) {
            if (i + b >= sz) { break; }
            r->buf[i + b] = (uint8_t) (s >> (8*b)) & 0xff;
        }
    }

    return r;
}

static void rbuf_free_cb(void *instance, void *env) {
    free(instance);
    (void)env;
}

static uint64_t rbuf_hash_cb(void *instance, void *env) {
    rbuf *r = (rbuf *)instance;
    (void)env;
    return theft_hash_onepass(r->buf, r->size);
}

/* Make a copy of a buffer, keeping NEW_SZ bytes starting at OFFSET. */
static void *copy_rbuf_subset(rbuf *cur, size_t new_sz, size_t byte_offset) {
    if (new_sz == 0) { return THEFT_DEAD_END; }
    rbuf *nr = malloc(sizeof(rbuf) + new_sz);
    if (nr == NULL) { return THEFT_ERROR; }
    nr->size = new_sz;
    memcpy(nr->buf, &cur->buf[byte_offset], new_sz);
    /* printf("%zu -> %zu\n", cur->size, new_sz); */
    return nr;
}

/* Make a copy of a buffer, but only PORTION, starting OFFSET in
 * (e.g. the third quarter is (0.25 at +0.75). Rounds to ints. */
static void *copy_rbuf_percent(rbuf *cur, float portion, float offset) {
    size_t new_sz = cur->size * portion;
    size_t byte_offset = (size_t)(cur->size * offset);
    return copy_rbuf_subset(cur, new_sz, byte_offset);
}

/* How to shrink a random buffer to a simpler one. */
static void *rbuf_shrink_cb(void *instance, uint32_t tactic, void *env) {
    rbuf *cur = (rbuf *)instance;

    if (tactic == 0) {          /* first half */
        return copy_rbuf_percent(cur, 0.5, 0);
    } else if (tactic == 1) {   /* second half */
        return copy_rbuf_percent(cur, 0.5, 0.5);
    } else if (tactic <= 18) {  /* drop 1-16 bytes at start */
        const int last_tactic = 1;
        const size_t drop = tactic - last_tactic;
        if (cur->size < drop) { return THEFT_DEAD_END; }
        return copy_rbuf_subset(cur, cur->size - drop, drop);
    } else if (tactic <= 34) {  /* drop 1-16 bytes at end */
        const int last_tactic = 18;
        const size_t drop = tactic - last_tactic;
        if (cur->size < drop) { return THEFT_DEAD_END; }
        return copy_rbuf_subset(cur, cur->size - drop, 0);
    } else if (tactic == 35) {
        /* Divide every byte by 2, saturating at 0 */
        rbuf *cp = copy_rbuf_percent(cur, 1, 0);
        if (cp == NULL) { return THEFT_ERROR; }
        for (size_t i = 0; i < cp->size; i++) { cp->buf[i] /= 2; }
        return cp;
    } else if (tactic == 36) {
        /* subtract 1 from every byte, saturating at 0 */
        rbuf *cp = copy_rbuf_percent(cur, 1, 0);
        if (cp == NULL) { return THEFT_ERROR; }
        for (size_t i = 0; i < cp->size; i++) {
            if (cp->buf[i] > 0) { cp->buf[i]--; }
        }
        return cp;
    } else {
        (void)env;
        return THEFT_NO_MORE_TACTICS;
    }

    return THEFT_NO_MORE_TACTICS;
}

static void rbuf_print_cb(FILE *f, void *instance, void *env) {
    rbuf *r = (rbuf *)instance;
    (void)env;
    fprintf(f, "buf[%zd]:\n    ", r->size);
    uint8_t bytes = 0;
    for (size_t i = 0; i < r->size; i++) {
        fprintf(f, "%02x", r->buf[i]);
        bytes++;
        if (bytes == 16) {
            fprintf(f, "\n    ");
            bytes = 0;
        }
    }
    fprintf(f, "\n");
}

static struct theft_type_info rbuf_info = {
    .alloc = rbuf_alloc_cb,
    .free = rbuf_free_cb,
    .hash = rbuf_hash_cb,
    .shrink = rbuf_shrink_cb,
    .print = rbuf_print_cb,
};

static theft_progress_callback_res
progress_cb(struct theft_trial_info *info, void *env) {
    test_env *te = (test_env *)env;
    if ((info->trial & 0xff) == 0) {
        printf(".");
        fflush(stdout);
        te->dots++;
        if (te->dots == 64) {
            printf("\n");
            te->dots = 0;
        }
    }

    if (info->status == THEFT_TRIAL_FAIL) {
        te->fails++;
        rbuf *cur = info->args[0];
        if (cur->size < 5) { return THEFT_PROGRESS_HALT; }
    }

    if (te->fails > 10) {
        return THEFT_PROGRESS_HALT;
    }
    return THEFT_PROGRESS_CONTINUE;
}

/* For an arbitrary input buffer, it should never get stuck in a
 * state where the data has been sunk but no data can be polled. */
static theft_trial_res prop_should_not_get_stuck(void *input) {
    /* Make a buffer large enough for the output: 4 KB of input with
     * each 16 bits becoming up to 16 bytes will fit in a 64 KB buffer.
     * (4 KB of input comes from `env.limit = 1 << 12;` below.) */
    uint8_t output[64 * 1024];
    heatshrink_decoder *hsd = heatshrink_decoder_alloc((64 * 1024L) - 1, 12, 4);
    if (hsd == NULL) { return THEFT_TRIAL_ERROR; }

    rbuf *r = (rbuf *)input;

    size_t count = 0;
    HSD_sink_res sres = heatshrink_decoder_sink(hsd, r->buf, r->size, &count);
    if (sres != HSDR_SINK_OK) { return THEFT_TRIAL_ERROR; }
    
    size_t out_sz = 0;
    HSD_poll_res pres = heatshrink_decoder_poll(hsd, output, sizeof(output), &out_sz);
    if (pres != HSDR_POLL_EMPTY) { return THEFT_TRIAL_FAIL; }
    
    HSD_finish_res fres = heatshrink_decoder_finish(hsd);
    heatshrink_decoder_free(hsd);
    if (fres != HSDR_FINISH_DONE) { return THEFT_TRIAL_FAIL; }

    return THEFT_TRIAL_PASS;
}

static bool get_time_seed(theft_seed *seed)
{
    struct timeval tv;
    if (-1 == gettimeofday(&tv, NULL)) { return false; }
    *seed = (theft_seed)((tv.tv_sec << 32) | tv.tv_usec);
    /* printf("seed is 0x%016llx\n", *seed); */
    return true;
}

TEST decoder_fuzzing_should_not_detect_stuck_state(void) {
    // Get a random number seed based on the time
    theft_seed seed;
    if (!get_time_seed(&seed)) { FAIL(); }
    
    /* Pass the max buffer size for this property (4 KB) in a closure */
    test_env env = { .limit = 1 << 12 };

    theft_seed always_seeds = { 0xe87bb1f61032a061 };

    struct theft *t = theft_init(0);
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_should_not_get_stuck,
        .type_info = { &rbuf_info },
        .seed = seed,
        .trials = 100000,
        .progress_cb = progress_cb,
        .env = &env,

        .always_seeds = &always_seeds,
        .always_seed_count = 1,
    };

    theft_run_res res = theft_run(t, &cfg);
    theft_free(t);
    printf("\n");
    GREATEST_ASSERT_EQm("should_not_get_stuck", THEFT_RUN_PASS, res);
    PASS();
}

static theft_trial_res prop_encoded_and_decoded_data_should_match(void *input) {
    uint8_t e_output[64 * 1024];
    uint8_t d_output[64 * 1024];
    heatshrink_encoder *hse = heatshrink_encoder_alloc(12, 4);
    if (hse == NULL) { return THEFT_TRIAL_ERROR; }
    heatshrink_decoder *hsd = heatshrink_decoder_alloc(4096, 12, 4);
    if (hsd == NULL) { return THEFT_TRIAL_ERROR; }
    
    rbuf *r = (rbuf *)input;

    size_t e_input_size = 0;
    HSE_sink_res esres = heatshrink_encoder_sink(hse,
        r->buf, r->size, &e_input_size);
    if (esres != HSER_SINK_OK) { return THEFT_TRIAL_ERROR; }
    if (e_input_size != r->size) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }

    HSE_finish_res efres = heatshrink_encoder_finish(hse);
    if (efres != HSER_FINISH_MORE) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }

    size_t e_output_size = 0;
    HSE_poll_res epres = heatshrink_encoder_poll(hse,
        e_output, sizeof(e_output), &e_output_size);
    if (epres != HSER_POLL_EMPTY) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }

    size_t count = 0;
    HSD_sink_res sres = heatshrink_decoder_sink(hsd, e_output, e_output_size, &count);
    if (sres != HSDR_SINK_OK) { return THEFT_TRIAL_ERROR; }

    size_t d_output_size = 0;
    HSD_poll_res pres = heatshrink_decoder_poll(hsd, d_output,
        sizeof(d_output), &d_output_size);
    if (pres != HSDR_POLL_EMPTY) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }
    if (d_output_size != r->size) {
        printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL;
    }

    if (0 != memcmp(d_output, r->buf, d_output_size)) {
        return THEFT_TRIAL_FAIL;
    }
    
    HSD_finish_res fres = heatshrink_decoder_finish(hsd);
    if (fres != HSDR_FINISH_DONE) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }
    
    heatshrink_encoder_free(hse);
    heatshrink_decoder_free(hsd);
    return THEFT_TRIAL_PASS;
}


TEST encoded_and_decoded_data_should_match(void) {
    test_env env = { .limit = 1 << 11 };
    
    theft_seed seed;
    if (!get_time_seed(&seed)) { FAIL(); }

    struct theft *t = theft_init(0);
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_encoded_and_decoded_data_should_match,
        .type_info = { &rbuf_info },
        .seed = seed,
        .trials = 1000000,
        .env = &env,
        .progress_cb = progress_cb,
    };

    theft_run_res res = theft_run(t, &cfg);
    theft_free(t);
    printf("\n");
    ASSERT_EQ(THEFT_RUN_PASS, res);
    PASS();
}

static size_t ceil_nine_eighths(size_t sz) {
    return sz + sz/8 + (sz & 0x07 ? 1 : 0);
}

static theft_trial_res
prop_encoding_data_should_never_increase_it_by_more_than_an_eighth_at_worst(void *input) {
    uint8_t output[32 * 1024];
    heatshrink_encoder *hse = heatshrink_encoder_alloc(12, 4);
    if (hse == NULL) { return THEFT_TRIAL_ERROR; }
    
    rbuf *r = (rbuf *)input;

    size_t input_size = 0;
    HSE_sink_res esres = heatshrink_encoder_sink(hse,
        r->buf, r->size, &input_size);
    if (esres != HSER_SINK_OK) { return THEFT_TRIAL_ERROR; }
    /* Assumes data fits in one sink, failure here means buffer must be larger. */
    if (input_size != r->size) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }

    HSE_finish_res efres = heatshrink_encoder_finish(hse);
    if (efres != HSER_FINISH_MORE) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }

    size_t output_size = 0;
    HSE_poll_res epres = heatshrink_encoder_poll(hse,
        output, sizeof(output), &output_size);
    if (epres != HSER_POLL_EMPTY) { printf("FAIL %d\n", __LINE__); return THEFT_TRIAL_FAIL; }

    size_t ceil_9_8s = ceil_nine_eighths(r->size);
    if (output_size > ceil_9_8s) {
        return THEFT_TRIAL_FAIL;
    }

    heatshrink_encoder_free(hse);
    return THEFT_TRIAL_PASS;
}

TEST encoding_data_should_never_increase_it_by_more_than_an_eighth_at_worst(void) {
    test_env env = { .limit = 1 << 11 };
    
    theft_seed seed;
    if (!get_time_seed(&seed)) { FAIL(); }

    struct theft *t = theft_init(0);
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_encoding_data_should_never_increase_it_by_more_than_an_eighth_at_worst,
        .type_info = { &rbuf_info },
        .seed = seed,
        .trials = 10000,
        .env = &env,
        .progress_cb = progress_cb,
    };

    theft_run_res res = theft_run(t, &cfg);
    theft_free(t);
    printf("\n");
    ASSERT_EQ(THEFT_RUN_PASS, res);
    PASS();
}

static theft_trial_res
prop_encoder_should_always_make_progress(void *instance) {
    uint8_t output[64 * 1024];
    heatshrink_encoder *hse = heatshrink_encoder_alloc(8, 4);
    if (hse == NULL) { return THEFT_TRIAL_ERROR; }
    
    rbuf *r = (rbuf *)instance;

    size_t sunk = 0;
    int no_progress = 0;

    while (1) {
        if (sunk < r->size) {
            size_t input_size = 0;
            HSE_sink_res esres = heatshrink_encoder_sink(hse,
                &r->buf[sunk], r->size - sunk, &input_size);
            if (esres != HSER_SINK_OK) { return THEFT_TRIAL_ERROR; }
            sunk += input_size;
        } else {
            HSE_finish_res efres = heatshrink_encoder_finish(hse);
            if (efres == HSER_FINISH_DONE) {
                break;
            } else if (efres != HSER_FINISH_MORE) {
                printf("FAIL %d\n", __LINE__);
                return THEFT_TRIAL_FAIL;
            }
        }

        size_t output_size = 0;
        HSE_poll_res epres = heatshrink_encoder_poll(hse,
            output, sizeof(output), &output_size);
        if (epres < 0) { return THEFT_TRIAL_ERROR; }
        if (output_size == 0 && sunk == r->size) {
            no_progress++;
            if (no_progress > 2) {
                return THEFT_TRIAL_FAIL;
            }
        } else {
            no_progress = 0;
        }
    }

    heatshrink_encoder_free(hse);
    return THEFT_TRIAL_PASS;
}

TEST encoder_should_always_make_progress(void) {
    test_env env = { .limit = 1 << 15 };
    
    theft_seed seed;
    if (!get_time_seed(&seed)) { FAIL(); }

    struct theft *t = theft_init(0);
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_encoder_should_always_make_progress,
        .type_info = { &rbuf_info },
        .seed = seed,
        .trials = 10000,
        .env = &env,
        .progress_cb = progress_cb,
    };

    theft_run_res res = theft_run(t, &cfg);
    theft_free(t);
    printf("\n");
    ASSERT_EQ(THEFT_RUN_PASS, res);
    PASS();
}

static theft_trial_res
prop_decoder_should_always_make_progress(void *instance) {
    uint8_t output[64 * 1024];
    heatshrink_decoder *hsd = heatshrink_decoder_alloc(512, 8, 4);
    if (hsd == NULL) { return THEFT_TRIAL_ERROR; }
    
    rbuf *r = (rbuf *)instance;

    size_t sunk = 0;
    int no_progress = 0;

    while (1) {
        if (sunk < r->size) {
            size_t input_size = 0;
            HSD_sink_res sres = heatshrink_decoder_sink(hsd,
                &r->buf[sunk], r->size - sunk, &input_size);
            if (sres != HSER_SINK_OK) { return THEFT_TRIAL_ERROR; }
            sunk += input_size;
        } else {
            HSD_finish_res fres = heatshrink_decoder_finish(hsd);
            if (fres == HSDR_FINISH_DONE) {
                break;
            } else if (fres != HSDR_FINISH_MORE) {
                printf("FAIL %d\n", __LINE__);
                return THEFT_TRIAL_FAIL;
            }
        }

        size_t output_size = 0;
        HSD_poll_res pres = heatshrink_decoder_poll(hsd,
            output, sizeof(output), &output_size);
        if (pres < 0) { return THEFT_TRIAL_ERROR; }
        if (output_size == 0 && sunk == r->size) {
            no_progress++;
            if (no_progress > 2) {
                return THEFT_TRIAL_FAIL;
            }
        } else {
            no_progress = 0;
        }
    }

    heatshrink_decoder_free(hsd);
    return THEFT_TRIAL_PASS;
}

TEST decoder_should_always_make_progress(void) {
    test_env env = { .limit = 1 << 15 };
    
    theft_seed seed;
    if (!get_time_seed(&seed)) { FAIL(); }

    struct theft *t = theft_init(0);
    struct theft_cfg cfg = {
        .name = __func__,
        .fun = prop_decoder_should_always_make_progress,
        .type_info = { &rbuf_info },
        .seed = seed,
        .trials = 10000,
        .env = &env,
        .progress_cb = progress_cb,
    };

    theft_run_res res = theft_run(t, &cfg);
    theft_free(t);
    printf("\n");
    ASSERT_EQ(THEFT_RUN_PASS, res);
    PASS();
}

SUITE(properties) {
    RUN_TEST(decoder_fuzzing_should_not_detect_stuck_state);
    RUN_TEST(encoded_and_decoded_data_should_match);
    RUN_TEST(encoding_data_should_never_increase_it_by_more_than_an_eighth_at_worst);
    RUN_TEST(encoder_should_always_make_progress);
    RUN_TEST(decoder_should_always_make_progress);
}
#else
struct because_iso_c_requires_at_least_one_declaration;
#endif
