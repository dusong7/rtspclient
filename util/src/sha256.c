/*
 * sha256.c
 *
 *  Created on: 2016年4月14日
 *      Author: WangZhen
 */

#include <util/util.h>
#include <util/sha1.h>

void sha224_init(sha256_state_t *c) {
	c->h[0] = 0xc1059ed8UL;
	c->h[1] = 0x367cd507UL;
	c->h[2] = 0x3070dd17UL;
	c->h[3] = 0xf70e5939UL;
	c->h[4] = 0xffc00b31UL;
	c->h[5] = 0x68581511UL;
	c->h[6] = 0x64f98fa7UL;
	c->h[7] = 0xbefa4fa4UL;
	c->Nl = 0;
	c->Nh = 0;
	c->num = 0;
	c->md_len = SHA224_DIGEST_LENGTH;
}

void sha256_init(sha256_state_t *c) {
	c->h[0] = 0x6a09e667UL;
	c->h[1] = 0xbb67ae85UL;
	c->h[2] = 0x3c6ef372UL;
	c->h[3] = 0xa54ff53aUL;
	c->h[4] = 0x510e527fUL;
	c->h[5] = 0x9b05688cUL;
	c->h[6] = 0x1f83d9abUL;
	c->h[7] = 0x5be0cd19UL;
	c->Nl = 0;
	c->Nh = 0;
	c->num = 0;
	c->md_len = SHA256_DIGEST_LENGTH;
}

void sha224_hash(const unsigned char *d, int n,
		unsigned char md[SHA224_DIGEST_LENGTH]) {
	sha256_state_t c;
	sha224_init(&c);
	sha256_append(&c, d, n);
	sha256_finish(&c, md);
}

void sha256_hash(const unsigned char *d, int n,
		unsigned char md[SHA256_DIGEST_LENGTH]) {
	sha256_state_t c;
	sha256_init(&c);
	sha256_append(&c, d, n);
	sha256_finish(&c, md);
}

void sha224_append(sha256_state_t *c, const unsigned char *data, int len) {
	sha256_append(c, data, len);
}
void sha224_finish(sha256_state_t *c, unsigned char md[SHA224_DIGEST_LENGTH]) {
	sha256_finish(c, md);
}
static const uint32_t K256[64] = { 0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL,
		0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
		0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL,
		0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL,
		0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL,
		0x76f988daUL, 0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
		0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL, 0x27b70a85UL,
		0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL, 0x650a7354UL, 0x766a0abbUL,
		0x81c2c92eUL, 0x92722c85UL, 0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL,
		0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
		0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL,
		0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL,
		0x84c87814UL, 0x8cc70208UL, 0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL,
		0xc67178f2UL };

#if 0

#define DATA_ORDER_IS_BIG_ENDIAN

#define HASH_LONG uint32_t
#define HASH_CTX sha256_state_t
#define HASH_CBLOCK SHA_CBLOCK

#define HASH_UPDATE 	sha256_append
#define HASH_TRANSFORM 	sha256_transform
#define HASH_FINAL 		sha256_final
#define HASH_BLOCK_DATA_ORDER sha256_block_data_order

static void sha256_block_data_order(sha256_state_t *ctx, const void *in, size_t num);

#define ROTATE(X,n) ((((X) << (n)) | ((X) >> (32-(n)))) & 0xffffffffL)

#define Sigma0(x) (ROTATE((x),30) ^ ROTATE((x),19) ^ ROTATE((x),10))
#define Sigma1(x) (ROTATE((x),26) ^ ROTATE((x),21) ^ ROTATE((x),7))
#define sigma0(x) (ROTATE((x),25) ^ ROTATE((x),14) ^ ((x)>>3))
#define sigma1(x) (ROTATE((x),15) ^ ROTATE((x),13) ^ ((x)>>10))

#define Ch(x,y,z) (((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#ifdef OPENSSL_SMALL_FOOTPRINT

