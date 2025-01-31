// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

// CHECK: %hostlayout.foo = type { %hostlayout.union.s0 }
// CHECK: %hostlayout.union.s0 = type { [2 x <4 x float>] }
union s0 {
  int1x1 a;
  float4x2 b;
};

cbuffer foo {
  s0 input;
}

// CHECK: fadd
// CHECK: @dx.op.storeOutput.f32
float4x2 main() : OUT {
  s0 s;
  float4x2 mat = { 0.0f, 0.1f, 0.2f, 0.3f, 
               0.4f, 0.5f, 0.6f, 0.7f };
      
  s.b = mat + input.b;
  return s.b;
}
