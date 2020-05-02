/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002 Jeremie Miller, Thomas Muldowney,
 *                    Ryan Eatmon, Robert Norris
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

#include "util/pool.h"
#include "util/util.h"
#include <stdio.h>

char *j_strdup(const char *str)
{
    if(str == NULL)
        return NULL;
    else
        return strdup(str);
}
void j_free(void *m){
	free(m);
}
char* j_strndup(const char* str, int len)
{
	if(str == NULL)
		return NULL;
	if(len == 0)
		return strdup("");
	else{
		char* dst = malloc(len + 1);
		memcpy(dst, str, len);
		dst[len] = 0;
		return dst;
	}
}
void* j_memdup(const void* src, int len)
{
	void *dst = malloc(len);
	if(dst)
		memcpy(dst, src, len);
	return dst;
}
void *j_memmem(const void *src, int len_src, const void *search, int len_s)
{
	char* p_src = (char*)src;
	while(len_src >= len_s){
		if(0 == memcmp(p_src, search, len_s)){
			return p_src;
		}
		++ p_src;
		-- len_src;
	}
	return NULL;
}

char *j_strtok_r(char *s, const char *delim, char **state) {
	char *cp, *start;
	start = cp = s ? s : *state;
	if (!cp)
		return NULL;
	while (*cp && !strchr(delim, *cp))
		++cp;
	if (!*cp) {
		if (cp == start)
			return NULL;
		*state = NULL;
		return start;
	} else {
		*cp++ = '\0';
		*state = cp;
		return start;
	}
}

char* j_strerror_r(int err, char dst[], int dstlen){
	snprintf(dst, dstlen, "error %d", err);
	return dst;
}

char *j_strcat(char *dest, char *txt)
{
    if(!txt) return(dest);

    while(*txt)
        *dest++ = *txt++;
    *dest = '\0';

    return(dest);
}

int j_strcmp(const char *a, const char *b)
{
	if(a == NULL && b == NULL) return 0;
    if(a == NULL || b == NULL)
        return a ? 1 : -1;

    while(*a == *b && *a != '\0' && *b != '\0'){ a++; b++; }

    if(*a == *b) return 0;

    return *a > *b ? 1 : -1;
}

int j_strcasecmp(const char *a, const char *b)
{
	if(a == NULL && b == NULL) return 0;
    if(a == NULL || b == NULL)
        return -1;
    else
        return strcasecmp(a, b);
}

int j_strncmp(const char *a, const char *b, int i)
{
    if(a == NULL || b == NULL)
        return -1;
    else
        return strncmp(a, b, i);
}

int j_strncasecmp(const char *a, const char *b, int i)
{
    if(a == NULL || b == NULL)
        return -1;
    else
        return strncasecmp(a, b, i);
}

int j_strlen(const char *a)
{
    if(a == NULL)
        return 0;
    else
        return strlen(a);
}

int j_atoi(const char *a, int def)
{
    if(a == NULL){
        return def;
	} else if(a[0] == '0' && tolower(a[1]) == 'x' ){
    	return strtol(&a[2], NULL, 16);
    } else {
        return atoi(a);
    }
}

int j_atoi_ex(const char* a, int len, int def)
{
	int v = 0;
	int i = 0;
	int is_neg = 0;
	if(a == NULL) return def;
	if(a[0] == '-'){
		is_neg = -1;
		++i;
	}
	while(i < len && a[i] >= '0' && a[i] <= '9'){
		register int d = a[i] - '0';
		v = v * 10 + d;
		++i;
	}
	if(is_neg)
		v = -v;
	return v;
}

char *j_attr(const char** atts, const char *attr)
{
    int i = 0;

    while(atts[i] != '\0')
    {
        if(j_strcmp(atts[i],attr) == 0) return (char*)atts[i+1];
        i += 2;
    }

    return NULL;
}

/** like strchr, but only searches n chars */
char *j_strnchr(const char *s, int c, int n) {
    int count;

    for(count = 0; count < n; count++)
        if(s[count] == (char) c)
            return &((char *)s)[count];

    return NULL;
}

