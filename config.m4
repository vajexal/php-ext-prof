PHP_ARG_WITH([prof],
  [for prof support],
  [AS_HELP_STRING([--with-prof],
    [Include prof support])])

if test "$PHP_PROF" != "no"; then
  PKG_CHECK_MODULES([ZLIB], [zlib >= 1.2.0.4])
  PHP_EVAL_INCLINE($ZLIB_CFLAGS)
  PHP_EVAL_LIBLINE($ZLIB_LIBS, PROF_SHARED_LIBADD)

  PROTOBUF_C_SOURCES="
    protobuf-c/protobuf-c/protobuf-c.c
  "

  PHP_ADD_INCLUDE(PHP_EXT_SRCDIR()/protobuf-c)

  AC_DEFINE(HAVE_PROF, 1, [ Have prof support ])

  PHP_SUBST(PROF_SHARED_LIBADD)
  PHP_NEW_EXTENSION(prof, prof.c helpers.c sampling.c func.c opcode.c errors.c prof_config.c gzencode.c profile.pb-c.c $PROTOBUF_C_SOURCES, $ext_shared)
fi
