// RUN: %dxc -T cs_6_0 -E main

struct Base {
  static const uint s_coefficientCount = 4;
  float4 m_coefficients[s_coefficientCount];
};

struct Derived : Base {
  float4 a;
};

RWStructuredBuffer<Derived> g_probes : register(u0);

[numthreads(64u, 1u, 1u)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
  Derived p;
  p.a = float4(0,0,0,0);

// Due to layout differences, 'p' has to be reconstructed to be stored in a RWStructuredBuffer.
// Layout differences mean that one type has decorations, and the other doesn't.
// Note that the m_coefficients is an array and has ArrayStride decoration when in RWStructuredBuffer,
// but does not have an ArrayStride decoration when used in the function scope (as part of p).
// Therefore the array needs to be reconstructed.
// The Derived.a member, however, is a vector, and has no decorations either way.
//
// CHECK:        [[p:%\d+]] = OpLoad %Derived_0 %p
// CHECK:   [[p_base:%\d+]] = OpCompositeExtract %Base_0 [[p]] 0
// CHECK:      [[arr:%\d+]] = OpCompositeExtract %_arr_v4float_uint_4_0 [[p_base]] 0
// CHECK:     [[arr0:%\d+]] = OpCompositeExtract %v4float [[arr]] 0
// CHECK:     [[arr1:%\d+]] = OpCompositeExtract %v4float [[arr]] 1
// CHECK:     [[arr2:%\d+]] = OpCompositeExtract %v4float [[arr]] 2
// CHECK:     [[arr3:%\d+]] = OpCompositeExtract %v4float [[arr]] 3
// CHECK:  [[new_arr:%\d+]] = OpCompositeConstruct %_arr_v4float_uint_4 [[arr0]] [[arr1]] [[arr2]] [[arr3]]
// CHECK: [[new_base:%\d+]] = OpCompositeConstruct %Base [[new_arr]]
// CHECK:        [[a:%\d+]] = OpCompositeExtract %v4float [[p]] 1
// CHECK:    [[new_p:%\d+]] = OpCompositeConstruct %Derived [[new_base]] [[a]]
// CHECK:                     OpStore {{%\d+}} [[new_p]]
	g_probes[0] = p;
}
