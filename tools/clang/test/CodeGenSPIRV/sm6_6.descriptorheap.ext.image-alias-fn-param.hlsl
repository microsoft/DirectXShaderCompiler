// XFAIL: *
// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// XFAIL: heap image alias passed to a function and used in an atomic.
// Same failure as alias-return.hlsl: [VUID-StandaloneSpirv-OpTypeImage-06924].
//
// The argument is copied into a Function-class "param.var.*" variable. The
// callee has no entry in the image alias map for its parameter, so the atomic
// falls back to OpImageTexelPointer on the parameter, pinning it as a variable
// and keeping the image-typed OpStore alive through validation.
//
// Plain reads/writes through the same parameter work — see alias-fn-readwrite.hlsl.
// Note: function-params.hlsl only checks the pre-legalization form (-fcgl),
// where the validator relaxes this rule, so it does not catch this failure.
//
// TODO: propagate descriptor index across function boundaries, then remove XFAIL.

// CHECK-DAG: %[[UntypedUniformConstant:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:              %[[RWTexType:[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 2 0 0 2 R32ui
// CHECK-DAG:             %[[RWTexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWTexType]]
// CHECK-DAG:           %[[UntypedImage:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR Image

// CHECK:               %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedUniformConstant]] UniformConstant

RWByteAddressBuffer outputBytes : register(u0);

void bump(RWTexture2D<uint> t, uint2 coord, out uint orig) {
  // CHECK:                     %[[Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[RWTexArray]] %[[ResourceHeap]] %uint_40
  // CHECK-NOT:                                           OpImageTexelPointer
  // CHECK:                 %[[TexelPtr:[a-zA-Z0-9_]+]] = OpUntypedImageTexelPointerEXT %[[UntypedImage]] %[[RWTexType]] %[[Desc]]
  // CHECK:                                               OpAtomicIAdd %uint %[[TexelPtr]]
  InterlockedAdd(t[coord], 1, orig);
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[40];
  uint r;
  bump(tex, tid.xy, r);
  outputBytes.Store(0, r);
}
