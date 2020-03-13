/* $OpenBSD: record_layer_test.c,v 1.1 2020/03/13 16:04:31 jsing Exp $ */
/*
 * Copyright (c) 2019, 2020 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <string.h>

#include "tls13_internal.h"
#include "tls13_record.h"

int tls13_record_layer_inc_seq_num(uint8_t *seq_num);

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02x,%s", buf[i - 1], i % 8 ? "" : "\n");
	if (len % 8 != 0)
		fprintf(stderr, "\n");
}

struct seq_num_test {
	uint8_t seq_num[TLS13_RECORD_SEQ_NUM_LEN];
	uint8_t want_num[TLS13_RECORD_SEQ_NUM_LEN];
	int want;
};

struct seq_num_test seq_num_tests[] = {
	{
		.seq_num = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.want_num = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
		.want = 1,
	},
	{
		.seq_num = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
		.want_num = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
		.want = 1,
	},
	{
		.seq_num = {0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		.want_num = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.want = 1,
	},
	{
		.seq_num = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe},
		.want_num = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		.want = 1,
	},
	{
		.seq_num = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		.want_num = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.want = 0,
	},
};

#define N_SEQ_NUM_TESTS (sizeof(seq_num_tests) / sizeof(seq_num_tests[0]))

static int
do_seq_num_test(size_t test_no, struct seq_num_test *snt)
{
	uint8_t seq_num[TLS13_RECORD_SEQ_NUM_LEN];
	int failed = 1;
	int ret;

	memcpy(seq_num, snt->seq_num, sizeof(seq_num));

	if ((ret = tls13_record_layer_inc_seq_num(seq_num)) != snt->want) {
		fprintf(stderr, "FAIL: Test %zu - got return %i, want %i\n",
		    test_no, ret, snt->want);
		goto failure;
	}

	if (memcmp(seq_num, snt->want_num, sizeof(seq_num)) != 0) {
		fprintf(stderr, "FAIL: Test %zu - got sequence number:\n",
		    test_no);
		hexdump(seq_num, sizeof(seq_num));
		fprintf(stderr, "want:\n");
		hexdump(snt->want_num, sizeof(snt->want_num));
		goto failure;
	}

	failed = 0;

 failure:
	return failed;
}

static int
test_seq_num(void)
{
	int failed = 0;
	size_t i;

	for (i = 0; i < N_SEQ_NUM_TESTS; i++)
		failed |= do_seq_num_test(i, &seq_num_tests[i]);

	return failed;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= test_seq_num();

	return failed;
}
