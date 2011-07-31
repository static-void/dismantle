/*
  Copyright (c) 2011 Edd Barrett. All rights reserved.

  This code was based upon the simplereader.c example distrubuted with
  dwarfutils. The original copyright and license follows.

  Copyright (c) 2009-2010	David Anderson.	All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the example nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY David Anderson ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL David Anderson BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <libdwarf.h>
#include <dwarf.h>

#include "tree.h"
#include "dm_dwarf.h"
#include "common.h"

extern FILE		*f;

RB_HEAD(dm_dwarf_sym_cache_, dm_dwarf_sym_cache_entry)
    dm_dwarf_sym_cache = RB_INITIALIZER(&dm_dwarf_sym_cache);
RB_GENERATE(dm_dwarf_sym_cache_,
    dm_dwarf_sym_cache_entry, entry, dm_dwarf_sym_rb_cmp);

int
dm_dwarf_sym_rb_cmp(struct dm_dwarf_sym_cache_entry *s1,
    struct dm_dwarf_sym_cache_entry *s2)
{
	return (strcmp(s1->name, s2->name));
}

int
dm_cmd_dwarf_funcs(char **args)
{

	struct dm_dwarf_sym_cache_entry		*sym;

	(void) args;

	RB_FOREACH(sym, dm_dwarf_sym_cache_, &dm_dwarf_sym_cache) {
		if (!sym->offset_err)
			printf("  Virtual: " ADDR_FMT_64
			    "   Offset: " ADDR_FMT_64 ":   %s()\n",
			    sym->vaddr, sym->offset, sym->name);
		else
			printf("  Virtual: " ADDR_FMT_64
			    "   Offset: %-10s:   %s()\n", sym->vaddr, "???", sym->name);
	}

	return (DM_OK); /* XXX */
}

int
dm_parse_dwarf()
{
	Dwarf_Debug			dbg = 0;
	Dwarf_Error			error;
	Dwarf_Handler			errhand = 0;
	Dwarf_Ptr			errarg = 0;
	int				ret = DM_OK;

	printf("%-40s", "Parsing dwarf symbols...");

	if (dwarf_init(fileno(f), DW_DLC_READ, errhand,
		    errarg, &dbg, &error) != DW_DLV_OK) {
		printf("Giving up, cannot do DWARF processing\n");
		goto error;
	}

	dm_dwarf_recurse_cu(dbg);

	if (dwarf_finish(dbg,&error) != DW_DLV_OK) {
		fprintf(stderr, "dwarf_finish failed!\n");
		goto error;
	}

	ret = DM_OK;
error:
	if (ret == DM_OK)
		printf("[OK]\n");
	else
		printf("[ERR]\n");

	return (ret);
}

void
dm_dwarf_recurse_cu(Dwarf_Debug dbg)
{
	Dwarf_Unsigned		cu_header_length = 0;
	Dwarf_Half		version_stamp = 0;
	Dwarf_Unsigned		abbrev_offset = 0;
	Dwarf_Half		address_size = 0;
	Dwarf_Unsigned		next_cu_header = 0;
	Dwarf_Error		error;
	int			cu_number = 0, res;
	Dwarf_Die		no_die = 0;
	Dwarf_Die		cu_die = 0;

	for(;;++cu_number) {
		no_die = cu_die = 0;
		res = DW_DLV_ERROR;

		res = dwarf_next_cu_header(dbg,&cu_header_length,
		    &version_stamp, &abbrev_offset, &address_size,
		    &next_cu_header, &error);

		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_next_cu_header\n");
			exit(1);
		}

		if (res == DW_DLV_NO_ENTRY)
			return; /* done */

		/* The CU will have a single sibling, a cu_die. */
		res = dwarf_siblingof(dbg,no_die,&cu_die,&error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_siblingof on CU die \n");
			exit(1);
		}

		if (res == DW_DLV_NO_ENTRY) { /* Impossible case. */
			printf("no entry! in dwarf_siblingof on CU die \n");
			exit(1);
		}

		dm_dwarf_recurse_die(dbg, cu_die, 0);
		dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
	}
}

