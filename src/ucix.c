/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *   Copyright (C) 2008 John Crispin <blogic@openwrt.org> 
 */

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include <uci_config.h>
#include <uci.h>
#include "ucix.h"
/*#include "log.h" */

static struct uci_ptr ptr;

static inline int ucix_get_ptr(struct uci_context *ctx, const char *p, const char *s, const char *o, const char *t)
{
	memset(&ptr, 0, sizeof(ptr));
	ptr.package = p;
	ptr.section = s;
	ptr.option = o;
	ptr.value = t;
	return uci_lookup_ptr(ctx, &ptr, NULL, true);
}

struct uci_context* ucix_init(const char *config_file)
{
	struct uci_context *ctx = uci_alloc_context();
	uci_add_delta_path(ctx, "/var/state");
	if(uci_load(ctx, config_file, NULL) != UCI_OK)
	{
		fprintf(stderr, "%s/%s is missing or corrupt\n", ctx->savedir, config_file);
		return NULL;
	}
	return ctx;
}

struct uci_context* ucix_init_path(const char *path, const char *config_file)
{
	struct uci_context *ctx = uci_alloc_context();
	if(path)
		uci_set_confdir(ctx, path);
	if(uci_load(ctx, config_file, NULL) != UCI_OK)
	{
		fprintf(stderr, "%s/%s is missing or corrupt\n", ctx->savedir, config_file);
		return NULL;
	}
	return ctx;
}

void ucix_cleanup(struct uci_context *ctx)
{
	uci_free_context(ctx);
}

void ucix_save(struct uci_context *ctx)
{
	uci_set_savedir(ctx, "/tmp/.uci/");
	uci_save(ctx, NULL);
}

void ucix_save_state(struct uci_context *ctx)
{
	uci_set_savedir(ctx, "/var/state/");
	uci_save(ctx, NULL);
}

const char* ucix_get_option(struct uci_context *ctx, const char *p, const char *s, const char *o)
{
	struct uci_element *e = NULL;
	const char *value = NULL;
	if(ucix_get_ptr(ctx, p, s, o, NULL))
		return NULL;
	if (!(ptr.flags & UCI_LOOKUP_COMPLETE))
		return NULL;
	e = ptr.last;
	switch (e->type)
	{
	case UCI_TYPE_SECTION:
		value = uci_to_section(e)->type;
		break;
	case UCI_TYPE_OPTION:
		switch(ptr.o->type) {
			case UCI_TYPE_STRING:
				value = ptr.o->v.string;
				break;
			default:
				value = NULL;
				break;
		}
		break;
	default:
		value = NULL;
		break;
	}

	return value;
}

int ucix_get_list(char *value[254], struct uci_context *ctx, const char *p, const char *s, const char *o)
{
	struct uci_element *e = NULL;
	int n;
	if(ucix_get_ptr(ctx, p, s, o, NULL))
		return 0;
	if (!(ptr.flags & UCI_LOOKUP_COMPLETE))
		return 0;
	e = ptr.last;
	switch (e->type)
	{
	case UCI_TYPE_OPTION:
		switch(ptr.o->type) {
			case UCI_TYPE_LIST:
				n = 0;
                uci_foreach_element(&ptr.o->v.list, e) {
                      value[n] = e->name;
                      n++;
                }
				break;
			default:
			    n = 0;
				break;
		}
		break;
	default:
		return 0;
	}

	return n;
}

int ucix_get_option_int(struct uci_context *ctx, const char *p, const char *s, const char *o, int def)
{
	const char *tmp = ucix_get_option(ctx, p, s, o);
	int ret = def;

	if (tmp)
		ret = atoi(tmp);
	return ret;
}

void ucix_add_section(struct uci_context *ctx, const char *p, const char *s,
    const char *t)
{
	if(ucix_get_ptr(ctx, p, s, NULL, t))
		return;
	uci_set(ctx, &ptr);
}

void ucix_add_option_int(struct uci_context *ctx, const char *p, const char *s,
    const char *o, int t)
{
	char tmp[64];
	snprintf(tmp, 64, "%d", t);
	ucix_add_option(ctx, p, s, o, tmp);
}

