/*
 * Copyright (c) 2015 Devoleda Organisation
 * Copyright (c) 2015 Ethan Levy <eitanlevy97@yandex.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <shared/message.h>
#include <shared/list.h>
#include <sysdb/internal.h>
#include <sysdb/resolver.h>

static void push_queue(tpsl_list_t *queue, DNA_struct *dna, char *cell)
{
	queue_item *item = NULL;

	if(queue) {
		if(dna) {
			if(!tpsl_list_find_str(queue, dna->name)) {
				item = calloc(1, sizeof(queue_item));
				strcpy(item->name, dna->name);
				memcpy(queue->version, dna->version);
				strcpy(item->conflict, dna->conflict);
				tpsl_list_add_node(queue, item);
			}
		} else if(cell && strlen(cell) > 7) {
			DNA_struct *p = NULL;
			if(!repodb_query_cell_by_name(cell, &p)) {
				push_queue(queue, p, NULL); // otherwise just ignore it
			} else
				pushdeb("Unknown Cell in repo: %s, skipped.\n", cell);
		}
	}
}

static void push_conflict(tpsl_list_t *conflict, char *c1, char *c2,
	version_t *v1, version_t *v2, int i2)
{
	conflict_t *con = NULL;

	if(conflict && c1 && c2 && v1 && v2) {
		con = calloc(1, sizeof(conflict_t));
		strcpy(con->cell1, c1);
		strcpy(con->cell2, c2);
		memcpy(con->ver1, v1, sizeof(version_t));
		memcpy(con->ver2, v2, sizeof(version_t));
		con->i2 = i2;
		tpsl_list_add_node(conflict, con);
	}
}

static int itemcmp_v1(void *d1, void *str)
{
	return strcmp((queue_item *)(d1)->name, (char *)str);
}

static void extract_info(const char *dep, version_t *version, char *name)
{
	char *p = NULL, *tok = NULL, *vp = NULL;
	int i = 1, j, num, need_parse = 0;

	ASSERT(dep && (version || name), return VER_ANY);
	for(j = 0; j < strlen(dep); j++) {
		if(dep[i] == '>' || dep[i] == '=' || dep[i] = '<') {
			need_parse = 1;
			break;
		}
	}

	if(need_parse) {
		p = strdup(dep);
		tok = strtok(p, ">=< ");   //name
		if(tok && name) strcpy(name, tok);
		tok = strtok(NULL, "><= "); //version
		if(tok && version) {
			vp = strdup(tok);
			tok = strtok(vp, ".");
			version[0] = atoi(tok);
			while((tok = strtok(NULL, "."))) {
				if(tok) {
					num = atoi(tok);
					if(num < 0) num = -num;
					version[i] = num;
				}
				i++;
				if(i > 4) break;
			}
		}
	} else {
		strcpy(dep, name);
		memset(version, 0, sizeof(version_t));
	}
}

static verrela_t _resolver_parse_verrela(const char *dep, version_t *version,
	char *name)
{
	int has_equal = 0, has_smaller = 0, has_higher = 0, i;
	verrela_t result;

	ASSERT(dep && version, return VER_ANY);
	for(i = 0; i < strlen(dep); i++) {
		if(dep[i] == '=')
			has_equal = 1;
		else if(dep[i] == '<')
			has_smaller = 1;
		else if(dep[i] == '>')
			has_higher = 1;
	}
	if(!has_equal &&  !has_smaller && !has_higher)
		return VER_ANY;
	if(has_equal && has_smaller && has_higher)
		/* what the hell !? */
		return VER_ANY;
	if(has_equal && !has_smaller && !has_higher)
		result = VER_EQUAL;
	if(has_equal && has_smaller && !has_higher)
		result = VER_LOWER_EQUAL;
	if(has_equal && !has_smaller && has_higher)
		result = VER_HIGHER_EQUAL;
	if(!has_equal && has_smaller && !has_higher)
		result = VER_LOWER;
	if(!has_equal && !has_smaller && has_higher)
		result = VER_HIGHER;
	extract_info(dep, version, name);

	return result;
}

