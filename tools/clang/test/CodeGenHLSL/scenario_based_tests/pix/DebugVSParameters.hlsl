// RUN: %dxc -Emain -Tvs_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2 | %FileCheck %s

// Check that the instance and vertex id are parsed and present:

// CHECK: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)
// CHECK: %VertId = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 undef)
// CHECK: %InstanceId = call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 0, i32 undef)
// CHECK: %CompareToVertId = icmp eq i32 %VertId, 1
// CHECK: %CompareToInstanceId = icmp eq i32 %InstanceId, 2
// CHECK: %CompareBoth = and i1 %CompareToVertId, %CompareToInstanceId
// CHECK: %OffsetMultiplicand = zext i1 %CompareBoth to i32


[RootSignature("")]
float4 main() : SV_Position{
  return float4(0,0,0,0);
}