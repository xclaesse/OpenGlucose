AC_PREREQ([2.59])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set nano_version to 0
#   make the release, tag it
#   set nano_version to 1

m4_define([openglucose_major_version], [0])
m4_define([openglucose_minor_version], [0])
m4_define([openglucose_micro_version], [0])
m4_define([openglucose_nano_version], [1])

# Some magic
m4_define([openglucose_base_version],
          [openglucose_major_version.openglucose_minor_version.openglucose_micro_version])
m4_define([openglucose_version],
          [m4_if(openglucose_nano_version, 0, [openglucose_base_version],
              [openglucose_base_version].[openglucose_nano_version])])

AC_INIT([OpenGlucose], [openglucose_version], [])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE([1.11 -Wno-portability foreign subdir-objects])
AC_CONFIG_HEADERS(config.h)

AM_SILENT_RULES

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
LT_INIT
AC_PROG_MKDIR_P
PKG_PROG_PKG_CONFIG([0.21])

OPENGLUCOSE_MAJOR_VERSION=openglucose_major_version
OPENGLUCOSE_MINOR_VERSION=openglucose_minor_version
OPENGLUCOSE_MICRO_VERSION=openglucose_micro_version
AC_SUBST([OPENGLUCOSE_MAJOR_VERSION])
AC_SUBST([OPENGLUCOSE_MINOR_VERSION])
AC_SUBST([OPENGLUCOSE_MICRO_VERSION])

dnl optimizations, etc.
COMPILER_OPTIMISATIONS
COMPILER_COVERAGE
LINKER_OPTIMISATIONS
LINKER_VERSION_SCRIPT

ifelse(openglucose_nano_version, 0,
[ # openglucose is version x.y.z - "official release",
  # disable extra checks by default
  official_release=yes
  AC_ARG_ENABLE([fatal-warnings],
    [AS_HELP_STRING([--enable-fatal-warnings],
                    [make various warnings fatal])],
    [],
    [enable_fatal_warnings=no])
],
[ # openglucose is version x.y.z.1 - development snapshot,
  # enable extra checks by default
  official_release=no
  AC_ARG_ENABLE([fatal-warnings],
    [AS_HELP_STRING([--disable-fatal-warnings],
                    [make various warnings non-fatal])],
    [],
    [enable_fatal_warnings=yes])
])

AC_SUBST([enable_fatal_warnings])

TP_COMPILER_WARNINGS([ERROR_CFLAGS], [test "x$enable_fatal_warnings" = xyes],
  [all \
   extra \
   declaration-after-statement \
   shadow \
   strict-prototypes \
   missing-prototypes \
   sign-compare \
   nested-externs \
   pointer-arith \
   format-security \
   init-self],
  [missing-field-initializers \
   unused-parameter])
AC_SUBST([ERROR_CFLAGS])

GLIB_REQUIRED=2.40.0
AC_DEFINE([GLIB_VERSION_MIN_REQUIRED], [GLIB_VERSION_2_40], [Ignore post 2.40 deprecations])
AC_DEFINE([GLIB_VERSION_MAX_ALLOWED], [GLIB_VERSION_2_40], [Prevent post 2.40 APIs])

GUSB_REQUIRED=0.1.6
AC_DEFINE([G_USB_API_IS_SUBJECT_TO_CHANGE], [1], [Acknowledge that it is unstable API])

GTK_REQUIRED=3.10
AC_DEFINE([GTK_VERSION_MIN_REQUIRED], [GLIB_VERSION_3_10], [Ignore post 3.10 deprecations])
AC_DEFINE([GTK_VERSION_MAX_ALLOWED], [GLIB_VERSION_3_10], [Prevent post 3.10 APIs])

WEBKIT_REQUIRED=2.4.3

PKG_CHECK_MODULES(OPENGLUCOSE,
[
  glib-2.0 >= $GLIB_REQUIRED
  gobject-2.0 >= $GLIB_REQUIRED
  gio-2.0 >= $GLIB_REQUIRED
  gusb >= $GUSB_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
  webkit2gtk-3.0 >= WEBKIT_REQUIRED
])

# Only use this where we really do want things to depend on whether it's
# a release or not (like ABI-stability enforcement). For fatal warnings,
# use ${enable_fatal_warnings} instead.
AM_CONDITIONAL([OFFICIAL_RELEASE], [test "x$official_release" = xyes])

AC_ARG_ENABLE(debug,
  AS_HELP_STRING([--disable-debug],[compile without debug code (note that this will disable much of the debug code in all GLib connection managers)]),
    enable_debug=$enableval, enable_debug=yes )

AS_IF([test x$enable_debug = xyes],
  [AC_DEFINE([ENABLE_DEBUG], [], [Enable debug code])])

AC_MSG_CHECKING([udev dir])
UDEV_DIR=`pkg-config --variable=udevdir udev`
AC_MSG_RESULT([$UDEV_DIR])
AC_SUBST(UDEV_DIR)

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG gio-2.0 --variable=glib_compile_resources`
AC_SUBST(GLIB_COMPILE_RESOURCES)

AC_OUTPUT(Makefile)