static void sha256_block_data_order (sha256_state_t *ctx, const void *in, size_t num)
{
	unsigned MD32_REG_T a,b,c,d,e,f,g,h,s0,s1,T1,T2;
	uint32_t X[16],l;
	int i;
	const unsigned char *data=in;

	while (num--) {

		a = ctx->h[0]; b = ctx->h[1]; c = ctx->h[2]; d = ctx->h[3];
		e = ctx->h[4]; f = ctx->h[5]; g = ctx->h[6]; h = ctx->h[7];

		for (i=0;i<16;i++)
		{
			HOST_c2l(data,l); T1 = X[i] = l;
			T1 += h + Sigma1(e) + Ch(e,f,g) + K256[i];
			T2 = Sigma0(a) + Maj(a,b,c);
			h = g; g = f; f = e; e = d + T1;
			d = c; c = b; b = a; a = T1 + T2;
		}

		for (;i<64;i++)
		{
			s0 = X[(i+1)&0x0f]; s0 = sigma0(s0);
			s1 = X[(i+14)&0x0f]; s1 = sigma1(s1);

			T1 = X[i&0xf] += s0 + s1 + X[(i+9)&0xf];
			T1 += h + Sigma1(e) + Ch(e,f,g) + K256[i];
			T2 = Sigma0(a) + Maj(a,b,c);
			h = g; g = f; f = e; e = d + T1;
			d = c; c = b; b = a; a = T1 + T2;
		}

		ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
		ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;

	}
}

#else

#define ROUND_00_15(i,a,b,c,d,e,f,g,h) do { \
T1 += h + Sigma1(e) + Ch(e,f,g) + K256[i]; \
h = Sigma0(a) + Maj(a,b,c); \
d += T1; h += T1; } while (0)

#define ROUND_16_63(i,a,b,c,d,e,f,g,h,X) do { \
s0 = X[(i+1)&0x0f]; s0 = sigma0(s0); \
s1 = X[(i+14)&0x0f]; s1 = sigma1(s1); \
T1 = X[(i)&0x0f] += s0 + s1 + X[(i+9)&0x0f]; \
ROUND_00_15(i,a,b,c,d,e,f,g,h); } while (0)

