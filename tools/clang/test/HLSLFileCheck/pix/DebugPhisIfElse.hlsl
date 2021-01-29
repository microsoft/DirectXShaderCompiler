// RUN: %dxc -EFlowControlPS -Tps_6_0 %s -Od | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation | %FileCheck %s

// Ensure that the pass added a block at the end of this if/else:
// CHECK: br label %PIXDebug
// CHECK: br label %PIXDebug

// Check that block 0 emits some debug info and returns where we expect:
// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label

// Check that block 1 emits some debug info and returns where we expect:
// CHECK: PIXDebug
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78
// CHECK: br label


float4 FlowControlPS(in uint value : value ) : SV_Target
{
  float4 ret = float4(0, 0, 0, 0);
  if (value > 1) {
    ret = float4(0, 0, 0, 2);
  } else {
    ret = float4(0, 0, 0, 1);
  }
  return ret;
}