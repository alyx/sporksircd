/*
 *  respond.c: A challenge response generator for ircd-ratbox
 *
 *  Note: This is not compatible with previous versions of the CHALLENGE
 *  command, as the prior version was seriously flawed in many ways.
 * 
 *  Copyright (C) 2001 by the past and present ircd-hybrid developers.
 *  Copyright (C) 2005 Aaron Sethman <androsyn@ratbox.org> 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  $Id: ratbox-respond.c 21723 2006-01-19 17:57:07Z androsyn $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <unistd.h>

static int called_passcb = 0;
static int pass_cb(char *buf, int size, int rwflag, void *u)
{
	int len;
        char *tmp;

	called_passcb++;

        if(!isatty(fileno(stdin)))
	{
        	if(fgets(buf, size, stdin) == NULL)
        		return 0;
		tmp = strpbrk(buf, "\r\n");
		if(tmp != NULL)
			*tmp = '\0';
		return strlen(buf);
        }
	tmp = getpass("Enter passphrase for private key: ");
        len = strlen(tmp);
        if (len <= 0) 
		return 0;
        if (len > size)
        	len = size;
        memcpy(buf, tmp, len);
        return len;
}



static const char base64_table[] =
	{ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'
	};

static const char base64_pad = '=';

static const short base64_reverse_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

unsigned char *
base64_encode(const unsigned char *str, int length)
{
	const unsigned char *current = str;
	unsigned char *p;
	unsigned char *result;

	if ((length + 2) < 0 || ((length + 2) / 3) >= (1 << (sizeof(int) * 8 - 2))) {
		return NULL;
	}

	result = malloc(((length + 2) / 3) * 4);
	p = result;

	while (length > 2) 
	{ 
		*p++ = base64_table[current[0] >> 2];
		*p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
		*p++ = base64_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
		*p++ = base64_table[current[2] & 0x3f];

		current += 3;
		length -= 3; 
	}

	if (length != 0) {
		*p++ = base64_table[current[0] >> 2];
		if (length > 1) {
			*p++ = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
			*p++ = base64_table[(current[1] & 0x0f) << 2];
			*p++ = base64_pad;
		} else {
			*p++ = base64_table[(current[0] & 0x03) << 4];
			*p++ = base64_pad;
			*p++ = base64_pad;
		}
	}
	*p = '\0';
	return result;
}

unsigned char *
base64_decode(const unsigned char *str, int length, int *ret)
{
	const unsigned char *current = str;
	int ch, i = 0, j = 0, k;
	unsigned char *result;
	
	result = malloc(length + 1);

	while ((ch = *current++) != '\0' && length-- > 0) {
		if (ch == base64_pad) break;

		ch = base64_reverse_table[ch];
		if (ch < 0) continue;

		switch(i % 4) {
		case 0:
			result[j] = ch << 2;
			break;
		case 1:
			result[j++] |= ch >> 4;
			result[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			result[j++] |= ch >>2;
			result[j] = (ch & 0x03) << 6;
			break;
		case 3:
			result[j++] |= ch;
			break;
		}
		i++;
	}

	k = j;

	if (ch == base64_pad) {
		switch(i % 4) {
		case 1:
			free(result);
			return NULL;
		case 2:
			k++;
		case 3:
			result[k++] = 0;
		}
	}
	result[j] = '\0';
	*ret = j;
	return result;
}

unsigned char *
read_challenge(FILE *f)
{
	static unsigned char buf[16384];
	char *tmp;

	if(isatty(fileno(f)))
	{
		fprintf(stderr, "Please paste challenge text now\n");
	} else {
		if(!called_passcb)
		{
			/* throw away the unneeded password line */
			fgets((char *)buf, sizeof(buf), f);
		}
	}

        fgets((char *)buf, sizeof(buf), stdin);

	tmp = strpbrk((char *)buf, "\r\n");
	if(tmp != NULL)
		*tmp = '\0';

//	fread(buf, sizeof(buf), 1, f);
	return buf;
}


int
main(int argc, char **argv)
{
	FILE *kfile;
	RSA *rsa = NULL;
	SHA_CTX ctx;
	unsigned char *ptr;
	unsigned char *ndata, ddata[512];
	int len;

	/* respond privatefile challenge */
	if (argc < 2)
	{
		puts("Error: Usage: respond privatefile");
		return -1;
	}

	if (!(kfile = fopen(argv[1], "r")))
	{
		puts("Error: Could not open the private keyfile.");
		return -1;
	}

	SSLeay_add_all_ciphers();
	rsa = PEM_read_RSAPrivateKey(kfile, NULL,pass_cb, NULL);

	if(!rsa)
	{
		puts("Error: Unable to read your private key, is the passphrase wrong?");
		return -1;
	}

	fclose(kfile);

	ptr = read_challenge(stdin);
	ndata = base64_decode(ptr, strlen((char *)ptr), &len);
	if (ndata == NULL)
	{
		puts("Error: Bad challenge.");
		return -1;
	}

	if ((len = RSA_private_decrypt(len, (unsigned char*)ndata,
		(unsigned char*)ddata, rsa, RSA_PKCS1_OAEP_PADDING)) == -1)
	{
		puts("Error: Decryption error.");
		return -1;
	}

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, (unsigned char *)ddata, len);
	SHA1_Final((unsigned char *)ddata, &ctx);
	ndata = base64_encode((unsigned char *)ddata, SHA_DIGEST_LENGTH);
	if(isatty(fileno(stdin)))
	{
		fprintf(stderr, "Response: /quote CHALLENGE +");
	}
	puts((char *)ndata);
	fflush(NULL);
	return 0;
}
