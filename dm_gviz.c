/*
 * Copyright (c) 2011, Ed Robbins <edd.robbins@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>

#include "dm_gviz.h"

FILE*
dm_new_graph(char *filename)
{
	FILE *fp = fopen(filename, "w+");
	if (fp) fprintf(fp, "digraph g {\n\tnode [shape = \"box\"];\n");
	return fp;
}

void
dm_start_subgraph(FILE *fp, char *name, char *label)
{
	fprintf(fp, "subgraph cluster_%s {\n\tlabel=\"%s\";\n\tcolour=blue;\n", name, label);
}

void
dm_end_subgraph(FILE *fp)
{
	fprintf(fp, "}\n");
}

void
dm_add_edge(FILE *fp, char *source, char *dest)
{
	fprintf(fp, "\t\"%s\" -> \"%s\";\n", source, dest);
}

void
dm_add_label(FILE *fp, char *node, char *label)
{
	fprintf(fp, "\t\"%s\" [label=\"%s\"];\n", node, label);
}

void
dm_colour_label(FILE *fp, char *node, char* colour)
{
	fprintf(fp, "\t\"%s\" [fillcolor = \"%s\" style = \"filled\"];\n", node, colour);
}

void
dm_same_rank(FILE *fp)
{
	fprintf(fp, "\trank = same;\n");
}

void
dm_invisible_edge(FILE *fp)
{
	fprintf(fp, "\tedge [style=\"invis\"];\n");
}

void
dm_min_sep(FILE *fp)
{
	fprintf(fp, "\tsep=0.01;\n\tranksep=0.01;\n\tesep=0.01;\n");
}

void
dm_end_graph(FILE *fp)
{
	fprintf(fp, "}\n");
	fsync(fileno(fp));
	fclose(fp);
}

void
dm_display_graph(char *filename)
{
	char	*sys_buf = NULL;
	char	*disp = getenv("DISPLAY");
	int	 c;

	if (!disp)
		return;

	if (asprintf(&sys_buf, "dot -Txlib %s &", filename) != -1)
		c = system(sys_buf);
	free(sys_buf);
	(void) c;
}

