AC_PREREQ([2.69])
AC_INIT([icetop], [0], [https://github.com/aperezdc/icetop/issues])
AC_CONFIG_SRCDIR([icetop.cc])

AC_PROG_CXX
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_14([noext], [mandatory])
AC_LANG_WERROR

PKG_CHECK_MODULES([ICETOP], [libdill icecc tickit])

CPPFLAGS="${CPPFLAGS} ${ICETOP_CFLAGS}"
LIBS="${LIBS} ${ICETOP_LIBS}"

AC_CHECK_HEADER([libdill.h], [],
				[AC_MSG_ERROR([The libdill.h header is required])])
AC_CHECK_HEADER([icecc/comm.h], [],
				[AC_MSG_ERROR([The icecc/comm.h header is required])])

# Check wether icecc was compiled using the old C++ ABI, and
# work around this to be able to still use C++11 (and later)
# features while being able to link to the library.
AC_MSG_CHECKING([whether IceCC was built using the pre-C++11 ABI])
AC_LINK_IFELSE([
	AC_LANG_SOURCE([[
	#include <icecc/comm.h>
	int main() { DiscoverSched sched("foo"); }
	]])
], [
	AC_MSG_RESULT([no])
	AC_DEFINE([_GLIBCXX_USE_CXX11_ABI], [1])
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([_GLIBCXX_USE_CXX11_ABI], [0])
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