static void sha256_block_data_order(sha256_state_t *ctx, const void *in,
		size_t num) {
	register uint32_t a, b, c, d, e, f, g, h, s0, s1, T1;
	uint32_t X[16];
	int i;
	const unsigned char *data = in;
	const union {
		long one;
		char little;
	}is_endian = {1};

	while (num--) {

		a = ctx->h[0];
		b = ctx->h[1];
		c = ctx->h[2];
		d = ctx->h[3];
		e = ctx->h[4];
		f = ctx->h[5];
		g = ctx->h[6];
		h = ctx->h[7];

		if (!is_endian.little && sizeof(uint32_t) == 4
				&& ((size_t) in % 4) == 0) {
			const uint32_t *W = (const uint32_t *) data;

			T1 = X[0] = W[0];
			ROUND_00_15(0, a, b, c, d, e, f, g, h);
			T1 = X[1] = W[1];
			ROUND_00_15(1, h, a, b, c, d, e, f, g);
			T1 = X[2] = W[2];
			ROUND_00_15(2, g, h, a, b, c, d, e, f);
			T1 = X[3] = W[3];
			ROUND_00_15(3, f, g, h, a, b, c, d, e);
			T1 = X[4] = W[4];
			ROUND_00_15(4, e, f, g, h, a, b, c, d);
			T1 = X[5] = W[5];
			ROUND_00_15(5, d, e, f, g, h, a, b, c);
			T1 = X[6] = W[6];
			ROUND_00_15(6, c, d, e, f, g, h, a, b);
			T1 = X[7] = W[7];
			ROUND_00_15(7, b, c, d, e, f, g, h, a);
			T1 = X[8] = W[8];
			ROUND_00_15(8, a, b, c, d, e, f, g, h);
			T1 = X[9] = W[9];
			ROUND_00_15(9, h, a, b, c, d, e, f, g);
			T1 = X[10] = W[10];
			ROUND_00_15(10, g, h, a, b, c, d, e, f);
			T1 = X[11] = W[11];
			ROUND_00_15(11, f, g, h, a, b, c, d, e);
			T1 = X[12] = W[12];
			ROUND_00_15(12, e, f, g, h, a, b, c, d);
			T1 = X[13] = W[13];
			ROUND_00_15(13, d, e, f, g, h, a, b, c);
			T1 = X[14] = W[14];
			ROUND_00_15(14, c, d, e, f, g, h, a, b);
			T1 = X[15] = W[15];
			ROUND_00_15(15, b, c, d, e, f, g, h, a);

			data += SHA256_CBLOCK;
		} else {
			uint32_t l;

			HOST_c2l(data, l);
			T1 = X[0] = l;
			ROUND_00_15(0, a, b, c, d, e, f, g, h);
			HOST_c2l(data, l);
			T1 = X[1] = l;
			ROUND_00_15(1, h, a, b, c, d, e, f, g);
			HOST_c2l(data, l);
			T1 = X[2] = l;
			ROUND_00_15(2, g, h, a, b, c, d, e, f);
			HOST_c2l(data, l);
			T1 = X[3] = l;
			ROUND_00_15(3, f, g, h, a, b, c, d, e);
			HOST_c2l(data, l);
			T1 = X[4] = l;
			ROUND_00_15(4, e, f, g, h, a, b, c, d);
			HOST_c2l(data, l);
			T1 = X[5] = l;
			ROUND_00_15(5, d, e, f, g, h, a, b, c);
			HOST_c2l(data, l);
			T1 = X[6] = l;
			ROUND_00_15(6, c, d, e, f, g, h, a, b);
			HOST_c2l(data, l);
			T1 = X[7] = l;
			ROUND_00_15(7, b, c, d, e, f, g, h, a);
			HOST_c2l(data, l);
			T1 = X[8] = l;
			ROUND_00_15(8, a, b, c, d, e, f, g, h);
			HOST_c2l(data, l);
			T1 = X[9] = l;
			ROUND_00_15(9, h, a, b, c, d, e, f, g);
			HOST_c2l(data, l);
			T1 = X[10] = l;
			ROUND_00_15(10, g, h, a, b, c, d, e, f);
			HOST_c2l(data, l);
			T1 = X[11] = l;
			ROUND_00_15(11, f, g, h, a, b, c, d, e);
			HOST_c2l(data, l);
			T1 = X[12] = l;
			ROUND_00_15(12, e, f, g, h, a, b, c, d);
			HOST_c2l(data, l);
			T1 = X[13] = l;
			ROUND_00_15(13, d, e, f, g, h, a, b, c);
			HOST_c2l(data, l);
			T1 = X[14] = l;
			ROUND_00_15(14, c, d, e, f, g, h, a, b);
			HOST_c2l(data, l);
			T1 = X[15] = l;
			ROUND_00_15(15, b, c, d, e, f, g, h, a);
		}

		for (i = 16; i < 64; i += 8) {
			ROUND_16_63(i + 0, a, b, c, d, e, f, g, h, X);
			ROUND_16_63(i + 1, h, a, b, c, d, e, f, g, X);
			ROUND_16_63(i + 2, g, h, a, b, c, d, e, f, X);
			ROUND_16_63(i + 3, f, g, h, a, b, c, d, e, X);
			ROUND_16_63(i + 4, e, f, g, h, a, b, c, d, X);
			ROUND_16_63(i + 5, d, e, f, g, h, a, b, c, X);
			ROUND_16_63(i + 6, c, d, e, f, g, h, a, b, X);
			ROUND_16_63(i + 7, b, c, d, e, f, g, h, a, X);
		}

		ctx->h[0] += a;
		ctx->h[1] += b;
		ctx->h[2] += c;
		ctx->h[3] += d;
		ctx->h[4] += e;
		ctx->h[5] += f;
		ctx->h[6] += g;
		ctx->h[7] += h;

	}
}

#endif
#else
/* define it for speed optimization */
/* #define _SHA256_UNROLL */
/* #define _SHA256_UNROLL2 */

