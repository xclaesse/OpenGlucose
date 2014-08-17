ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = --disable-debug

EXTRA_DIST = \
	autogen.sh \
	m4/compiler.m4 \
	m4/linker.m4 \
	m4/tp-compiler-flag.m4 \
	m4/tp-compiler-warnings.m4 \
	$(NULL)

CLEANFILES = FIXME.out

check-local::
	egrep -A 5 '[F]IXME|[T]ODO|[X]XX' $(srcdir)/folks/*.[ch] \
		> FIXME.out || true

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
