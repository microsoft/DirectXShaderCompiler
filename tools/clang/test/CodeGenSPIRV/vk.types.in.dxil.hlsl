// RUN: not %dxc -T ps_6_0 -E PSMain -fcgl  %s -DINTEGRAL_CONSTANT 2>&1 | FileCheck %s --check-prefix=INTEGRAL_CONSTANT

#ifdef INTEGRAL_CONSTANT
// INTEGRAL_CONSTANT: 5:14: error: unknown type name 'integral_constant'
static const integral_constant<uint> MyVar;
#endif

// RUN: not %dxc -T ps_6_0 -E PSMain -fcgl  %s -DSPIRV_TYPE 2>&1 | FileCheck %s --check-prefix=SPIRV_TYPE

#ifdef SPIRV_TYPE
// SPIRV_TYPE: 12:14: error: unknown type name 'SpirvType'
static const SpirvType<uint> MyVar;
#endif

// RUN: not %dxc -T ps_6_0 -E PSMain -fcgl  %s -DSPIRV_OPAQUE_TYPE 2>&1 | FileCheck %s --check-prefix=SPIRV_OPAQUE_TYPE

#ifdef SPIRV_OPAQUE_TYPE
// SPIRV_OPAQUE_TYPE: 19:14: error: unknown type name 'SpirvOpaqueType'
static const SpirvOpaqueType<uint> MyVar;
#endif

// RUN: not %dxc -T ps_6_0 -E PSMain -fcgl  %s -DLITERAL 2>&1 | FileCheck %s --check-prefix=LITERAL

#ifdef LITERAL
// LITERAL: 26:14: error: unknown type name 'Literal'
static const Literal<uint> MyVar;
#endif
