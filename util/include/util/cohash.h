/*
 * cohash.h
 *
 *  Created on: 2013-4-6
 *      Author: wangzhen02
 */

#ifndef COHASH_H_
#define COHASH_H_

#include "util/xhash.h"

typedef struct jcohash_node_st{
	char		*id;
	int			disabled;
}*jcohash_node_t;

typedef struct j_hashed_node
{
	unsigned int hash_val;
	int target_index;
}*j_node_t;

typedef struct j_hash_circle_st
{
	int count;
	struct j_hashed_node nodes[];
}*j_circle_t;

typedef struct jcohash_st{
	char                *name;
	int			        rtype;
	jcohash_node_t       *comp;
	int                 ncomp;
	xht                 hashes;
	j_circle_t          circle;
	time_t              re_hash;
	int                 delay;
	int                 cohash_policy;
	int                 cohash_group;
}*jcohash_t;

enum {hash_COHASH = 0x80};

#ifdef __cplusplus
extern "C" {
#endif

JABBERD2_API int jcohash_get_target(jcohash_t cohash, unsigned int key);
JABBERD2_API void jcohash_build(jcohash_t cohash);
JABBERD2_API void jcohash_clean(jcohash_t cohash);
JABBERD2_API void jcohash_initialize(jcohash_t cohash, const char* name, int policy, int group);
JABBERD2_API jcohash_t jcohash_create(const char* name, int policy, int group);
JABBERD2_API void jcohash_free(jcohash_t cohash);
JABBERD2_API int jcohash_add_node(jcohash_t cohash, jcohash_node_t comp);
JABBERD2_API int jcohash_remove_node(jcohash_t cohash, jcohash_node_t comp);
JABBERD2_API unsigned int j_fnv_32a_str_c(const char *str, int len);

#ifdef __cplusplus
}
#endif
#endif /* COHASH_H_ */
