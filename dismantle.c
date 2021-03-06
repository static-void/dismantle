/*
 * Copyright (c) 2011, Edd Barrett <vext01@gmail.com>
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

#include <stdio.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "dismantle.h"

uint8_t				 colours_on = 1;

char				 *debug_names[] = {
				    "error", "warn", "info", "debug"};

int	dm_setting_cmp(struct dm_setting *e1, struct dm_setting *e2);
RB_HEAD(dm_settings_tree, dm_setting) head = RB_INITIALIZER(&head);
RB_GENERATE(dm_settings_tree, dm_setting, entry, dm_setting_cmp);

char *banner =
"        	       ___                            __  __   \n"
"         	  ____/ (_)________ ___  ____ _____  / /_/ /__ \n"
"        	 / __  / / ___/ __ `__ \\/ __ `/ __ \\/ __/ / _ \\\n"
"        	/ /_/ / (__  ) / / / / / /_/ / / / / /_/ /  __/\n"
"        	\\__,_/_/____/_/ /_/ /_/\\__,_/_/ /_/\\__/_/\\___/ \n"
"\n"
"        	      Small i386/amd64 binary browser\n"
"\n"
"        	  (c) Edd Barrett 2011	<vext01@gmail.com>\n"
"        	  (c) Ed Robbins 2011	<edd.robbins@gmail.com>\n";

int				 dm_debug = DM_D_WARN;
struct dm_file_info		file_info;

void	dm_parse_cmd(char *line);
void	dm_update_prompt();
void	dm_interp();
int	dm_dump_hex_pretty(uint8_t *buf, size_t sz, NADDR start_addr);
int	dm_dump_hex(size_t bytes);
int	dm_settings_init();
int	dm_clean_settings();

#define DM_MAX_PROMPT			32
char			prompt[DM_MAX_PROMPT];

#define DM_HEX_CHUNK           16
/*
 * pretty print bytes from a buffer (as hex)
 */
int
dm_dump_hex_pretty(uint8_t *buf, size_t sz, NADDR start_addr)
{
	size_t	done = 0;

	if (sz > 16)
		return (DM_FAIL);

	printf("  " NADDR_FMT ":  ", start_addr);

	/* first hex view */
	for (done = 0; done < 16; done++) {
		if (done < sz)
			printf("%02x ", buf[done]);
		else
			printf("   ");
	}
	printf("    ");

	/* now ASCII view */
	for (done = 0; done < sz; done++) {
		if ((buf[done] > 0x20) && (buf[done] < 0x7e))
			printf("%c", buf[done]);
		else
			printf(".");
	}
	printf("\n");

	return (DM_OK);
}

/*
 * reads a number of bytes from a file and pretty prints them
 * in lines of 16 bytes
 */
int
dm_dump_hex(size_t bytes)
{
	size_t		orig_pos = ftell(file_info.fptr);
	size_t		done = 0, read = 0, to_read = DM_HEX_CHUNK;
	uint8_t		buf[DM_HEX_CHUNK];

	printf("\n");
	for (done = 0; done < bytes; done += read) {
		if (DM_HEX_CHUNK > bytes - done)
			to_read = bytes - done;

		read = fread(buf, 1, to_read, file_info.fptr);

		if ((!read) && (ferror(file_info.fptr))) {
			perror("failed to read bytes");
			clearerr(file_info.fptr);
			return (DM_FAIL);
		}
		dm_dump_hex_pretty(buf, read, orig_pos + done);

		if ((!read) && (feof(file_info.fptr)))
			break;
	}

	if (fseek(file_info.fptr, orig_pos, SEEK_SET) < 0) {
		perror("could not seek file");
		return (DM_FAIL);
	}
	printf("\n");

	return (DM_OK);
}

int
dm_cmd_hex(char **args)
{
	dm_dump_hex(strtoll(args[0], NULL, 0));
	return (DM_OK);
}

int
dm_cmd_hex_noargs(char **args)
{
	(void) args;
	dm_dump_hex(64);
	return (DM_OK);
}

