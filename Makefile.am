ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = --disable-debug

jqplotdir = $(datadir)/openglucose/jqplot
jqplot_DATA = \
	jqplot/*.min.css \
	jqplot/*.min.js \
	$(NULL)

jqplotpluginsdir = $(datadir)/openglucose/jqplot/plugins
jqplotplugins_DATA = \
	jqplot/plugins/*.min.js \
	$(NULL)

EXTRA_DIST = \
	autogen.sh \
	data/60-insulinx.rules \
	m4/compiler.m4 \
	m4/linker.m4 \
	m4/tp-compiler-flag.m4 \
	m4/tp-compiler-warnings.m4 \
	$(jqplot_DATA) \
	$(jqplotplugins_DATA) \
	$(NULL)

AM_CPPFLAGS = \
	$(OPENGLUCOSE_CFLAGS) \
	$(ERROR_CFLAGS) \
	$(NULL)

bin_PROGRAMS = openglucose

openglucose_SOURCES = \
	src/base-device.c src/base-device.h \
	src/device-info.c src/device-info.h \
	src/insulinx.c src/insulinx.h \
	src/main.c \
	$(NULL)
openglucose_LDADD = $(OPENGLUCOSE_LIBS)

doc_DATA = README AUTHORS COPYING

udevdir = $(UDEV_DIR)/rules.d
udev_DATA = data/60-insulinx.rules

