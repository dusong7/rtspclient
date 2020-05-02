/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002-2003 Jeremie Miller, Thomas Muldowney,
 *                         Ryan Eatmon, Robert Norris
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

/* simple hex conversion functions */

#include "util/util.h"

/** turn raw into hex - out must be (inlen*2)+1 */
void hex_from_raw(const char *in, int inlen, char *out) {
    int i, h, l;

    for(i = 0; i < inlen; i++) {
        h = in[i] & 0xf0;
        h >>= 4;
        l = in[i] & 0x0f;
        out[i * 2] = (h >= 0x0 && h <= 0x9) ? (h + 0x30) : (h + 0x57);
        out[i * 2 + 1] = (l >= 0x0 && l <= 0x9) ? (l + 0x30) : (l + 0x57);
    }
    out[i * 2] = '\0';
}

/** turn hex into raw - out must be (inlen/2) */
int hex_to_raw(const char *in, int inlen, char *out) {
    int i, o, h, l;

    /* need +ve even input */
    if(inlen == 0 || (inlen / 2 * 2) != inlen)
        return 1;

    for(i = o = 0; i < inlen; i += 2, o++) {
        h = (in[i] >= 0x30 && in[i] <= 0x39) ? (in[i] - 0x30) : (in[i] >= 0x41 && in[i] <= 0x46) ? (in[i] - 0x37) : (in[i] >= 0x61 && in[i] <= 0x66) ? (in[i] - 0x57) : -1;
        l = (in[i + 1] >= 0x30 && in[i + 1] <= 0x39) ? (in[i + 1] - 0x30) : (in[i + 1] >= 0x41 && in[i + 1] <= 0x46) ? (in[i + 1] - 0x37) : (in[i + 1] >= 0x61 && in[i + 1] <= 0x66) ? (in[i + 1] - 0x57) : -1;

        if(h < 0 || l < 0)
            return 1;

        out[o] = (h << 4) + l;
    }

    return 0;
}

/** turn uri escaped str to original - out must be no smaller than (inlen * 3 + 1) */
int j_uri_encode(const char* in, int inlen, char* out){
	int pos = 0;
	const char* end = in + inlen;
	while(in < end){
		register char c = *in;
		if((c >= 'A' && c <= 'Z') ||
				(c >= 'a' && c <= 'z') ||
				(c >= '0' && c<= '9') ||
				c == '.' || c == '-' || c == '_'){
			out[pos++] = c;
		} else if(c == ' '){
			out[pos++] = '+';
		} else {
			out[pos++] = '%';
			hex_from_raw(in, 1, &out[pos]);
			pos += 2;
		}
		++in;
	}
	out[pos] = '\0';
	return pos;
}

/** turn uri escaped str to original - out must be no smaller than (inlen + 1) */
int j_uri_decode(const char* in, int inlen, char* out){
	int pos = 0;
	const char* end = in + inlen;
	while(in < end && *in){
		if((unsigned)*in > 127){
			// this is not an ascii char. can not decode, just copy it till end.
			break;
		}
		switch(*in){
		case '+':
			out[pos ++] = ' ';
			break;
		case '%':
			// following 2 byte contains hex for one byte.
			if(in + 2 < end){
				hex_to_raw(in + 1, 2, &out[pos++]);
				in += 3;
				continue;
			} else {
				// ignore bad char.
			}
			break;
		default:
			out[pos ++] = *in;
			break;
		}
		++ in;
	}

	while(in < end && *in){
		out[pos++] = *in;
		++ in;
	}
	out[pos] = '\0';
	return pos;
}
