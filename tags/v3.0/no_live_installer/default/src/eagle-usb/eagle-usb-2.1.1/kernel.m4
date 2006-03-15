dnl
dnl Kernel related m4 macros
dnl

AC_DEFUN([AC_CHECK_KERNEL_VERSION],
[
AC_MSG_CHECKING([for kernel version])
cat <<EOF > conftest.c
#include <stdio.h>
#include "$KERNELSRC/include/linux/version.h"

int
main()
{
  fprintf(stdout, "%s\n", UTS_RELEASE);
}

EOF

gcc -I$KERNELSRC/include -o conftest conftest.c >> config.log 2>&1
if test -s ./conftest; then
    $1=`./conftest`
    AC_MSG_RESULT($KERNELVER)
else
    $1=none
    AC_MSG_RESULT([ not found ])
fi
rm -f conftest.c conftest
])