PHP_ARG_ENABLE([prof],
  [whether to enable prof support],
  [AS_HELP_STRING([--enable-prof],
    [Enable prof support])],
  [no])

if test "$PHP_PROF" != "no"; then
  AC_DEFINE(HAVE_PROF, 1, [ Have prof support ])

  PHP_NEW_EXTENSION(prof, prof.c helpers.c sampling.c func.c opcode.c, $ext_shared)
fi