char* j_str_ltrim(const char* src){
	while(*src && *src <= ' ') ++ src;
	return (char*)src;
}

char* j_str_rtrim(char* src){
	if(!src) return NULL;
	char* p = src + strlen(src) - 1;
	while(p >= src && *p <= ' ') --p;
	p[1] = '\0';
	return src;
}

spool spool_new(pool_t p)
{
    spool s;

    s = pmalloc(p, sizeof(struct spool_struct));
    s->p = p;
    s->len = 0;
    s->last = NULL;
    s->first = NULL;
    return s;
}

static void _spool_add(spool s, char *goodstr)
{
    struct spool_node *sn;

    sn = pmalloc(s->p, sizeof(struct spool_node));
    sn->c = goodstr;
    sn->next = NULL;

    s->len += strlen(goodstr);
    if(s->last != NULL)
        s->last->next = sn;
    s->last = sn;
    if(s->first == NULL)
        s->first = sn;
}

void spool_add(spool s, char *str)
{
    if(str == NULL || strlen(str) == 0)
        return;

    _spool_add(s, pstrdup(s->p, str));
}

void spool_escape(spool s, char *raw, int len)
{
    if(raw == NULL || len <= 0)
        return;

    _spool_add(s, strescape(s->p, raw, len));
}

void spooler(spool s, ...)
{
    va_list ap;
    char *arg = NULL;

    if(s == NULL)
        return;

    va_start(ap, s);

    /* loop till we hit our end flag, the first arg */
    while(1)
    {
        arg = va_arg(ap,char *);
        if((spool)arg == s)
            break;
        else
            spool_add(s, arg);
    }

    va_end(ap);
}

char *spool_print(spool s)
{
    char *ret,*tmp;
    struct spool_node *next;

    if(s == NULL || s->len == 0 || s->first == NULL)
        return NULL;

    ret = pmalloc(s->p, s->len + 1);
    *ret = '\0';

    next = s->first;
    tmp = ret;
    while(next != NULL)
    {
        tmp = j_strcat(tmp,next->c);
        next = next->next;
    }

    return ret;
}

/** convenience :) */
char *spools(pool_t p, ...)
{
    va_list ap;
    spool s;
    char *arg = NULL;

    if(p == NULL)
        return NULL;

    s = spool_new(p);

    va_start(ap, p);

    /* loop till we hit our end flag, the first arg */
    while(1)
    {
        arg = va_arg(ap,char *);
        if((pool_t)arg == p)
            break;
        else
            spool_add(s, arg);
    }

    va_end(ap);

    return spool_print(s);
}


char *strunescape(pool_t p, char *buf)
{
    int i,j=0;
    char *temp;

    if (buf == NULL) return(NULL);

    if (strchr(buf,'&') == NULL) return(buf);

    if(p != NULL)
        temp = pmalloc(p,strlen(buf)+1);
    else
        temp = malloc(strlen(buf)+1);

    if (temp == NULL) return(NULL);

    for(i=0;i<strlen(buf);i++)
    {
        if (buf[i]=='&')
        {
            if (strncmp(&buf[i],"&amp;",5)==0)
            {
                temp[j] = '&';
                i += 4;
            } else if (strncmp(&buf[i],"&quot;",6)==0) {
                temp[j] = '\"';
                i += 5;
            } else if (strncmp(&buf[i],"&apos;",6)==0) {
                temp[j] = '\'';
                i += 5;
            } else if (strncmp(&buf[i],"&lt;",4)==0) {
                temp[j] = '<';
                i += 3;
            } else if (strncmp(&buf[i],"&gt;",4)==0) {
                temp[j] = '>';
                i += 3;
            }
        } else {
            temp[j]=buf[i];
        }
        j++;
    }
    temp[j]='\0';
    return(temp);
}