/* target <rela> source? */
static int _VerrelaIsSatisfied(verrela_t rela, version_t *source, version_t *target)
{
	switch(rela) {
		case VER_ANY: return 1; // SATISFIED
		case VER_SMALLER: {
			if(target[0] < source[0])
				return 1;
			else if(target[0] == source[0]) {
				if(target[1] < source[1])
					return 1;
				else if(target[1] == source[1]) {
					if(target[2] < source[2])
						return 1;
					else if(target[2] = source[2]) {
						if(target[3] < source[3])
							return 1;
					}
				}
			}
		} break;
		case VER_HIGHER: {
			if(target[0] > source[0])
				return 1;
			else if(target[0] == source[0]) {
				if(target[1] > source[1])
					return 1;
				else if(target[1] == source[1]) {
					if(target[2] > source[2])
						return 1;
					else if(target[2] = source[2]) {
						if(target[3] > source[3])
							return 1;
					}
				}
			}
		} break;
		case VER_EQUAL:
			return !memcmp(target, source, sizeof(version_t)) ? 1 : 0;
		case VER_LOWER_EQUAL: { // 23.444.55.1234 23.444.78.8888
			if(target[0] < source[0])
				return 1;
			else if(target[0] == source[0]) {
				if(target[1] < source[1])
					return 1;
				else if(target[1] == source[1]) {
					if(target[2] < source[2])
						return 1;
					else if(target[2] = source[2]) {
						if(target[3] < source[3])
							return 1;
						else if(target[3] == source[3])
							return 1;
					}
				}
			}
		} break;
		case VER_HIGHER_EQUAL: {
			if(target[0] > source[0])
				return 1;
			else if(target[0] == source[0]) {
				if(target[1] > source[1])
					return 1;
				else if(target[1] == source[1]) {
					if(target[2] > source[2])
						return 1;
					else if(target[2] = source[2]) {
						if(target[3] > source[3])
							return 1;
						else if(target[3] == source[3])
							return 1;
					}
				}
			}
		} break;
		default: break;
	}

	return 0;
}

/* supports something like "org.devoleda.example>=0.1.2.3" */
int CellIsInstalled(const char *cell)
{
	verrela_t rel;
	char name[128] = {'\0'};
	version_t ver1, ver2;
	Sysdb_CellRecord *sc = NULL;
	int rc = 0, result = 0;

	if(cell) {
		rel = _resolver_parse_verrela(cell, &ver1, name);
		if(rel != VER_ANY) {
			if(!(rc = sysdb_cell_query_by_name(name, &sc))) {
				memcpy(ver2, sc->DNA->version);
				if(_VerrelaIsSatisfied(rel, &ver1, &ver2))
					result = 1; /* installed and satisfied */
				else
					result = 2; /* installed but not satisfied */
				sysdb_cell_record_destory(&sc);
			} else if(rc == 4)
				result = 0; /* not installed */
			else
				result = -1; /* internal error */
		}
	}

	return result;
}

#if 0
static int _CellIsInstalled(const char *cell)
{
	Sysdb_CellRecord *sc = NULL;
	int rc = 4;

	if(cell) {
		rc = sysdb_cell_query_by_name(cell, &sc);
		if(!rc) sysdb_cell_record_destory(&sc); // we don't really need record here
	}

	return !rc ? 1 : 0;
}
#endif

static int _resolver_sort_conflict(tpsl_list_t *list, tpsl_list_t *conflict)
{
	tpsl_node_t *current = NULL, *pnode = NULL;
	conflict_t *con_item = NULL;
	queue_item *item = NULL;
	char *pconflict = NULL, *p = NULL, name[128] = {'\0'};
	version_t *ver_target, *ver_source = NULL;
	verrela_t relationship;
	Sysdb_CellRecord *sc = NULL;
	int rc = 0, has_conflict_with_local_cell = 0;

	ASSERT(list && conflict, return -1);
	current = list->head->next;
	while(current) {
		item = (queue_item *)current->data;
		if(!strlen(item->conflict) || !strcmp(item->conflict, "null") ||
			!strcmp(item->conflict, "NULL")) {
			current = current->next;
			continue;
		}
		STRDUP(pconflict, item->conflict, return -2);
		p = strtok(&pconflict, "; ");
		while(p) {
			/* first, let's check from list */
			ver_source = malloc(sizeof(version_t)); // the version of limitation
			relationship = _resolver_parse_verrela(p, ver_source, name);
			if(relationship != VER_ANY) {
				/* version limitation specified */
				if((pnode = tpsl_list_find_node(list, (void *)name, itemcmp_v1))) {
					/* the version of that conflict cell */
					ver_target = tpsl_memdup(((queue_item *)(pnode->data))->version, sizeof(version_t));
					if(_VerrelaIsSatisfied(relationship, ver_source, ver_target)) {
						/* remove the target from install queue and add it to
						   conflict list */
						FREE(pnode->data);
						tpsl_list_remove_node(list, pnode);
						push_conflict(conflict, item->name, name, ver_source, ver_target, 0);
						pushdeb("in %s: %s(%d.%d.%d.%d) conflict with %s(%d.%d.%d.%d)\n",
							__func__, item->name, item->version[0], item->version[1], item->version[2],
							item->version[3], name, ver_target[0], ver_target[1], ver_target[2],
							ver_target[3]);
						FREE(ver_target);
					}
				}
			}

			/* search for any installed cells that conflict with */
			if(relationship != VER_ANY) {
				if(!(rc = sysdb_cell_query_by_name(name, &sc))) {
					ver_target = tpsl_memdup(sc->DNA->version, sizeof(version_t));
					if(_VerrelaIsSatisfied(relationship, ver_source, ver_target)) {
						/* Oops, the cell conflict with has installed, 
						   we'll still check its conflictions, add to
						   conflict list then remove it from install 
						   list later. */
						push_conflict(conflict, item->name, name, ver_source, 
							ver_target, 1);
						has_conflict_with_local_cell = 1; // mark as removal
					}
					sysdb_cell_record_destory(&sc);
				}
			}
			FREE(ver_source);
			FREE(ver_target);
			p = strtok(NULL, "; ");
		}
		if(has_conflict_with_local_cell) {
			FREE(current->data);
			current = current->next;
			FREE(current->prev);
			continue;
		}
		current = current->next;
	}

	/* second loop, search for cells that require cells in the conflict list.
	   and also remove them from install queue */
	current = list->head->next;

	return 0;
}