int
dm_cmd_info(char **args)
{
	(void) args;

	printf("  %-16s %s\n", "Filename:", file_info.name);
	//printf("  %-16s " NADDR_FMT, "Size:", file_info.stat.st_size);
	printf("  %-16s %hd\n", "Bits:", file_info.bits);
	printf("  %-16s %s\n", "Ident:", file_info.ident);
	printf("  %-16s %hd\n", "ELF:", file_info.elf);
	printf("  %-16s %hd\n", "DWARF:", file_info.dwarf);

	return (DM_OK);
}

/* XXX when we have more search funcs, move into dm_search.c */
int
dm_cmd_findstr(char **args)
{
	NADDR                    byte = 0;
	char                    *find = args[0], *cmp = NULL;
	size_t                   find_len = strlen(find);
	size_t                   orig_pos = ftell(file_info.fptr), read;
	int                      ret = DM_FAIL;
	int                      hit = 0;

	if (file_info.stat.st_size < (off_t) find_len) {
		fprintf(stderr, "file not big enough for that string\n");
		goto clean;
	}

	rewind(file_info.fptr);

	cmp = malloc(find_len);
	for (byte = 0; byte < file_info.stat.st_size - find_len; byte++) {

		if (fseek(file_info.fptr, byte, SEEK_SET))
			perror("fseek");

		read = fread(cmp, 1, find_len, file_info.fptr);
		if (!read) {
			if (feof(file_info.fptr))
				break;
			perror("could not read file");
			goto clean;
		}

		if (memcmp(cmp, find, find_len) == 0)
			printf("  HIT %03d: " NADDR_FMT "\n", hit++, byte);
	}

	ret = DM_OK;
clean:
	if (cmp)
		free(cmp);

	if (fseek(file_info.fptr, orig_pos, SEEK_SET))
		perror("fseek");

	return ret;
}


int
dm_cmd_help()
{
	struct dm_help_rec	*h = help_recs;

	while (h->cmd != 0) {
		printf("%-15s   %s\n", h->cmd, h->descr);
		h++;
	}

	return (DM_OK);
}

#define DM_CMD_MAX_TOKS			8
void
dm_parse_cmd(char *line)
{
	int			 found_toks = 0;
	char			*tok, *next = line;
	char			*toks[DM_CMD_MAX_TOKS];
	struct dm_cmd_sw	*cmd = dm_cmds;

	toks[found_toks++] = strtok(next, " ");
	while ((found_toks < DM_CMD_MAX_TOKS) && (tok = strtok(NULL, " "))) {
		toks[found_toks++] = tok;
	}

	while (cmd->cmd != NULL) {
		if ((strcmp(cmd->cmd, toks[0]) != 0) ||
		    (cmd->args != found_toks - 1)) {
			cmd++;
			continue;
		}

		cmd->handler(&toks[1]);
		break;
	}

	if (cmd->cmd == NULL)
		printf("parse error\n");
}

void
dm_update_prompt()
{
	snprintf(prompt, DM_MAX_PROMPT, NADDR_FMT " dm> ", cur_addr);
}

void
dm_interp()
{
	char			*line;

	dm_update_prompt();
	while((line = readline(prompt)) != NULL) {
		if (*line) {
			add_history(line);
			dm_parse_cmd(line);
		}
		free(line);
		dm_update_prompt();
	}
	printf("\n");
}

char *fname;

int
dm_open_file(char *path)
{
	char *fname2;

	memset(&file_info, 0, sizeof(file_info));
	file_info.bits = 64; /* we guess */
	file_info.name = path;

	if ((file_info.fptr = fopen(path, "r")) == NULL) {
		DPRINTF(DM_D_ERROR, "Failed to open '%s': %s", path, strerror(errno));
		return (DM_FAIL);
	}

	if (fstat(fileno(file_info.fptr), &file_info.stat) < 0) {
		perror("fstat");
		return (DM_FAIL);
	}
	
	/* Get filename */
	fname = strtok(path, "/");
	while ((fname2 = strtok(NULL, "/")) != NULL) {
		fname = fname2;
	}
	printf("filename: %s\n", fname);

	return (DM_OK);
}

