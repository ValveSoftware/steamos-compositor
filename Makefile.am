bin_PROGRAMS = steamcompmgr loadargb_cursor udev_is_boot_vga

steamcompmgr_SOURCES = src/steamcompmgr.c src/glext.h
loadargb_cursor_SOURCES = src/loadargbcursor.c
udev_is_boot_vga_SOURCES = src/udev_is_boot_vga.c

AM_CFLAGS = $(DEPS_CFLAGS)
AM_LIBS = $(DEPS_LIBS)

steamcompmgr_CFLAGS = $(DEPS_CFLAGS)
steamcompmgr_LDADD = $(DEPS_LIBS)

loadargb_cursor_CFLAGS = $(DEPS_CFLAGS)
loadargb_cursor_LDADD = $(DEPS_LIBS)

udev_is_boot_vga_CFLAGS = $(DEPS_CFLAGS)
udev_is_boot_vga_LDADD = $(DEPS_LIBS)

dist_doc_DATA = README