char *strescape(pool_t p, char *buf, int len)
{
    int i,j,newlen = len;
    char *temp;

    if (buf == NULL || len < 0) return NULL;

    for(i=0;i<len;i++)
    {
        switch(buf[i])
        {
        case '&':
            newlen+=5;
            break;
        case '\'':
            newlen+=6;
            break;
        case '\"':
            newlen+=6;
            break;
        case '<':
            newlen+=4;
            break;
        case '>':
            newlen+=4;
            break;
        }
    }

    if(p != NULL)
        temp = pmalloc(p,newlen+1);
    else
        temp = malloc(newlen+1);
    if(newlen == len)
    {
        memcpy(temp,buf,len);
        temp[len] = '\0';
        return temp;
    }

    for(i=j=0;i<len;i++)
    {
        switch(buf[i])
        {
        case '&':
            memcpy(&temp[j],"&amp;",5);
            j += 5;
            break;
        case '\'':
            memcpy(&temp[j],"&apos;",6);
            j += 6;
            break;
        case '\"':
            memcpy(&temp[j],"&quot;",6);
            j += 6;
            break;
        case '<':
            memcpy(&temp[j],"&lt;",4);
            j += 4;
            break;
        case '>':
            memcpy(&temp[j],"&gt;",4);
            j += 4;
            break;
        default:
            temp[j++] = buf[i];
        }
    }
    temp[j] = '\0';
    return temp;
}

/** convenience (originally by Thomas Muldowney) */
void shahash_r(const char* str, char hashbuf[41]) {
    unsigned char hashval[20];

    shahash_raw(str, hashval);
    hex_from_raw((const char*)hashval, 20, hashbuf);
}
//#   include <openssl/sha.h>
void shahash_raw(const char* str, unsigned char hashval[20]) {
#ifdef HAVE_SSL
    /* use OpenSSL functions when available */
    SHA1((unsigned char *)str, strlen(str), hashval);
#else
    sha1_hash((unsigned char *)str, strlen(str), hashval);
#endif
}

int check_utf8(const unsigned char *str, size_t len){
	const unsigned char *p = str;
	const unsigned char *end = str + len;
	int expect = 0;
	for(p = str; p < end; ++p){
		if(expect){
			if((*p & 0xC0) == 0x80){
				--expect;
				continue;
			}else
				return 0;
		}
		if((*p & 0x80) == 0)
			continue;
		else if ((*p & 0xC0) == 0x80)
			return 0;
		else if ((*p & 0xE0) == 0xC0)
			expect = 1;
		else if ((*p & 0xF0) == 0xE0)
			expect = 2;
		else if ((*p & 0xF8) == 0xF0)
			expect = 3;
		else if ((*p & 0xFC) == 0xF8)
			expect = 4;
		else if ((*p & 0xFE) == 0xFC)
			expect = 5;
		else
			return 0;
	}
	if(expect)
		return 0;
	else
		return 1;
}
#ifndef _WIN32
#include <iconv.h>
char* j_transcode(const char* src, int length, const char* src_locale, const char* dst_locale){
	iconv_t ct = iconv_open(dst_locale, src_locale);
	if(length <= 0) length = strlen(src);
	char* dst = (char*) calloc(1, (length << 1) + 1);
	char* srcptr = (char*)src;
	char* dstptr = dst;
	size_t srclen = length;
	size_t dstlen = srclen << 1;
	iconv(ct, &srcptr, &srclen, &dstptr, &dstlen);
	iconv_close(ct);
	return dst;
}
#else
static wchar_t* j_to_wchar(const char* src, int length, unsigned int code_page){
    int num_chars;
    wchar_t* str_wchar = NULL;
    num_chars = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, src, -1, NULL, 0);
    if (num_chars > 0) {
		str_wchar = (wchar_t *)calloc(num_chars, sizeof(wchar_t) + 1);
		if (!str_wchar) return NULL;
		str_wchar[num_chars] = '\0';
		MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, src, -1, str_wchar, num_chars);
    }
    return str_wchar;
}
static char* j_to_mchar(const wchar_t* str_utf16, int str_len, unsigned int code_page){
	char* dest = NULL;
	int	length = WideCharToMultiByte(code_page, MB_ERR_INVALID_CHARS, str_utf16, str_len, NULL, 0, NULL, NULL);
	if(length > 0){
		dest = (char*)malloc(length + 1);
		if(!dest) return NULL;
		WideCharToMultiByte(code_page, MB_ERR_INVALID_CHARS, str_utf16, str_len, dest, length, NULL, NULL);
		dest[length] = '\0';
	}
	return dest;
}
static unsigned int _get_cp(const char* locale){
	if(0 == strcasecmp(locale, "utf-8"))
		return CP_UTF8;
	if(0 == strcasecmp(locale, "utf-7"))
		return CP_UTF7;
	if(0 == strcasecmp(locale, "gb2312") || 0 == strcasecmp(locale, "gbk"))
		return 936;
	if(0 == strcasecmp(locale, "gb18030"))
		return 54936;
	return 0;
}
char* j_transcode(const char* src, int length, const char* src_locale, const char* dst_locale){
	// only some locale is supported.
	int _len;
	char* dest;
	wchar_t *str_utf16;
	unsigned int src_cp = _get_cp(src_locale);
	unsigned int dest_cp = _get_cp(dst_locale);
	if(0 == src_cp || 0 == dest_cp)
		return NULL;
	str_utf16 = j_to_wchar(src, length,  src_cp);

	if(!str_utf16)return NULL;
	_len = wcslen(str_utf16);
	dest = j_to_mchar(str_utf16, _len, dest_cp);
	free(str_utf16);
	return dest;
}
#endif

