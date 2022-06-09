PHP_ARG_WITH([prof],
  [for prof support],
  [AS_HELP_STRING([--with-prof],
    [Include prof support])])

if test "$PHP_PROF" != "no"; then
  PROF_SOURCES="
    prof.c
    src/helpers.c
    src/sampling.c
    src/func.c
    src/opcode.c
    src/errors.c
    src/prof_config.c
    src/gzencode.c
    src/profile.pb-c.c
  "

  PKG_CHECK_MODULES([ZLIB], [zlib >= 1.2.0.4])
  PHP_EVAL_INCLINE($ZLIB_CFLAGS)
  PHP_EVAL_LIBLINE($ZLIB_LIBS, PROF_SHARED_LIBADD)

  PROTOBUF_C_SOURCES="
    protobuf-c/protobuf-c/protobuf-c.c
  "

  AC_DEFINE(HAVE_PROF, 1, [ Have prof support ])

  PHP_NEW_EXTENSION(prof, $PROF_SOURCES $PROTOBUF_C_SOURCES, $ext_shared)
  PHP_SUBST(PROF_SHARED_LIBADD)

  PHP_ADD_INCLUDE($ext_srcdir/src)
  PHP_ADD_INCLUDE($ext_srcdir/protobuf-c)
  PHP_ADD_BUILD_DIR($ext_builddir/src)
  PHP_ADD_BUILD_DIR($ext_builddir/protobuf-c)
fi
