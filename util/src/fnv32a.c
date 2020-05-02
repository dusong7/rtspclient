/*
 * fnv32.c
 *
 *  Created on: 2014年1月8日
 *      Author: lily
 */

/* jabberd2 Windows DLL */
#  define JABBERD2_API
#ifndef JABBERD2_API
# ifdef _WIN32
#  ifdef JABBERD2_EXPORTS
#   define JABBERD2_API  __declspec(dllexport)
#  else /* JABBERD2_EXPORTS */
#   define JABBERD2_API  __declspec(dllimport)
#  endif /* JABBERD2_EXPORTS */
# else /* _WIN32 */
#  define JABBERD2_API
# endif /* _WIN32 */
#endif /* JABBERD2_API */

JABBERD2_API unsigned int j_fnv_32a_str_c(const char *str, int len)
{
    unsigned char *s = (unsigned char *)str;    /* unsigned string */
    unsigned int hval= 2166136261U;
    /*
     * FNV-1a hash each octet in the buffer
     */
    do {
		/* xor the bottom with the current octet */
		hval ^= (unsigned int)*s;

		/* multiply by the 32 bit FNV magic prime mod 2^32 */
		//hval *= 0x01000193;  /* the same as following */
		hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
		++s;
    }while(--len > 0);
    return hval;
}
