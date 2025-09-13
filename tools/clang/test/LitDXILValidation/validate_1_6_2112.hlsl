// REQUIRES: v1.6.2112

// Verify that the older DLL is being loaded, and that
// due to it being older, it errors on something that would
// be accepted in newer validators

// RUN: %dxc -T lib_6_9 %s | FileCheck %s 
// CHECK: error: validation errors
// CHECK: error: RWStructuredBuffers may increment or decrement their counters, but not both.
// CHECK: note: at '%3 = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle %1, i8 1)' in block '#0' of function 'main'.

RWStructuredBuffer<uint> UAV         : register(u0);

[numthreads(64, 1, 1)]
void main(uint tid : SV_GroupIndex) {
  UAV[UAV.IncrementCounter()] = tid;
  UAV[UAV.DecrementCounter()] = tid * 2;
}
