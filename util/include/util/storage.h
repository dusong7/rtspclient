/*
 * storage.h
 *
 *  Created on: 2011-9-15
 *      Author: wangzhen02
 */

#ifndef STORAGE_H_
#define STORAGE_H_

#ifndef JABBERD2_API
#define JABBERD2_API extern
#endif /* JABBERD2_API */

#ifdef __cplusplus
extern "C" {
#endif

/* object sets */

/** object types */
typedef enum {
    os_type_BOOLEAN,            /**< boolean (0 or 1) */
    os_type_INTEGER,            /**< integer */
    os_type_STRING,             /**< string */
    os_type_NAD,                /**< XML */
    os_type_UNKNOWN,            /**< unknown */
    os_type_BIGINT              /**< 64 bit integer */
} os_type_t;

/** a single tuple (value) within an object */
typedef struct os_field_st {
    char        *key;           /**< field name */
    void        *val;           /**< field value */
    os_type_t   type;           /**< field type */
} *os_field_t;

typedef struct os_st        *os_t;
typedef struct os_object_st *os_object_t;

/** object set (ie group of several objects) */
struct os_st {
    pool_t      p;              /**< pool the objects are allocated from */

    os_object_t head;           /**< first object in the list */
    os_object_t tail;           /**< last object in the list */

    int         count;          /**< number of objects in this set */

    os_object_t iter;           /**< pointer for iteration */
};

/** an object */
struct os_object_st {
    /** object set this object is part of */
    os_t        os;

    /** fields (key is field name) */
    xht         hash;

    os_object_t next;           /**< next object in the list */
    os_object_t prev;           /**< previous object in the list */
};

/** create a new object set */
JABBERD2_API os_t        os_new(void);
/** free an object set */
JABBERD2_API void        os_free(os_t os);

/** number of objects in a set */
JABBERD2_API int         os_count(os_t os);

/** set iterator to first object (1 = exists, 0 = doesn't exist) */
JABBERD2_API int         os_iter_first(os_t os);

/** set iterator to next object (1 = exists, 0 = doesn't exist) */
JABBERD2_API int         os_iter_next(os_t os);

/** get the object currently under the iterator */
JABBERD2_API os_object_t os_iter_object(os_t os);

/** create a new object in this set */
JABBERD2_API os_object_t os_object_new(os_t os);
/** free an object (remove it from its set) */
JABBERD2_API void        os_object_free(os_object_t o);

/** add a field to the object */
JABBERD2_API void        os_object_put(os_object_t o, const char *key, const void *val, os_type_t type);

/** get a field from the object of type type (result in val), ret 0 == not found */
JABBERD2_API int         os_object_get(os_t os, os_object_t o, const char *key, void **val, os_type_t type, os_type_t *ot);

/** wrappers for os_object_get to avoid breaking strict-aliasing rules in gcc3 */
JABBERD2_API int         os_object_get_nad(os_t os, os_object_t o, const char *key, nad_t *val);
JABBERD2_API int         os_object_get_str(os_t os, os_object_t o, const char *key, char **val);
JABBERD2_API int         os_object_get_int(os_t os, os_object_t o, const char *key, int *val);
JABBERD2_API int         os_object_get_int64(os_t os, os_object_t o, const char *key, int64_t *val);
JABBERD2_API int         os_object_get_bool(os_t os, os_object_t o, const char *key, int *val);
JABBERD2_API int         os_object_get_time(os_t os, os_object_t o, const char *key, time_t *val);

/** wrappers for os_object_put to avoid breaking strict-aliasing rules in gcc3 */
JABBERD2_API void        os_object_put_time(os_object_t o, const char *key, const time_t *val);

/** set field iterator to first field (1 = exists, 0 = doesn't exist) */
JABBERD2_API int         os_object_iter_first(os_object_t o);
/** set field iterator to next field (1 = exists, 0 = doesn't exist) */
JABBERD2_API int         os_object_iter_next(os_object_t o);
/** extract field values from field currently under the iterator */
JABBERD2_API void        os_object_iter_get(os_object_t o, char **key, void **val, os_type_t *type);


/* storage manager */

/** storage driver return values */
typedef enum {
    st_SUCCESS,                 /**< call completed successful */
    st_FAILED,                  /**< call failed (driver internal error) */
    st_NOTFOUND,                /**< no matching objects were found */
    st_NOTIMPL,                 /**< call not implemented */
    st_ASYNC,                   /**< calling is working in async mode */
	st_TIMEOUT
} st_ret_t;

typedef struct storage_st   *storage_t;
typedef struct st_driver_st *st_driver_t;

/** storage manager data */
struct storage_st {
	/* sm_t        sm; */		/**< sm context */
	//void		*parent;
	config_t	config;
	log_t		log;

    xht         drivers;        /**< pointers to drivers (key is driver name) */
    xht         types;          /**< pointers to drivers (key is type name) */

    st_driver_t default_drv;    /**< default driver (used when there is no module
                                     explicitly registered for a type) */
};

typedef struct st_context_st {
	st_ret_t ret;
	os_t os;
	int count;
}st_context, *st_context_t;

/** data for a single storage driver */
struct st_driver_st {
    storage_t   st;             /**< storage manager context */

    char        *name;          /**< name of driver */

    void        *_private;       /**< driver private data */

    /** called to find out if this driver can handle a particular type */
    st_ret_t    (*add_type)(st_driver_t drv, const char *type);

    /** put handler */
    st_ret_t    (*put)(st_driver_t drv, const char *type, const char *owner, os_t os);
    /** get handler */
    st_ret_t    (*get)(st_driver_t drv, const char *type, const char *owner, const char *filter, os_t *os);
    /** count handler */
    st_ret_t    (*count)(st_driver_t drv, const char *type, const char *owner, const char *filter, int *count);
    /** delete handler */
    st_ret_t    (*_delete)(st_driver_t drv, const char *type, const char *owner, const char *filter);
    /** replace handler */
    st_ret_t    (*replace)(st_driver_t drv, const char *type, const char *owner, const char *filter, os_t os);

    /** called when driver is freed */
    void        (*free)(st_driver_t drv);
    /** async version for put handler */
    st_ret_t    (*put_async)(st_driver_t drv, const char *type, const char *owner, os_t os, st_context_t st_ct, void (*cb)(void*), void *arg);
    /** async version for get handler */
    st_ret_t    (*get_async)(st_driver_t drv, const char *type, const char *owner, const char *filter, st_context_t st_ct, void (*cb)(void*), void *arg);
    /** async version for count handler */
    st_ret_t    (*count_async)(st_driver_t drv, const char *type, const char *owner, const char *filter, st_context_t st_ct, void (*cb)(void*), void *arg);
    /** async version for delete handler */
    st_ret_t    (*delete_async)(st_driver_t drv, const char *type, const char *owner, const char *filter, st_context_t st_ct, void (*cb)(void*), void *arg);
    /** async version for replace handler (replace = delete + put) */
    st_ret_t    (*replace_async)(st_driver_t drv, const char *type, const char *owner, const char *filter, os_t os, st_context_t st_ct, void (*cb)(void*), void *arg);
    /** called when driver is reloaded */
    st_ret_t    (*reload)(st_driver_t drv,config_t config);

    /** redis command  */
    st_ret_t    (*sadd)(st_driver_t st,const char* type,const char* owner,os_t os);
    st_ret_t    (*srem)(st_driver_t st,const char* type,const char* owner,os_t os);
    st_ret_t    (*smembers)(st_driver_t st,const char* type,const char* owner,os_t *os);

    /** update handler, only async version is implemented yet. */
    st_ret_t    (*update)(st_driver_t st, const char *type, const char *owner, const char *filter, os_t os);
    st_ret_t    (*update_async)(st_driver_t drv, const char *type, const char *owner, const char *filter, os_t os, st_context_t st_ct, void (*cb)(void*), void *arg);

    /** generic commands, only async version is implemented yet.  */
    st_ret_t    (*queryraw)(st_driver_t st, const char *type, const char *owner, const char *query, os_t *os);
    st_ret_t    (*queryraw_async)(st_driver_t drv, const char *type, const char *owner, const char *query, st_context_t st_ct, void (*cb)(void*), void *arg);
};

/** allocate a storage manager instance */
JABBERD2_API storage_t       storage_new(config_t config, log_t log);
/** free a storage manager instance */
JABBERD2_API void            storage_free(storage_t st);

/** associate this data type with this driver */
JABBERD2_API st_ret_t        storage_add_type(storage_t st, const char *driver, const char *type);

/** store objects in this set */
JABBERD2_API st_ret_t        storage_put(storage_t st, const char *type, const char *owner, os_t os);
/** get objects matching this filter */
JABBERD2_API st_ret_t        storage_get(storage_t st, const char *type, const char *owner, const char *filter, os_t *os);
/** count objects matching this filter */
JABBERD2_API st_ret_t        storage_count(storage_t st, const char *type, const char *owner, const char *filter, int *count);
/** delete objects matching this filter */
JABBERD2_API st_ret_t        storage_delete(storage_t st, const char *type, const char *owner, const char *filter);
/** replace objects matching this filter with objects in this set (atomic delete + get) */
JABBERD2_API st_ret_t        storage_replace(storage_t st, const char *type, const char *owner, const char *filter, os_t os);

JABBERD2_API st_ret_t        storage_put_async(storage_t st, const char *type, const char *owner, os_t os, st_context_t st_ct, void (*cb)(void*), void *arg);
JABBERD2_API st_ret_t        storage_get_async(storage_t st, const char *type, const char *owner, const char *filter, st_context_t st_ct, void (*cb)(void*), void *arg);
JABBERD2_API st_ret_t        storage_count_async(storage_t st, const char *type, const char *owner, const char *filter, st_context_t st_ct, void (*cb)(void*), void *arg);
JABBERD2_API st_ret_t        storage_delete_async(storage_t st, const char *type, const char *owner, const char *filter, st_context_t st_ct, void (*cb)(void*), void *arg);
JABBERD2_API st_ret_t        storage_replace_async(storage_t st, const char *type, const char *owner, const char *filter, os_t os, st_context_t st_ct, void (*cb)(void*), void *arg);
JABBERD2_API void        	 storage_reload(storage_t st,config_t config);
JABBERD2_API st_ret_t        storage_sadd(storage_t st,const char* type,const char* owner,os_t os);
JABBERD2_API st_ret_t        storage_srem(storage_t st,const char* type,const char* owner,os_t os);
JABBERD2_API st_ret_t        storage_smembers(storage_t st,const char* type,const char* owner,os_t *os);
//JABBERD2_API st_ret_t        storage_queryraw(storage_t st, const char* type, const char *owner, const char* query, os_t *os);
//JABBERD2_API st_ret_t        storage_update(storage_t st, const char *type, const char *owner, const char *filter, os_t os);
JABBERD2_API st_ret_t        storage_queryraw_async(storage_t st, const char* type, const char *owner, const char* query, st_context_t st_ct, void (*cb)(void*), void *arg);
JABBERD2_API st_ret_t        storage_update_async(storage_t st, const char *type, const char *owner, const char *filter, os_t os, st_context_t st_ct, void (*cb)(void*), void *arg);

/** type for the driver init function */
typedef st_ret_t (*st_driver_init_fn)(st_driver_t);

/** storage filter types */
typedef enum {
    st_filter_type_PAIR,        /**< key=value pair */
    st_filter_type_AND,         /**< and operator */
    st_filter_type_OR,          /**< or operator */
    st_filter_type_NOT          /**< not operator */
} st_filter_type_t;

typedef struct st_filter_st *st_filter_t;
/** filter abstraction */
struct st_filter_st {
    pool_t              p;      /**< pool that filter is allocated from */

    st_filter_type_t    type;   /**< type of this filter */

    char                *key;   /**< key for PAIR filters */
    char                *val;   /**< value for PAIR filters */

    st_filter_t         sub;    /**< sub-filter for operator filters */

    st_filter_t         next;   /**< next filter in a group */
};

/** create a filter abstraction from a LDAP-like filter string */
JABBERD2_API st_filter_t     storage_filter(const char *filter);

/** see if the object matches the filter */
JABBERD2_API int             storage_match(st_filter_t filter, os_object_t o, os_t os);
JABBERD2_API os_t storage_filter_os(os_t os,st_filter_t f);
JABBERD2_API os_t storage_filterout_os(os_t os,st_filter_t f);
JABBERD2_API void os_copy_object(os_t os,os_object_t o);
JABBERD2_API void os_append(os_t os1,os_t os2);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H_ */
