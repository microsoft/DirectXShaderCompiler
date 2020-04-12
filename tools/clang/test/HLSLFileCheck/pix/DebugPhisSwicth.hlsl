// RUN: %dxc -EFlowControlPS -Tps_6_0 %s -Od | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation | %FileCheck %s

// Check for a branch to a new block for each case:
// CHECK: br label %PIXDebug
// CHECK: br label %PIXDebug
// CHECK: br label %PIXDebug

// Check that three PIXDebug blocks emit some debug info and returns where we expect:
// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label

// Check that three PIXDebug blocks emit some debug info and returns where we expect:
// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label

// Check that three PIXDebug blocks emit some debug info and returns where we expect:
// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label


float4 FlowControlPS(in uint value : value ) : SV_Target
{
  float4 ret = float4(0, 0, 0, 0);
  switch (value)
  {
  case 0:
    ret = float4(1, 0, 0, 0);
    break;
  case 1:
    ret = float4(2, 0, 0, 0);
    break;
  default:
    ret = float4(3, 0, 0, 0);
    break;
  }
  return ret;
}