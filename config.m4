PHP_ARG_WITH([prof],
  [for prof support],
  [AS_HELP_STRING([--with-prof],
    [Include prof support])])

if test "$PHP_PROF" != "no"; then
  PKG_CHECK_MODULES([LIBPROTOBUF_C], [libprotobuf-c >= 1.0.0])
  PHP_EVAL_INCLINE($LIBPROTOBUF_C_CFLAGS)
  PHP_EVAL_LIBLINE($LIBPROTOBUF_C_LIBS, PROF_SHARED_LIBADD)
  PHP_SUBST(PROF_SHARED_LIBADD)

  AC_DEFINE(HAVE_PROF, 1, [ Have prof support ])

  PHP_NEW_EXTENSION(prof, prof.c helpers.c sampling.c func.c opcode.c errors.c prof_config.c profile.pb-c.c, $ext_shared)
fi
