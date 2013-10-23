bin_PROGRAMS = steamcompmgr loadargb_cursor

steamcompmgr_SOURCES = src/steamcompmgr.c src/glext.h
loadargb_cursor_SOURCES = src/loadargbcursor.c

AM_CFLAGS = $(DEPS_CFLAGS)
AM_LIBS = $(DEPS_LIBS)

steamcompmgr_CFLAGS = $(DEPS_CFLAGS)
steamcompmgr_LDADD = $(DEPS_LIBS)

loadargb_cursor_CFLAGS = $(DEPS_CFLAGS)
loadargb_cursor_LDADD = $(DEPS_LIBS)

dist_doc_DATA = README
