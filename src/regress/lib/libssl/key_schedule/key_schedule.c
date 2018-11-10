/* $OpenBSD: key_schedule.c,v 1.3 2018/11/10 00:18:25 beck Exp $ */
/*
 * Copyright (c) 2018 Bob Beck <beck@openbsd.org>
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

#include "ssl_locl.h"

#include "bytestring.h"
#include "ssl_tlsext.h"
#include "tls13_internal.h"

static int failures = 0;

static void
hexdump(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 1; i <= len; i++)
		fprintf(stderr, " 0x%02hhx,%s", buf[i - 1], i % 8 ? "" : "\n");

	fprintf(stderr, "\n");
}

static void
compare_data(const uint8_t *recv, size_t recv_len, const uint8_t *expect,
    size_t expect_len)
{
	fprintf(stderr, "received:\n");
	hexdump(recv, recv_len);

	fprintf(stderr, "test data:\n");
	hexdump(expect, expect_len);
}

#define FAIL(msg, ...)						\
do {								\
	fprintf(stderr, "[%s:%d] FAIL: ", __FILE__, __LINE__);	\
	fprintf(stderr, msg, ##__VA_ARGS__);			\
	failures++;						\
} while(0)

/* Hashes and secrets from test vector */

uint8_t chello[] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
	0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
	0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
	0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};
const struct tls13_secret chello_hash = {
	.data = chello,
	.len = 32,
};

uint8_t cshello [] = {
	0x86, 0x0c, 0x06, 0xed, 0xc0, 0x78, 0x58, 0xee,
	0x8e, 0x78, 0xf0, 0xe7, 0x42, 0x8c, 0x58, 0xed,
	0xd6, 0xb4, 0x3f, 0x2c, 0xa3, 0xe6, 0xe9, 0x5f,
	0x02, 0xed, 0x06, 0x3c, 0xf0, 0xe1, 0xca, 0xd8
};

const struct tls13_secret cshello_hash = {
	.data = cshello,
	.len = 32,
};

const uint8_t ecdhe [] = {
	0x8b, 0xd4, 0x05, 0x4f, 0xb5, 0x5b, 0x9d, 0x63,
	0xfd, 0xfb, 0xac, 0xf9, 0xf0, 0x4b, 0x9f, 0x0d,
	0x35, 0xe6, 0xd6, 0x3f, 0x53, 0x75, 0x63, 0xef,
	0xd4, 0x62, 0x72, 0x90, 0x0f, 0x89, 0x49, 0x2d
};

/* Expected Values */

uint8_t expected_extracted_early[] = {
	0x33, 0xad, 0x0a, 0x1c, 0x60, 0x7e, 0xc0, 0x3b,
	0x09, 0xe6, 0xcd, 0x98, 0x93, 0x68, 0x0c, 0xe2,
	0x10, 0xad, 0xf3, 0x00, 0xaa, 0x1f, 0x26, 0x60,
	0xe1, 0xb2, 0x2e, 0x10, 0xf1, 0x70, 0xf9, 0x2a
};
uint8_t expected_derived_early[] = {
	0x6f, 0x26, 0x15, 0xa1, 0x08, 0xc7, 0x02, 0xc5,
	0x67, 0x8f, 0x54, 0xfc, 0x9d, 0xba, 0xb6, 0x97,
	0x16, 0xc0, 0x76, 0x18, 0x9c, 0x48, 0x25, 0x0c,
	0xeb, 0xea, 0xc3, 0x57, 0x6c, 0x36, 0x11, 0xba
};
uint8_t expected_extracted_handshake[] = {
	0x1d, 0xc8, 0x26, 0xe9, 0x36, 0x06, 0xaa, 0x6f,
	0xdc, 0x0a, 0xad, 0xc1, 0x2f, 0x74, 0x1b, 0x01,
	0x04, 0x6a, 0xa6, 0xb9, 0x9f, 0x69, 0x1e, 0xd2,
	0x21, 0xa9, 0xf0, 0xca, 0x04, 0x3f, 0xbe, 0xac
};
uint8_t expected_client_handshake_traffic[] = {
	0xb3, 0xed, 0xdb, 0x12, 0x6e, 0x06, 0x7f, 0x35,
	0xa7, 0x80, 0xb3, 0xab, 0xf4, 0x5e, 0x2d, 0x8f,
	0x3b, 0x1a, 0x95, 0x07, 0x38, 0xf5, 0x2e, 0x96,
	0x00, 0x74, 0x6a, 0x0e, 0x27, 0xa5, 0x5a, 0x21
};

uint8_t expected_server_handshake_traffic[] = {
	0xb6, 0x7b, 0x7d, 0x69, 0x0c, 0xc1, 0x6c, 0x4e,
	0x75, 0xe5, 0x42, 0x13, 0xcb, 0x2d, 0x37, 0xb4,
	0xe9, 0xc9, 0x12, 0xbc, 0xde, 0xd9, 0x10, 0x5d,
	0x42, 0xbe, 0xfd, 0x59, 0xd3, 0x91, 0xad, 0x38
};