void
dm_show_version()
{
	printf("%s\n", banner);
	printf("%-32s%s\n\n", "", "Version: " PACKAGE_VERSION);
}

void
usage()
{
	printf("Usage: dismantle [args] <elf binary>\n\n");
	printf("  Arguments:\n");
	printf("    -a         Disable ANSII colours\n");
	printf("    -x lvl     Set debug level to 'lvl'\n");
	printf("    -v         Show version and exit\n\n");
}

int
main(int argc, char **argv)
{
	int			ch, getopt_err = 0, getopt_exit = 0;
	GElf_Shdr		shdr;

	while ((ch = getopt(argc, argv, "ahx:v")) != -1) {
		switch (ch) {
		case 'a':
			colours_on = 0;
			break;
		case 'x':
			dm_debug = atoi(optarg);
			if ((dm_debug < 0) || (dm_debug > 3))
				getopt_err = 0;
			break;
		case 'v':
			dm_show_version();
			getopt_exit = 1;
		case 'h':
			getopt_exit = 1;
			break;
		default:
			getopt_err = 1;
		}
	}

	/* command line args were bogus or we just need to exit */
	if ((getopt_err)  || (getopt_exit)) {
		if (getopt_err)
			DPRINTF(DM_D_ERROR, "Bogus usage!\n");

		usage();
		goto clean;
	}

	/* initialise settings */
	dm_settings_init();

	/* check a binary was supplied */
	if (argc == optind) {
		DPRINTF(DM_D_ERROR, "Missing filename\n");
		usage();
		goto clean;
	}

	/* From here on, cmd line was A-OK */
	if (dm_open_file(argv[optind]) != DM_OK)
		goto clean;

	/* parse elf and dwarf junk */
	dm_init_elf();
	dm_parse_pht();
	dm_parse_dwarf();

	ud_init(&ud);
	ud_set_input_file(&ud, file_info.fptr);
	ud_set_mode(&ud, file_info.bits);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	/* start at .text */
	if (file_info.elf) {
		dm_find_section(".text", &shdr);
		dm_seek(shdr.sh_offset);
	} else {
		dm_seek(0);
	}

	dm_show_version();
	dm_cmd_info(NULL);
	printf("\n");

	dm_interp();

	/* clean up */
clean:
	dm_clean_elf();
	dm_clean_dwarf();
	dm_clean_settings();

	if (file_info.fptr != NULL)
		fclose(file_info.fptr);

	return (EXIT_SUCCESS);
}

int
dm_cmd_debug(char **args)
{
	int		lvl = atoi(args[0]);

	if ((lvl < 0) || (lvl > 3)) {
		DPRINTF(DM_D_ERROR, "Debug level is between 0 and 3");
		return (DM_FAIL);
	}

	dm_debug = lvl;

	return (DM_OK);
}

int
dm_cmd_debug_noargs(char **args)
{
	(void) args;

	printf("\n  %d (%s)\n\n", dm_debug, debug_names[dm_debug]);
	return (DM_OK);
}

int
dm_cmd_ansii(char **args)
{
	colours_on = atoi(args[0]);

	if (colours_on > 1)
		colours_on = 1;

	return (DM_OK);
}

int
dm_cmd_ansii_noargs(char **args)
{
	(void) args;
	printf("\n  %d\n\n", colours_on);
	return (DM_OK);
}

int
dm_setting_cmp(struct dm_setting *e1, struct dm_setting *e2)
{
	return (strcmp(e1->name, e2->name));
}

int
dm_setting_add_int(char *name, int default_val, char *help)
{
	struct dm_setting		*s;

	s = xmalloc(sizeof(struct dm_setting));

	s->name = xstrdup(name);
	s->type = DM_SETTING_INT;
	s->val.ival = default_val;
	s->help = xstrdup(help);

	RB_INSERT(dm_settings_tree, &head, s);

	return (DM_OK);
}