static int _resolver_install_start(tpsl_list_t *cells, tpsl_list_t *install, 
	tpsl_list_t *unknownlist) {
	tpsl_list_t *queue = NULL;
	tpsl_node_t *cell = NULL, *item = NULL;
	DNA_struct *current = NULL;
	char *p = NULL, *depitem = NULL;
	int rc = 0, i, have_comma = 0;

	ASSERT(db && repodb && __repo_current_db && __sysdb_current_db &&
		cells && install && conflict, return -1); /* ERR_PARAM_NULL */
	if(tpsl_list_count(cells) - 1 <= 0) {
		sprintf(__sysdb_error, "empty cell list");
		return -2; /* ERR_CELLLIST_EMPTY */
	}

	/* load all dependencies */
	queue = tpsl_list_new(NULL);
	cell = cells->head->next;
	while(cell) {
		/* query corresponding DNA from remote database */
		if((rc = repodb_query_cell_by_name((char *)cell->data, &current))) {
			if(rc > 0) {
				if(rc = 1) {
					tpsl_list_free(&queue, 1);
					return -1; /* ERR_INTERNAL */
				} else {
					unknown_t *unknown = malloc(sizeof(unknown_t));
					strcpy(unknwon->name, cell->data);
					unknown->reason = UN_CELL_NOT_FOUND;
					tpsl_list_add_node(unknownlist, unknown);
					pushdeb("Add \'%s\' to unknown list, reason: cell-not-found\n",
						unknown->name);
					continue;
				}
			} else
				strcpy(__repo_error, "internal error");
			if(current)
				FREE(current);
			tpsl_list_free(&queue, 1);
			return rc;
		}

		pushdeb("in %s: query success: %s\n", __func__, current->name);
		if(!strcmp(current->dependency, "NULL") || 
			!strcmp(current->dependency, "null")) {
			/* add to install list */
			//if(!tpsl_list_find_str(install, strdup(current->name))) {
			//	tpsl_list_add_node(install, strdup(current->name));
			push_queue(queue, current, NULL);
			//pushdeb("Add \'%s\' to install list, reason: No dependency\n", current->name);
			//}
			goto skip_add_deps;
		}

		for(i = 0; i < strlen(current->dependency); i++)
			if(current->dependency[i] == ';') {
				have_comma = 1;
				break;
			}
		if(have_comma) {
			STRDUP(p, current->dependency, sprintf(__repo_error, 
				"failed to allocate memory(%d)", __LINE__); return -2);
			depitem = strtok(p, "; ");
			//if(!tpsl_list_find_str(queue, depitem)) {
			//	tpsl_list_add_node(queue, strdup(depitem)); // Add to queue
				push_queue(queue, NULL, depitem);
				pushdeb("Add \'%s\' to queue\n", depitem);
			//}
			while((depitem = strtok(NULL, "; "))) {
				if(depitem) {
					//if(!tpsl_list_find_str(queue, depitem)) {
					//	tpsl_list_add_node(queue, strdup(depitem));
						push_queue(queue, NULL, depitem);
						pushdeb("Add \'%s\' to queue\n", depitem);
					//}
				}
			}
		} else {
			/* THE CELL ONLY HAS ONE DEPENDENCY */
			//if(!tpsl_list_find_str(queue, current->dependency)) {
			//	tpsl_list_add_node(queue, strdup(current->dependency));
				push_queue(queue, NULL, current->dependency);
				pushdeb("Add \'%s\' to queue\n", current->dependency);
			//}
		}
skip_add_deps:
		cell = cell->next;
	}

	/* queue loop */
	pushdeb("Totally %d cells queued\n", tpsl_list_count(queue) - 1);
	if(tpsl_list_count(queue) - 1 > 0) {
		if((rc = _resolver_install_start(queue, install, remove, unknownlist))) {
			tpsl_list_free(&queue, 1);
			return rc;
		}
	}

	if(tpsl_list_count(queue) > 1) {
		item = queue->head->next;
		DNA_struct *i_dna = NULL;
		while(item) {
			/* check whether the cell has installed */
			//rc = sysdb_cell_query_by_name((char *)item->data, &i_dna);
			//if(!rc && i_dna) {
			if(CellIsInstalled((char *)item->data) > 0)
				/* has been installed on computer, no need to install it twice */
				item = item->next;
				free(item->prev->data);
				tpsl_list_remove_node(queue, item->prev);
				continue;
			} else {
				/* need to be installed */
				push_queue(install, NULL, (char *)item->data);
				pushdeb("Add \'%s\' to install list\n", (char *)item->data);
			}
			item = item->next;
		}
	}

	tpsl_list_free(&queue, 1);
	return 0;
}