
#ifndef V3DVK_VALGRIND_H
#define V3DVK_VALGRIND_H

#ifdef HAVE_VALGRIND
#  include <valgrind.h>
#  define VG(x) x
#else
#  define VG(x)
#endif

#endif // V3DVK_VALGRIND_H
