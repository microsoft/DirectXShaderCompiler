// RUN: %dxc -EFlowControlPS -Tps_6_0 %s -Od | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation | %FileCheck %s

// CHECK: oh for fucks sake

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