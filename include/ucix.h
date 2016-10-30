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
#include <time.h>

#ifndef _UCI_H__
#define _UCI_H__
struct uci_context* ucix_init(const char *config_file);
struct uci_context* ucix_init_path(const char *path, const char *config_file);

/* value/name tuples */
struct value_tuple {
	char value[16];
	time_t value_time;
	int Out_Of_Service;
	char idx[18];
	struct value_tuple *next;
};

/* structure to hold tuple-list and uci context during iteration */
struct uci_itr_ctx {
	struct value_tuple *list;
	struct uci_context *ctx;
	char *section;
};
typedef struct value_tuple value_tuple_t;

void ucix_cleanup(struct uci_context *ctx);
void ucix_save(struct uci_context *ctx);
void ucix_save_state(struct uci_context *ctx);
const char* ucix_get_option(struct uci_context *ctx,
	const char *p, const char *s, const char *o);
int ucix_get_list(char *value[254], struct uci_context *ctx,
	const char *p, const char *s, const char *o);
int ucix_get_option_int(struct uci_context *ctx,
	const char *p, const char *s, const char *o, int def);
void ucix_add_section(struct uci_context *ctx,
	const char *p, const char *s, const char *t);
void ucix_add_option_int(struct uci_context *ctx,
	const char *p, const char *s, const char *o, int t);
void ucix_add_option(struct uci_context *ctx,
	const char *p, const char *s, const char *o, const char *t);
void ucix_set_list(struct uci_context *ctx,
	const char *p, const char *s, const char *o, char value[254][64], int l);
int ucix_commit(struct uci_context *ctx, const char *p);
void ucix_revert(struct uci_context *ctx,
	const char *p, const char *s, const char *o);
void ucix_del(struct uci_context *ctx, const char *p,
	const char *s, const char *o);
bool ucix_string_copy(char *dest, size_t i, char *src);
void ucix_for_each_section_type(struct uci_context *ctx,
	const char *p, const char *t,
	void (*cb)(const char*, void*), void *priv);
/* Check if given uci file was updated */
time_t check_uci_update(const char *config, time_t mtime);
/* Add tuple */
void load_value(const char *sec_idx, struct uci_itr_ctx *itr);
#endif
