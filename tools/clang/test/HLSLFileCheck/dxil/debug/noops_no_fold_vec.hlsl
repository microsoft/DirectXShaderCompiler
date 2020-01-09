// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

// CHECK: @dx.nothing = internal constant i32 0

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {

  float2 xy = float2(10, 20);
  // CHECK: load i32, i32* @dx.nothing

  float2 zw = xy + float2(5, 30);
  // CHECK: fadd
  // CHECK: fadd

  float2 foo = zw * 2;
  // CHECK: fmul
  // CHECK: fmul

  float2 bar = foo / 0.5;
  // CHECK: fdiv
  // CHECK: fdiv

  Texture2D tex = tex0; 
  // CHECK: load i32, i32* @dx.nothing

  // CHECK: br i1
  if (foo.x+bar.y >= 0) {
    tex = tex1;
    // CHECK: load i32, i32* @dx.nothing
    // CHECK: br
  }

  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  return tex.Load(0) + float4(foo,bar);
  // CHECK: load i32, i32* @dx.nothing
}



