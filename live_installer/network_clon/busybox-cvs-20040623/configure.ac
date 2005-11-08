AC_INIT(Rules.mak.in)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADER(include/acconfig.h)

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_LD
AC_PROG_RANLIB
AC_CHECK_TOOL(STRIP, strip, :)

case "$host_cpu" in
  alpha*)    host_cpu_unified=alpha ;;
  i?86)      host_cpu_unified=i386  ;;
  arm*)      host_cpu_unified=arm   ;;
  mips*)     host_cpu_unified=mips  ;;
  sh*)       host_cpu_unified=sh    ;;
  powerpc)   host_cpu_unified=ppc   ;;
  powerpc64) host_cpu_unified=ppc64 ;;
  hppa2*)    host_cpu_unified=hppa64 ;;
  *)         host_cpu_unified=$host_cpu ;;
esac

AC_SUBST(host_cpu_unified)

case "$host_cpu_unified" in
  hppa*)     host_cpu_modutils=hppa
             host_cpu_modutils_biarch=hppa64 ;;
  ppc*)      host_cpu_modutils=ppc
             host_cpu_modutils_biarch=ppc64 ;;
  sparc*)    host_cpu_modutils=sparc
             host_cpu_modutils_biarch=sparc64 ;;
  s390*)     host_cpu_modutils=s390
             host_cpu_modutils_biarch=s390x ;;
  *)         host_cpu_modutils=$host_cpu_unified ;;
esac

AC_SUBST(host_cpu_modutils)
AC_SUBST(host_cpu_modutils_biarch)

case "$host_os" in
  linux*)    host_os_unified=linux ;;
  *)         host_os_unified=$host_os ;;
esac

AC_SUBST(host_os_unified)

AC_OUTPUT([
Rules.mak
])