uint8_t expected_derived_handshake[] = {
	0x43, 0xde, 0x77, 0xe0, 0xc7, 0x77, 0x13, 0x85,
	0x9a, 0x94, 0x4d, 0xb9, 0xdb, 0x25, 0x90, 0xb5,
	0x31, 0x90, 0xa6, 0x5b, 0x3e, 0xe2, 0xe4, 0xf1,
	0x2d, 0xd7, 0xa0, 0xbb, 0x7c, 0xe2, 0x54, 0xb4
};

uint8_t expected_extracted_master[] = {
	0x18, 0xdf, 0x06, 0x84, 0x3d, 0x13, 0xa0, 0x8b,
	0xf2, 0xa4, 0x49, 0x84, 0x4c, 0x5f, 0x8a, 0x47,
	0x80, 0x01, 0xbc, 0x4d, 0x4c, 0x62, 0x79, 0x84,
	0xd5, 0xa4, 0x1d, 0xa8, 0xd0, 0x40, 0x29, 0x19
};

int main () {
	struct tls13_secrets *secrets;

	if ((secrets = tls13_secrets_create(EVP_sha256(), 0)) == NULL)
		FAIL("failed to create secrets\n");

	secrets->insecure = 1; /* don't explicit_bzero when done */

	if (tls13_derive_handshake_secrets(secrets, ecdhe, 32, &cshello_hash))
		FAIL("derive_handshake_secrets worked when it shouldn't\n");
	if (tls13_derive_application_secrets(secrets,
	    &chello_hash))
		FAIL("derive_application_secrets worked when it shouldn't\n");

	if (!tls13_derive_early_secrets(secrets,
	    secrets->zeros.data, secrets->zeros.len, &chello_hash))
		FAIL("derive_early_secrets failed\n");
	if (tls13_derive_early_secrets(secrets,
	    secrets->zeros.data, secrets->zeros.len, &chello_hash))
		FAIL("derive_early_secrets worked when it shouldn't(2)\n");

	if (!tls13_derive_handshake_secrets(secrets, ecdhe, 32, &cshello_hash))
		FAIL("derive_handshake_secrets failed\n");
	if (tls13_derive_handshake_secrets(secrets, ecdhe, 32, &cshello_hash))
		FAIL("derive_handshake_secrets worked when it shouldn't(2)\n");

	/* XXX fix hash here once test vector sorted */
	if (!tls13_derive_application_secrets(secrets, &cshello_hash))
		FAIL("derive_application_secrets failed\n");
	if (tls13_derive_application_secrets(secrets, &cshello_hash))
		FAIL("derive_application_secrets worked when it "
		    "shouldn't(2)\n");

	fprintf(stderr, "extracted_early:\n");
	compare_data(secrets->extracted_early.data, 32,
	    expected_extracted_early, 32);
	if (memcmp(secrets->extracted_early.data,
	    expected_extracted_early, 32) != 0)
		FAIL("extracted_early does not match\n");

	fprintf(stderr, "derived_early:\n");
	compare_data(secrets->derived_early.data, 32,
	    expected_derived_early, 32);
	if (memcmp(secrets->derived_early.data,
	    expected_derived_early, 32) != 0)
		FAIL("derived_early does not match\n");

	fprintf(stderr, "extracted_handshake:\n");
	compare_data(secrets->extracted_handshake.data, 32,
	    expected_extracted_handshake, 32);
	if (memcmp(secrets->extracted_handshake.data,
	    expected_extracted_handshake, 32) != 0)
		FAIL("extracted_handshake does not match\n");

	fprintf(stderr, "client_handshake_traffic:\n");
	compare_data(secrets->client_handshake_traffic.data, 32,
	    expected_client_handshake_traffic, 32);
	if (memcmp(secrets->client_handshake_traffic.data,
	    expected_client_handshake_traffic, 32) != 0)
		FAIL("client_handshake_traffic does not match\n");

	fprintf(stderr, "server_handshake_traffic:\n");
	compare_data(secrets->server_handshake_traffic.data, 32,
	    expected_server_handshake_traffic, 32);
	if (memcmp(secrets->server_handshake_traffic.data,
	    expected_server_handshake_traffic, 32) != 0)
		FAIL("server_handshake_traffic does not match\n");

	fprintf(stderr, "derived_early:\n");
	compare_data(secrets->derived_early.data, 32,
	    expected_derived_early, 32);
	if (memcmp(secrets->derived_early.data,
	    expected_derived_early, 32) != 0)
		FAIL("derived_early does not match\n");

	/* XXX this is currently totally volkswagened from above */
	fprintf(stderr, "derived_handshake:\n");
	compare_data(secrets->derived_handshake.data, 32,
	    expected_derived_handshake, 32);
	if (memcmp(secrets->derived_handshake.data,
	    expected_derived_handshake, 32) != 0)
		FAIL("derived_handshake does not match\n");

	fprintf(stderr, "extracted_master:\n");
	compare_data(secrets->extracted_master.data, 32,
	    expected_extracted_master, 32);
	if (memcmp(secrets->extracted_master.data,
	    expected_extracted_master, 32) != 0)
		FAIL("extracted_master does not match\n");

	return failures;
}
