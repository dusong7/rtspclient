/*
 * config.h
 *
 *  Created on: 2014年2月17日
 *      Author: lily
 */

#ifndef UTIL_CONFIG_H_
#define UTIL_CONFIG_H_

#include <util/util.h>

#ifdef __cplusplus
extern "C"{
#endif

JABBERD2_API const char* config_get_one(xht conf, const char* item);
JABBERD2_API const char* config_get_str(xht conf, const char* item);
JABBERD2_API xht 		config_get(xht conf, const char* item);
JABBERD2_API xht 		config_parse_str(const char* param);

JABBERD2_API xht		config_load(const char* conf_file, const char* segment);
JABBERD2_API void		config_free(xht conf);
JABBERD2_API int		config_prepare_env(xht conf);
JABBERD2_API int 		config_save_item(const char* config_file, const char* item, int itemlen, const char* value);

#ifdef __cplusplus
}
#endif

#include <sys/stat.h>

#define PREPARE_FILE_CHECKING(name, path) \
		static struct stat file_stat_##name; \
		static const char* file_path_##name;\
		if(!file_path_##name) file_path_##name = path;

/** provide this macro for file changing detecting, the first time
 * */
#define IF_FILE_CHANGED_SINCE(name) \
		struct stat cur_stat_##name; \
		if(0 == stat(file_path_##name, &cur_stat_##name) && \
			   (((file_stat_##name.st_ino != 0 || file_stat_##name.st_size != 0) && \
				 (file_stat_##name.st_ctime != cur_stat_##name.st_ctime || \
				  file_stat_##name.st_mtime != cur_stat_##name.st_mtime || \
				  file_stat_##name.st_size != cur_stat_##name.st_size || \
				  file_stat_##name.st_ino != cur_stat_##name.st_ino) && \
				 (file_stat_##name = cur_stat_##name, 1)) || \
			    (file_stat_##name = cur_stat_##name, 0)))

#define IF_FILE_CHANGED(name) \
		struct stat cur_stat_##name; \
		if(0 == stat(file_path_##name, &cur_stat_##name) && \
				(file_stat_##name.st_ctime != cur_stat_##name.st_ctime || \
				 file_stat_##name.st_mtime != cur_stat_##name.st_mtime || \
				 file_stat_##name.st_size != cur_stat_##name.st_size || \
				 file_stat_##name.st_ino != cur_stat_##name.st_ino) && \
				(file_stat_##name = cur_stat_##name, 1))

#endif /* UTIL_CONFIG_H_ */
