ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = --disable-debug

jqplotdir = $(datadir)/openglucose/jqplot
jqplot_DATA = \
	$(srcdir)/jqplot/*.min.css \
	$(srcdir)/jqplot/*.min.js \
	$(NULL)

jqplotpluginsdir = $(datadir)/openglucose/jqplot/plugins
jqplotplugins_DATA = \
	$(srcdir)/jqplot/plugins/*.min.js \
	$(NULL)

EXTRA_DIST = \
	autogen.sh \
	data/60-insulinx.rules \
	m4/compiler.m4 \
	m4/linker.m4 \
	m4/tp-compiler-flag.m4 \
	m4/tp-compiler-warnings.m4 \
	src/average-chart.js \
	src/chart.html \
	src/modal-day-chart.js \
	src/openglucose.css \
	src/openglucose.gresource.xml \
	$(jqplot_DATA) \
	$(jqplotplugins_DATA) \
	$(NULL)

AM_CPPFLAGS = \
	$(OPENGLUCOSE_CFLAGS) \
	$(ERROR_CFLAGS) \
	-DDATA_DIR=\"$(datadir)/openglucose\" \
	$(NULL)

bin_PROGRAMS = openglucose

src/openglucose-resources.c: src/openglucose.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies $(srcdir)/src/openglucose.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ \
		--sourcedir=$(srcdir) \
		--generate-source \
		$<

src/openglucose-resources.h: src/openglucose.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies $(srcdir)/src/openglucose.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ \
		--sourcedir=$(srcdir) \
		--generate-header \
		$<

openglucose_SOURCES = \
	src/base-device.c src/base-device.h \
	src/device-widget.c src/device-widget.h \
	src/dummy-device.c src/dummy-device.h \
	src/insulinx.c src/insulinx.h \
	src/main.c \
	src/main-window.c src/main-window.h \
	src/record.c src/record.h \
	$(NULL)
nodist_openglucose_SOURCES = \
	src/openglucose-resources.c \
	src/openglucose-resources.h \
	$(NULL)
openglucose_LDADD = $(OPENGLUCOSE_LIBS) -lm

BUILT_SOURCES = \
	$(nodist_openglucose_SOURCES) \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

doc_DATA = README AUTHORS COPYING

udevdir = $(UDEV_DIR)/rules.d
udev_DATA = $(srcdir)/data/60-openglucose.rules

