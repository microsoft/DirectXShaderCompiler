// REQUIRES: v1.6.2112

// Verify that the older DLL is being loaded, and that
// due to it being older, it errors on something that would
// be accepted in newer validators.
// Specifically, the v1.6.2112 validator would emit the 
// below validation error, while validators newer than 1.6
// would be able to validate the module.

// RUN: not dxc -T cs_6_0 -validator-version 1.7 %s 2>&1 | FileCheck %s
// CHECK: error: Validator version in metadata (1.7) is not supported; maximum: (1.6).
// CHECK: error: RWStructuredBuffers may increment or decrement their counters, but not both.
// CHECK: note: at '%3 = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle %1, i8 1)' in block '#0' of function 'main'.
// CHECK: Validation failed.

RWStructuredBuffer<uint> UAV         : register(u0);

[numthreads(64, 1, 1)]
void main(uint tid : SV_GroupIndex) {
  UAV[UAV.IncrementCounter()] = tid;
  UAV[UAV.DecrementCounter()] = tid * 2;
}