#define rotrFixed(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define S0(x) (rotrFixed(x, 2) ^ rotrFixed(x,13) ^ rotrFixed(x, 22))
#define S1(x) (rotrFixed(x, 6) ^ rotrFixed(x,11) ^ rotrFixed(x, 25))
#define s0(x) (rotrFixed(x, 7) ^ rotrFixed(x,18) ^ (x >> 3))
#define s1(x) (rotrFixed(x,17) ^ rotrFixed(x,19) ^ (x >> 10))
#define blk0(i) (W[i] = data[i])
#define blk2(i) (W[i&15] += s1(W[(i-2)&15]) + W[(i-7)&15] + s0(W[(i-15)&15]))
#define Ch(x,y,z) (z^(x&(y^z)))
#define Maj(x,y,z) ((x&y)|(z&(x|y)))
#define a(i) T[(0-(i))&7]
#define b(i) T[(1-(i))&7]
#define c(i) T[(2-(i))&7]
#define d(i) T[(3-(i))&7]
#define e(i) T[(4-(i))&7]
#define f(i) T[(5-(i))&7]
#define g(i) T[(6-(i))&7]
#define h(i) T[(7-(i))&7]
#ifdef _SHA256_UNROLL2
#define R(a,b,c,d,e,f,g,h, i) h += S1(e) + Ch(e,f,g) + K[i+j] + (j?blk2(i):blk0(i));
d += h; h += S0(a) + Maj(a, b, c)
#define RX_8(i)
R(a,b,c,d,e,f,g,h, i);
R(h,a,b,c,d,e,f,g, i+1);
R(g,h,a,b,c,d,e,f, i+2);
R(f,g,h,a,b,c,d,e, i+3);
R(e,f,g,h,a,b,c,d, i+4);
R(d,e,f,g,h,a,b,c, i+5);
R(c,d,e,f,g,h,a,b, i+6);
R(b,c,d,e,f,g,h,a, i+7)
#else
#define R(i) h(i) += S1(e(i)) + Ch(e(i),f(i),g(i)) + K256[i+j] + (j?blk2(i):blk0(i)); \
  d(i) += h(i); h(i) += S0(a(i)) + Maj(a(i), b(i), c(i))
#ifdef _SHA256_UNROLL
#define RX_8(i) R(i+0); R(i+1); R(i+2); R(i+3); R(i+4); R(i+5); R(i+6); R(i+7);
#endif
#endif

static void _sha256_transform(uint32_t *state, const uint32_t *data) {
	uint32_t W[16];
	unsigned j;
#ifdef _SHA256_UNROLL2
	uint32_t a,b,c,d,e,f,g,h;
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	f = state[5];
	g = state[6];
	h = state[7];
#else
	uint32_t T[8];
	for (j = 0; j < 8; j++)
		T[j] = state[j];
#endif
	for (j = 0; j < 64; j += 16) {
#if defined(_SHA256_UNROLL) || defined(_SHA256_UNROLL2)
		RX_8(0); RX_8(8);
#else
		unsigned i;
		for (i = 0; i < 16; i++) {
			R(i);
		}
#endif
	}
#ifdef _SHA256_UNROLL2
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
#else
	for (j = 0; j < 8; j++)
		state[j] += T[j];
#endif

	/* Wipe variables */
	/* memset(W, 0, sizeof(W)); */
	/* memset(T, 0, sizeof(T)); */
}
#undef S0
#undef S1
#undef s0
#undef s1
static void _sha256_writeByteBlock(sha256_state_t *p) {
	uint32_t data32[16];
	unsigned i;
	for (i = 0; i < 16; i++)
		data32[i] = ((uint32_t) (p->data[i * 4]) << 24)
				+ ((uint32_t) (p->data[i * 4 + 1]) << 16)
				+ ((uint32_t) (p->data[i * 4 + 2]) << 8)
				+ ((uint32_t) (p->data[i * 4 + 3]));
	_sha256_transform(p->h, data32);
}
void sha256_append(sha256_state_t *p, const unsigned char *data, int size) {
	uint32_t curBufferPos = (uint32_t) p->num & 0x3F;
	while (size > 0) {
		p->data[curBufferPos++] = *data++;
		p->num++;
		size--;
		if (curBufferPos == 64) {
			curBufferPos = 0;
			_sha256_writeByteBlock(p);
		}
	}
}
void sha256_finish(sha256_state_t *p, unsigned char digest[SHA256_DIGEST_LENGTH]) {
	uint64_t lenInBits = (p->num << 3);
	uint32_t curBufferPos = (uint32_t) p->num & 0x3F;
	unsigned i;
	int n = p->md_len / 4;
	p->data[curBufferPos++] = 0x80;
	while (curBufferPos != (64 - 8)) {
		curBufferPos &= 0x3F;
		if (curBufferPos == 0)
			_sha256_writeByteBlock(p);
		p->data[curBufferPos++] = 0;
	}
	for (i = 0; i < 8; i++) {
		p->data[curBufferPos++] = (unsigned char) (lenInBits >> 56);
		lenInBits <<= 8;
	}
	_sha256_writeByteBlock(p);
	for (i = 0; i < n; i++) {
		*digest++ = (unsigned char) (p->h[i] >> 24);
		*digest++ = (unsigned char) (p->h[i] >> 16);
		*digest++ = (unsigned char) (p->h[i] >> 8);
		*digest++ = (unsigned char) (p->h[i]);
	}
}
#endif

