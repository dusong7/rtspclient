/*
 * config.c
 *
 *  Created on: 2014年2月17日
 *      Author: zhen.wang
 */

#include <stdlib.h>
#include <stdio.h>
#include "util/config.h"

static char *ltrim(char* src){
	while(*src != 0 && *src <= ' '){
		++src;
	}
	return src;
}

static char* rtrim(char* src){
	register int n = strlen(src) - 1;
	while(n >= 0 && src[n] <= ' '){
		--n;
	}
	src[n + 1] = '\0';
	return src;
}
/**
 * @note You can call it as many times as you need, no worry about memory arising or leasing.
 * @param conf
 * @param item
 * @return
 */
const char* config_get_one(xht conf, const char* item)
{
	return (const char*) xhash_get(conf, item);
}
/**
 * @note You should call it no more than once for each item, else you may got memory Arising.
 * @param conf
 * @param item
 * @return
 */
const char* config_get_str(xht conf, const char* item)
{
	char str[4096] = "";
	int n_str = 0;
	int n = strlen(item);
	if(xhash_iter_first(conf)){
		do
		{
			const char *key;
			int keylen;
			const char *val;
			xhash_iter_get(conf, &key, &keylen, (void**)&val);
			if(keylen - n > 1 && strncmp(key, item, n) == 0 && key[n] == '.'){
				n_str += snprintf(str + n_str, sizeof(str) - n_str, "%.*s=%s&", keylen-n-1, key+n+1, val);
			}
		}while(xhash_iter_next(conf));
	}
	return pstrdup(xhash_pool(conf), str);
}
/**
 * @note You should call it no more than once for each item, else you may got memory Arising.
 *       You should not free it in any case.
 * @param conf
 * @param item
 * @return
 */
xht config_get(xht conf, const char* item)
{
	xht ret = xhash_new(53);
	int n = strlen(item);
	pool_cleanup(xhash_pool(conf), (void (*)(void*))xhash_free, ret);

	if(xhash_iter_first(conf)){
		do
		{
			const char *key;
			int keylen;
			const char *val;
			xhash_iter_get(conf, &key, &keylen, (void**)&val);
			if(keylen - n > 1 && strncmp(key, item, n) == 0 && key[n] == '.'){
				xhash_putx(ret, key+n+1, keylen-n-1, (void*)val);
			}
		}while(xhash_iter_next(conf));
	}
	return ret;
}
/**
 * @note You need free it by yourself after usage by calling of config_free or xhash_free.
 * @param param
 * @return
 */
xht config_parse_str(const char* param)
{
	xht ret = xhash_new(53);
	char *tmp = pstrdup(xhash_pool(ret), param);
	char *saved_ptr = NULL;
	char *p = strtok_r(tmp, "&", &saved_ptr);
	while(p){
		char *q = strchr(p, '=');
		if(q){
			*q = 0;
			++q;
			xhash_put(ret, p, q);
		}
		p = strtok_r(NULL, "&", &saved_ptr);
	}
	return ret;
}

xht config_load(const char* conf_file, const char* segment)
{
	xht conf = xhash_new(101);
	char line[102400];
	int segment_wanted = 1;
	FILE *f = fopen(conf_file, "r");
	char *p;
	// generate default conf begin
	xhash_put(conf, "recorder.engine", (void*)"localfile");
	xhash_put(conf, "recorder.engine.file.prefix", (void*)"/vms/record");
	xhash_put(conf, "recorder.engine.hdfs.prefix", (void*)"/vms/record");
	xhash_put(conf, "recorder.engine.hdfs.nn_addr", (void*)"localhost");
	xhash_put(conf, "recorder.engine.hdfs.nn_port", (void*)"8020");
	// default conf end
	if(!f){
		return conf;
	}
	while(fgets(line, sizeof(line), f))
	{
		if(line[0] == '#') {
			// ignore comment line in config file
			continue;
		}
		if(line[0] == '[' && strchr(line, ']'))
		{
			int n_seg = strchr(line, ']') - line - 1;
			// segment string
			if(line[1] == '_')
				segment_wanted = 1;// each segment named _ beginning is always loaded
			else if(!segment ||
					(n_seg == strlen(segment) && strncmp(&line[1], segment, n_seg) == 0))
				segment_wanted = 1;
			else
				segment_wanted = 0;
		}
		if(!segment_wanted)
			continue;
		p = strchr(line, '=');
		if(!p)
			continue;
		if(p){
			*p = 0;
			rtrim(line);
			p = ltrim(p + 1);
			rtrim(p);
		}
		if(!line[0] || !p[0])
			continue; // line without key must be ignored.
		xhash_put(conf, pstrdup(xhash_pool(conf), line),
				  pstrdup(xhash_pool(conf), p));
	}

	fclose(f);
	return conf;
}

void config_free(xht conf)
{
	xhash_free(conf);
}

#ifdef _WIN32
void setenv(const char* key, const char* val, int replace)
{
	char *str = malloc(strlen(val) + strlen(key) + 2);
	sprintf(str, "%s=%s", key, val);
	putenv(str);
}
#endif

int config_prepare_env(xht conf)
{
	int ret = 0;
	char* env_str;
	char *key, *val;
	char *saved;
	if(getenv("ENV_Prepared"))
		return 0;
	env_str = (char*)config_get_str(conf, "env");
	if(!env_str)
		return 0;
	env_str = strdup(env_str);
	key = strtok_r(env_str, "&", &saved);
	while(key){
		const char* env;
		val = strchr(key, '=');
		if(!val)
			break;
		*val = 0;
		++val;
		env = getenv(key);
		if(!env || strcmp(val, env) != 0){
			fprintf(stderr, "set env %s=%s\n", key, val);
			setenv(key, val, 1);
			if(strcmp(key, "LD_LIBRARY_PATH") == 0)
				ret = 1;
		}
		key = strtok_r(NULL, "&", &saved);
	}

	free(env_str);
	if(ret){
		setenv("ENV_Prepared", "1", 1);
	}
	return ret;
}

#ifdef _MSC_VER
#include <io.h> 
#define ftruncate _chsize 
#endif

int config_save_item(const char* config_file, const char* item, int itemlen, const char* value)
{
	FILE* fp = fopen(config_file, "rb+");
	int length;
	char* content, *p, *line;

	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	content = (char*)malloc(length);
	rewind(fp);
	fread(content, 1, length, fp);
	rewind(fp);
	p = strchr(content, '\n');
	line = content;
	while(p){
		if(line < p && 0 == strncmp(line, item, itemlen) && (line[itemlen] == '=' || line[itemlen] == ' ')){
			// got the item. change it.
			int n, total;
			fseek(fp, (line - content), SEEK_SET);
			n = fprintf(fp, "%.*s = %s%s", itemlen, item, value, p[-1] == '\r' ? "\r":"");
			// write all following data.
			fwrite(p, 1, length - (p - content), fp);
			total = length + n - (p - line);
			if(total < length)
				ftruncate(fileno(fp), total);
			break;
		}
		line = ltrim(p);
		// find next line.
		p = strchr(line, '\n');
	}
	fclose(fp);
	return 0;
}
