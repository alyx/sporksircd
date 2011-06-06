/* The below was taken from FreeBSD */

/*-
 * Copyright (c) 2002 Thomas Moestl <tmm@FreeBSD.org>
 * Copyright (c) 2005 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Derived from FreeBSD src/sys/sys/endian.h:1.6.
 * $P4: //depot/projects/trustedbsd/openbsm/compat/endian.h#8 $
 */

#ifndef _COMPAT_ENDIAN_H_
#define _COMPAT_ENDIAN_H_

/*
 * Some operating systems do not yet have the more recent endian APIs that
 * permit encoding to and decoding from byte streams.  For those systems, we
 * implement local non-optimized versions.
 */

static inline uint16_t
rb_bswap16(uint16_t int16)
{
	const unsigned char *from;
	unsigned char *to;
	uint16_t t;

	from = (const unsigned char *) &int16;
	to = (unsigned char *) &t;

	to[0] = from[1];
	to[1] = from[0];

	return (t);
}

static inline uint32_t
rb_bswap32(uint32_t int32)
{
	const unsigned char *from;
	unsigned char *to;
	uint32_t t;

	from = (const unsigned char *) &int32;
	to = (unsigned char *) &t;

	to[0] = from[3];
	to[1] = from[2];
	to[2] = from[1];
	to[3] = from[0];

	return (t);
}

static inline uint64_t
rb_bswap64(uint64_t int64)
{
	const unsigned char *from;
	unsigned char *to;
	uint64_t t;

	from = (const unsigned char *) &int64;
	to = (unsigned char *) &t;

	to[0] = from[7];
	to[1] = from[6];
	to[2] = from[5];
	to[3] = from[4];
	to[4] = from[3];
	to[5] = from[2];
	to[6] = from[1];
	to[7] = from[0];

	return (t);
}

/*
 * Host to big endian, host to little endian, big endian to host, and little
 * endian to host byte order functions as detailed in byteorder(9).
 */
#ifndef WORDS_BIGENDIAN
#define	rb_htobe16(x)	rb_bswap16((x))
#define	rb_htobe32(x)	rb_bswap32((x))
#define	rb_htobe64(x)	rb_bswap64((x))
#define	rb_htole16(x)	((uint16_t)(x))
#define	rb_htole32(x)	((uint32_t)(x))
#define	rb_htole64(x)	((uint64_t)(x))

#define	rb_be16toh(x)	rb_bswap16((x))
#define	rb_be32toh(x)	rb_bswap32((x))
#define	rb_be64toh(x)	rb_bswap64((x))
#define	rb_le16toh(x)	((uint16_t)(x))
#define	rb_le32toh(x)	((uint32_t)(x))
#define	rb_le64toh(x)	((uint64_t)(x))
#else
#define	rb_htobe16(x)	((uint16_t)(x))
#define	rb_htobe32(x)	((uint32_t)(x))
#define	rb_htobe64(x)	((uint64_t)(x))
#define	rb_htole16(x)	rb_bswap16((x))
#define	rb_htole32(x)	rb_bswap32((x))
#define	rb_htole64(x)	rb_bswap64((x))

#define	rb_be16toh(x)	((uint16_t)(x))
#define	rb_be32toh(x)	((uint32_t)(x))
#define	rb_be64toh(x)	((uint64_t)(x))
#define	rb_le16toh(x)	rb_bswap16((x))
#define	rb_le32toh(x)	rb_bswap32((x))
#define	rb_le64toh(x)	rb_bswap64((x))
#endif

/* Alignment-agnostic encode/decode bytestream to/from little/big endian. */

static inline uint16_t
rb_be16dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[0] << 8) | p[1]);
}

static inline uint32_t
rb_be32dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static inline uint64_t
rb_be64dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return (((uint64_t)rb_be32dec(p) << 32) | rb_be32dec(p + 4));
}

static inline uint16_t
le16dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[1] << 8) | p[0]);
}

static inline uint32_t
rb_le32dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

static inline uint64_t
rb_le64dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return (((uint64_t)rb_le32dec(p + 4) << 32) | rb_le32dec(p));
}

static inline void
rb_be16enc(void *pp, uint16_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = (u >> 8) & 0xff;
	p[1] = u & 0xff;
}

static inline void
rb_be32enc(void *pp, uint32_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = (u >> 24) & 0xff;
	p[1] = (u >> 16) & 0xff;
	p[2] = (u >> 8) & 0xff;
	p[3] = u & 0xff;
}

static inline void
rb_be64enc(void *pp, uint64_t u)
{
	unsigned char *p = (unsigned char *)pp;

	rb_be32enc(p, u >> 32);
	rb_be32enc(p + 4, u & 0xffffffff);
}

static inline void
rb_le16enc(void *pp, uint16_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
}

static inline void
rb_le32enc(void *pp, uint32_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
	p[2] = (u >> 16) & 0xff;
	p[3] = (u >> 24) & 0xff;
}

static inline void
rb_le64enc(void *pp, uint64_t u)
{
	unsigned char *p = (unsigned char *)pp;

	rb_le32enc(p, u & 0xffffffff);
	rb_le32enc(p + 4, u >> 32);
}

#endif	/* _COMPAT_ENDIAN_H_ */