int
dm_setting_add_str(char *name, char *default_val, char *help)
{
	struct dm_setting		*s;

	s = xmalloc(sizeof(struct dm_setting));

	s->name = xstrdup(name);
	s->type = DM_SETTING_STR;
	s->val.sval = xstrdup(default_val);
	s->help = xstrdup(help);

	RB_INSERT(dm_settings_tree, &head, s);

	return (DM_OK);
}

int
dm_settings_init()
{
	dm_setting_add_int("ssa.transform", 1, "SSA: Instruction transforming (1 = default = On, 0 = off");
	dm_setting_add_int("ssa.flatten", 1, "SSA: Flatten indirect addressing (1 = default = On, 0 = off)");
	dm_setting_add_int("cfg.verbose", 1, "Control flow graph verbosity (0=postorder, 1=start address, 2=full)");
	dm_setting_add_int("cfg.fcalls", 1, "Control flow graph, follow calls/jumps? (0=don't, 1=end at 1st branch, 2=all jumps, 3=all local, 4=all");
	dm_setting_add_str("cfg.outfile", "XXX", "CFG output file");
	dm_setting_add_str("cfg.gvfile", "XXX", "Graphviz CFG output file");
	dm_setting_add_int("pref.ansi", -1, "Use ANSI colour terminal");
	dm_setting_add_int("arch.bits", -1, "64 or 32 bit architecture");
	dm_setting_add_int("dbg.level", -1, "Debug level");

	return (DM_OK);
}

int
dm_cmd_set_noargs(char **args)
{
	struct dm_setting	*s;

	(void) args;

	RB_FOREACH(s, dm_settings_tree, &head) {
		switch(s->type) {
		case DM_SETTING_INT:
			printf("%s=%d\n", s->name, s->val.ival);
			break;
		case DM_SETTING_STR:
			printf("%s=\"%s\"\n", s->name, s->val.sval);
			break;
		default:
			DPRINTF(DM_D_WARN, "Unknown config type");
			break;
		};
	}

	return (DM_OK);
}

int
dm_find_setting(char *name, struct dm_setting **s)
{
	struct dm_setting	find;

	find.name = name;

	if ((*s = RB_FIND(dm_settings_tree, &head, &find)) == NULL) {
		DPRINTF(DM_D_WARN, "No such setting: %s\n", name);
		return (DM_FAIL);
	}

	return (DM_OK);
}

int
dm_cmd_set_one_arg(char **args)
{
	struct dm_setting		*s;

	if (dm_find_setting(args[0], &s) == DM_FAIL)
		return (DM_FAIL);

	switch (s->type) {
	case DM_SETTING_INT:
		printf("%s=%d\n%s\n", s->name, s->val.ival, s->help);
		break;
	case DM_SETTING_STR:
		printf("%s=\"%s\"\n%s\n", s->name, s->val.sval, s->help);
		break;
	default:
		DPRINTF(DM_D_WARN, "Unknown setting type");
		return (DM_FAIL);
		break;
	};

	return (DM_OK);
}

int
dm_cmd_set_two_args(char **args)
{
	struct dm_setting		*s;

	if (dm_find_setting(args[0], &s) == DM_FAIL)
		return (DM_FAIL);

	switch (s->type) {
	case DM_SETTING_INT:
		s->val.ival = atoi(args[1]);
		break;
	case DM_SETTING_STR:
		free(s->val.sval);
		s->val.sval = xstrdup(args[1]);
		break;
	default:
		DPRINTF(DM_D_WARN, "Unknown setting type");
		return (DM_FAIL);
		break;
	};

	return (DM_OK);
}

int
dm_clean_settings()
{
	struct dm_setting	*s;

	RB_FOREACH(s, dm_settings_tree, &head) {
		free(s->name);
		if (s->type ==  DM_SETTING_STR)
			free(s->val.sval);
	}

	return (DM_OK);
}
