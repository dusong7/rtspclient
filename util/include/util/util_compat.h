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


#ifndef INCL_UTIL_COMPAT_H
#define INCL_UTIL_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file util/util_compat.h
 * @brief define the structures that could be missing in old libc implementations
 */

#ifndef PF_INET6
# define PF_INET6    10		/**< protcol family for IPv6 */
#endif

#ifndef AF_INET6
# define AF_INET6    PF_INET6	/**< address family for IPv6 */
#endif

#ifndef INET6_ADDRSTRLEN
# define INET6_ADDRSTRLEN 46	/**< maximum length of the string
				  representation of an IPv6 address */
#endif


#ifndef IN6_IS_ADDR_V4MAPPED
    /** check if an IPv6 is just a mapped IPv4 address */
#define IN6_IS_ADDR_V4MAPPED(a) \
    ((*(const uint32_t *)(const void *)(&(a)->s6_addr[0]) == 0) && \
    (*(const uint32_t *)(const void *)(&(a)->s6_addr[4]) == 0) && \
    (*(const uint32_t *)(const void *)(&(a)->s6_addr[8]) == ntohl(0x0000ffff)))
#endif

#ifndef HAVE_SA_FAMILY_T
typedef unsigned short  sa_family_t;
#endif


#ifndef SSL_OP_NO_TICKET 
#define SSL_OP_NO_TICKET		0x00004000L
#endif

#ifdef __cplusplus
}
#endif

#endif    /* INCL_UTIL_H */