void
dm_dwarf_recurse_die(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level)
{
	int			res = DW_DLV_ERROR;
	Dwarf_Die		cur_die = in_die, child = 0, sib_die = 0;
	Dwarf_Error		error;

	dm_dwarf_inspect_die(dbg, in_die,in_level);

	for (;;) {
		sib_die = 0;

		res = dwarf_child(cur_die,&child,&error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_child, level %d\n",in_level);
			exit(1);
		}

		if (res == DW_DLV_OK)
			dm_dwarf_recurse_die(dbg, child, in_level + 1);

		res = dwarf_siblingof(dbg, cur_die, &sib_die, &error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_siblingof, level %d\n",in_level);
			exit(1);
		}

		if (res == DW_DLV_NO_ENTRY)
			break;	/* Done at this level. */

		if (cur_die != in_die)
			dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);

		cur_die = sib_die;
		dm_dwarf_inspect_die(dbg, cur_die, in_level);
	}

	return;
}

int
dm_dwarf_inspect_die(Dwarf_Debug dbg, Dwarf_Die print_me, int level)
{
	char				*name = 0;
	Dwarf_Error			 error = 0;
	Dwarf_Half			 tag = 0;
	const char			*tagname = 0;
	int				 res;
	Dwarf_Addr			 lo;
	ADDR64				 offset;
	int				 offset_err = 0, ret = DM_FAIL;
	struct dm_dwarf_sym_cache_entry	*sym_rec;

	res = dwarf_diename(print_me,&name,&error);

	if (res == DW_DLV_ERROR) {
		printf("Error in dwarf_diename , level %d \n", level);
		goto clean;
	}

	if (res == DW_DLV_NO_ENTRY) {
		goto clean;
	}

	res = dwarf_tag(print_me, &tag, &error);
	if (res != DW_DLV_OK) {
		fprintf(stderr, "Error in dwarf_tag , level %d \n", level);
		goto clean;
	}

	if (tag != DW_TAG_subprogram) {
		ret = DM_OK;
		goto clean; /* we only extract funcs for now */
	}

	/* get virtual addr */
	res = dwarf_lowpc(print_me, &lo, &error);
	if (res != DW_DLV_OK) {
		fprintf(stderr, "Failed to dwarf_lopc()");
		if (res == DW_DLV_NO_ENTRY)
			fprintf(stderr, "no entry\n");
		goto clean;
	}

	/* get the function name */
	res = dwarf_get_TAG_name(tag, &tagname);
	if (res != DW_DLV_OK) {
		fprintf(stderr, "Failed to dwarf_get_TAG_name");
		goto clean;
	}

	offset_err = 0;
	if ((dm_offset_from_vaddr(lo, &offset)) != DM_OK)
		offset_err = 1;

	sym_rec = calloc(1, sizeof(struct dm_dwarf_sym_cache_entry));
	sym_rec->name = strdup(name);
	sym_rec->vaddr = lo;
	sym_rec->offset = offset;
	sym_rec->sym_type = DW_TAG_subprogram;
	sym_rec->offset_err = offset_err;
	RB_INSERT(dm_dwarf_sym_cache_, &dm_dwarf_sym_cache, sym_rec);

clean:
	if (name)
		dwarf_dealloc(dbg,name,DW_DLA_STRING);

	return (DM_OK);
}

int
dm_clean_dwarf()
{
	struct dm_dwarf_sym_cache_entry		*var, *nxt;

	for (var = RB_MIN(dm_dwarf_sym_cache_, &dm_dwarf_sym_cache); var != NULL; var = nxt) {
		nxt = RB_NEXT(dm_dwarf_sym_cache_, &dm_dwarf_sym_cache, var);
		RB_REMOVE(dm_dwarf_sym_cache_, &dm_dwarf_sym_cache, var);
		free(var->name);
		free(var);
	}

	return (DM_OK);
}
