// RUN: %dxc -T cs_6_6 -HV 202x %s | FileCheck %s

RWBuffer<uint> output;
groupshared uint sharedValue;

[numthreads(1, 1, 1)]
void main(uint index : SV_DispatchThreadID) {
  uint old;
  hlsl::InterlockedAdd(output[index], 1u, old);
  hlsl::InterlockedAdd(sharedValue, old);
}

// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78,
// CHECK: atomicrmw add i32 addrspace(3)*
