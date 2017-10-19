// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-f16:32-f32:32-n8:16:32"

float4 main(float4 a : A) : SV_Target {
  return 1;
}