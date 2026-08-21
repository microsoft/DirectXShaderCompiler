// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies that heap-alias image reads and writes survive function boundaries
// (both return and parameter directions) with the correct descriptor slot.
//
//   returned from a function -> slot 40, OpImageRead/OpImageWrite
//   passed as a parameter    -> slot 41, OpImageRead/OpImageWrite
//
// Plain reads/writes work because the loaded handle is enough for spirv-opt to
// promote the copies. Atomics fail because the descriptor index does not cross
// OpFunctionCall — see alias-return.hlsl and image-alias-fn-param.hlsl.
// Keep this passing when fixing the atomic case.

// CHECK-DAG: %[[UntypedUniformConstant:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:              %[[RWTexType:[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 2 0 0 2 R32ui
// CHECK-DAG:             %[[RWTexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWTexType]]

// CHECK:               %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedUniformConstant]] UniformConstant

RWByteAddressBuffer outputBytes : register(u0);

RWTexture2D<uint> getTex() {
  RWTexture2D<uint> t = ResourceDescriptorHeap[40];
  return t;
}

uint bump(RWTexture2D<uint> t, uint2 coord) {
  uint v = t[coord];
  t[coord] = v + 1;
  return v;
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  // Returned from a function: uses of the returned resource hit slot 40.
  // CHECK:                  %[[RetDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[RWTexArray]] %[[ResourceHeap]] %uint_40
  // CHECK:                %[[RetHandle:[a-zA-Z0-9_]+]] = OpLoad %[[RWTexType]] %[[RetDesc]]
  // CHECK-NOT:                                           OpStore {{.*}} %[[RetHandle]]
  // CHECK:                                               OpImageRead %v4uint %[[RetHandle]]
  // CHECK:                                               OpImageWrite %[[RetHandle]]
  RWTexture2D<uint> returned = getTex();
  uint r0 = returned[tid.xy];
  returned[tid.xy] = r0 + 1;

  // Passed to a function: uses inside the callee hit slot 41.
  // CHECK:                %[[ParamDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[RWTexArray]] %[[ResourceHeap]] %uint_41
  // CHECK:              %[[ParamHandle:[a-zA-Z0-9_]+]] = OpLoad %[[RWTexType]] %[[ParamDesc]]
  // CHECK-NOT:                                           OpStore {{.*}} %[[ParamHandle]]
  // CHECK:                                               OpImageRead %v4uint %[[ParamHandle]]
  // CHECK:                                               OpImageWrite %[[ParamHandle]]
  RWTexture2D<uint> passed = ResourceDescriptorHeap[41];
  uint r1 = bump(passed, tid.xy);

  outputBytes.Store(0, r0);
  outputBytes.Store(4, r1);
}
