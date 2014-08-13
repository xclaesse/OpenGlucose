ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
    m4 \
    src \
    $(NULL)

DISTCHECK_CONFIGURE_FLAGS = --disable-debug

EXTRA_DIST = \
    autogen.sh \
    $(NULL)

CLEANFILES = FIXME.out

check-local::
	egrep -A 5 '[F]IXME|[T]ODO|[X]XX' $(srcdir)/folks/*.[ch] \
		> FIXME.out || true