void ucix_add_option(struct uci_context *ctx, const char *p, const char *s,
    const char *o, const char *t)
{
	if(ucix_get_ptr(ctx, p, s, o, (t)?(t):("")))
		return;
	uci_set(ctx, &ptr);
}

void ucix_set_list(struct uci_context *ctx, const char *p, const char *s, const char *o, char value[254][64], int l)
{
    int i;
    ucix_get_ptr(ctx, p, s, o, NULL);
    uci_delete(ctx, &ptr);
    ucix_save(ctx);
    for (i = 0; i < l; i++) {
        if (value[i]) {
            ucix_get_ptr(ctx, p, s, o, value[i]);
            uci_add_list(ctx, &ptr);
        } else {
            ucix_get_ptr(ctx, p, s, o, NULL);
            uci_add_list(ctx, &ptr);
        }
    }
}

void ucix_del(struct uci_context *ctx, const char *p, const char *s, const char *o)
{
	if(!ucix_get_ptr(ctx, p, s, o, NULL))
		uci_delete(ctx, &ptr);
}

void ucix_revert(struct uci_context *ctx, const char *p, const char *s, const char *o)
{
	if(!ucix_get_ptr(ctx, p, s, o, NULL))
		uci_revert(ctx, &ptr);
}

void ucix_for_each_section_type(struct uci_context *ctx,
	const char *p, const char *t,
	void (*cb)(const char*, void*), void *priv)
{
	struct uci_element *e;
	if(ucix_get_ptr(ctx, p, NULL, NULL, NULL))
		return;
	uci_foreach_element(&ptr.p->sections, e)
		if (!strcmp(t, uci_to_section(e)->type))
			cb(e->name, priv);
}

int ucix_commit(struct uci_context *ctx, const char *p)
{
	if(ucix_get_ptr(ctx, p, NULL, NULL, NULL))
		return 1;
	return uci_commit(ctx, &ptr.p, false);
}

bool ucix_string_copy(char *dest, size_t j, char *src)
{
    size_t i;   /* counter */
    if (src != 0) {
        for (i = 0; i < j; i++) {
            if (src[i]) {
                dest[i] = src[i];
            } else {
                dest[i] = 0;
            }
        }
        return true;
    }
    return false;
}

/* Check if given uci file was updated */
time_t check_uci_update(const char *config, time_t mtime)
{
	struct stat s;
	char path[128];
	time_t f_mtime = 0;

	snprintf(path, sizeof(path), "/var/state/%s", config);
	if( stat(path, &s) > -1 )
		f_mtime = s.st_mtime;

	snprintf(path, sizeof(path), "/etc/config/%s", config);
	if( stat(path, &s) > -1 ) {
		if( (f_mtime == 0) || (s.st_mtime > f_mtime) ) {
			f_mtime = s.st_mtime;
		}
	}

/*	snprintf(path, sizeof(path), "/tmp/.uci/%s", config);
	if( stat(path, &s) > -1 ) {
		if( (f_mtime == 0) || (s.st_mtime > f_mtime) ) {
			f_mtime = s.st_mtime;
		}
	} */
	if( (mtime == 0) || (f_mtime > mtime) ) {
		return f_mtime;
	} else {
		f_mtime = 0;
	}
	return f_mtime;
}

/* Add tuple */
void load_value(const char *sec_idx, struct uci_itr_ctx *itr)
{
	value_tuple_t *t;

	if (!strcmp(sec_idx, "default"))
		return;

	const char *value;
	time_t value_time;
	int Out_Of_Service;
	value = ucix_get_option(itr->ctx, itr->section, sec_idx, "value");
	value_time = ucix_get_option_int(itr->ctx, itr->section, sec_idx,
		"value_time",0);
	Out_Of_Service = ucix_get_option_int(itr->ctx, itr->section, sec_idx,
		"Out_Of_Service",1);
	if( (t = (value_tuple_t *)malloc(sizeof(value_tuple_t))) != NULL ) {
		strncpy(t->idx, sec_idx, sizeof(t->idx));
		if ( value != NULL ) {
			strncpy(t->value, value, sizeof(t->value));
		}
		t->value_time=value_time;
		t->Out_Of_Service=Out_Of_Service;
		t->next = itr->list;
		itr->list = t;
	}
}


