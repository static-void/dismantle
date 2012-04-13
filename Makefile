UDIS86_ARCHIVE=	udis86/libudis86/.libs/libudis86.a
LDFLAGS= 	-L/usr/local/lib -lelf -lreadline -ltermcap -ldwarf
CPPFLAGS=	-I/usr/local/include
CFLAGS=		-g -Wall -Wextra 

all: dismantle

udis86/Makefile: udis86/configure
	cd udis86 && ./configure

udis86: udis86/Makefile ${UDIS86_ARCHIVE}
	cd udis86 && ./configure && ${MAKE}

.PHONY: ${UDIS86_ARCHIVE}

DISMANTLE_DEPS = dismantle.c dismantle.h dm_open.h dm_dis.o dm_elf.o dm_cfg.o dm_gviz.o dm_dom.o \
	       dm_ssa.o dm_dwarf.o dm_util.o

dismantle: ${DISMANTLE_DEPS}
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle \
		dismantle.c dm_dis.o dm_elf.o dm_cfg.o dm_gviz.o dm_dom.o \
		    dm_ssa.o dm_dwarf.o dm_util.o ${UDIS86_ARCHIVE}

static: ${DISMANTLE_DEPS}
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o dismantle \
		dismantle.c dm_dis.o dm_elf.o dm_cfg.o dm_gviz.o dm_dom.o \
		    dm_ssa.o dm_dwarf.o dm_util.o /usr/lib/libdwarf.a \
		    /usr/lib/libelf.a /usr/lib/libreadline.a -lncurses ${UDIS86_ARCHIVE} ${CLOSED}

dismantle-closed: CC += -DDM_CLOSED
dismantle-closed: CLOSED = dm_tc.o
dismantle-closed: DISMANTLE_DEPS += dm_closed.h dm_tc.o
dismantle-closed: dm_tc.o dismantle

static-closed: CC += -DDM_CLOSED
static-closed: CLOSED =	dm_tc.o dm_tr.o
static-closed: DISMANTLE_DEPS += dm_closed.h dm_tc.o dm_tr.o
static-closed: dm_tc.o dm_tr.o static

dm_dis.o: dm_dis.c dm_dis.h common.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_dis.o dm_dis.c

dm_elf.o: dm_elf.c dm_elf.h common.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_elf.o dm_elf.c

dm_cfg.o: dm_cfg.c dm_cfg.h dm_dis.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_cfg.o dm_cfg.c

dm_gviz.o: dm_gviz.c dm_gviz.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_gviz.o dm_gviz.c

dm_dom.o: dm_dom.c dm_dom.h dm_cfg.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_dom.o dm_dom.c

dm_ssa.o: dm_ssa.c dm_ssa.h dm_dom.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_ssa.o dm_ssa.c

dm_dwarf.o: dm_dwarf.c dm_dwarf.h dm_elf.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_dwarf.o dm_dwarf.c

dm_util.o: dm_util.c dm_util.h
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_util.o dm_util.c

dm_tc.o: dm_tc.c dm_tc.h dm_ssa.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_tc.o dm_tc.c

dm_tr.o: dm_tr.c dm_tr.h dm_tc.o
	${CC} -c ${CPPFLAGS} ${CFLAGS} -o dm_tr.o dm_tr.c

clean:
	rm -f *.o *.dot dismantle && cd udis86 && ${MAKE} clean