////////// getopt ///////////

#ifdef _MSC_VER

int opterr = 1,        /* if error message should be printed */
    optind = 1,        /* index into parent argv vector */
    optopt,            /* character checked for validity */
    optreset;        /* reset getopt */
char    *optarg;        /* argument associated with option */

#define    BADCH    (int)' '
#define    BADARG    (int)':'
#define    EMSG    ""

/*
 * getopt --
 *    Parse argc/argv argument vector.
 */
int getopt(int argc, char * const argv[], const char *optstring)
{
    static char *place = EMSG;        /* option letter processing */
    char *oli;                /* option letter list index */

    if (optreset || *place == 0) {        /* update scanning pointer */
        optreset = 0;
        place = argv[optind];
        if (optind >= argc || *place++ != '-') {
            /* Argument is absent or is not an option */
            place = EMSG;
			          return (-1);
        }
        optopt = *place++;
        if (optopt == '-' && *place == 0) {
            /* "--" => end of options */
            ++optind;
            place = EMSG;
            return (-1);
        }
        if (optopt == 0) {
            /* Solitary '-', treat as a '-' option
               if the program (eg su) is looking for it. */
            place = EMSG;
            if (strchr(optstring, '-') == NULL)
                return -1;
            optopt = '-';
        }
    } else
        optopt = *place++;

    /* See if option letter is one the caller wanted */
    if (optopt == ':' || (oli = strchr(optstring, optopt)) == NULL) {
        if (*place == 0)
            ++optind;
        if (opterr && *optstring != ':')
            (void)fprintf(stderr,
                                      "unknown option -- %c\n", optopt);
        return (BADCH);
    }
	/* Does this option need an argument  */
    if (oli[1] != ':') {
        /* don't need argument */
        optarg = NULL;
        if (*place == 0)
            ++optind;
    } else {
        /* Option-argument is either the rest of this argument or the
           entire next argument. */
        if (*place)
            optarg = place;
        else if (argc > ++optind)
            optarg = argv[optind];
        else {
            /* option-argument absent */
            place = EMSG;
            if (*optstring == ':')
                return (BADARG);
            if (opterr)
                (void)fprintf(stderr,
                                        "option requires an argument -- %c\n",
                                        optopt);
            return (BADCH);
        }
        place = EMSG;
        ++optind;
    }
    return (optopt);            /* return option letter */
}
#endif
