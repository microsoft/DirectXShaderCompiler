/*===---- x86intrin.h - X86 intrinsics -------------------------------------===
#ifndef __X86INTRIN_H
#define __X86INTRIN_H

#include <ia32intrin.h>

#include <immintrin.h>

#ifdef __3dNOW__
#include <mm3dnow.h>
#endif

#ifdef __BMI__
#include <bmiintrin.h>
#endif

#ifdef __BMI2__
#include <bmi2intrin.h>
#endif

#ifdef __LZCNT__
#include <lzcntintrin.h>
#endif

#ifdef __POPCNT__
#include <popcntintrin.h>
#endif

#ifdef __RDSEED__
#include <rdseedintrin.h>
#endif

#ifdef __PRFCHW__
#include <prfchwintrin.h>
#endif

#ifdef __SSE4A__
#include <ammintrin.h>
#endif

#ifdef __FMA4__
#include <fma4intrin.h>
#endif

#ifdef __XOP__
#include <xopintrin.h>
#endif

#ifdef __TBM__
#include <tbmintrin.h>
#endif

#ifdef __F16C__
#include <f16cintrin.h>
#endif

/* FIXME: LWP */

#endif /* __X86INTRIN_H */

