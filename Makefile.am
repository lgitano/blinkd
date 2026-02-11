sbin_PROGRAMS = blinkd
bin_PROGRAMS = blink
blink_SOURCES = blink.c
blinkd_SOURCES = blinkd.c
blink_LDADD =
blinkd_LDADD = -lpthread
man_MANS = blink.1 blinkd.8
SUBDIRS = po
localedir = $(datadir)/locale
INCLUDES = -DLOCALEDIR=\"$(localedir)\"
AM_CFLAGS = -Wall -ansi -pedantic -O2 -g
DB2MAN=http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
XP=xsltproc --nonet --novalid
EXTRA_DIST=intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update

blink.1: blink.dbk
	$(XP) $(DB2MAN) $<

blinkd.8: blinkd.dbk
	$(XP) $(DB2MAN) $<

