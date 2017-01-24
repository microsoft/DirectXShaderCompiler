/*===---- nmmintrin.h - SSE4 intrinsics ------------------------------------===
#ifndef _NMMINTRIN_H
#define _NMMINTRIN_H

#ifndef __SSE4_2__
#error "SSE4.2 instruction set not enabled"
#else

/* To match expectations of gcc we put the sse4.2 definitions into smmintrin.h,
   just include it now then.  */
#include <smmintrin.h>
#endif /* __SSE4_2__ */
#endif /* _NMMINTRIN_H */

