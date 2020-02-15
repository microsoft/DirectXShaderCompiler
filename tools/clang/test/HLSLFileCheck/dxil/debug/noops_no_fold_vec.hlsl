// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

// CHECK: @dx.nothing = internal constant i32 0

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {
  // CHECK: %[[preserve_i32:[0-9]+]] = load i32, i32* @dx.preserve.value
  // CHECK: %[[preserve_f32:[0-9]+]] = sitofp i32 %[[preserve_i32]]

  float2 xy = float2(10, 20);
  // fadd float 1.000000e+01, %[[preserve_f32]]
  // fadd float 2.000000e+01, %[[preserve_f32]]

  float2 zw = xy + float2(5, 30);
  // CHECK: %[[a1:.+]] = fadd
  // CHECK: %[[a2:.+]] = fadd
  // fadd float [[a1]], %[[preserve_f32]]
  // fadd float [[a2]], %[[preserve_f32]]

  float2 foo = zw * 2;
  // CHECK: %[[b1:.+]] = fmul
  // CHECK: %[[b2:.+]] = fmul
  // fadd float [[b1]], %[[preserve_f32]]
  // fadd float [[b2]], %[[preserve_f32]]

  float2 bar = foo / 0.5;
  // CHECK: %[[c1:.+]] = fdiv
  // CHECK: %[[c2:.+]] = fdiv
  // fadd float [[c1]], %[[preserve_f32]]
  // fadd float [[c2]], %[[preserve_f32]]

  Texture2D tex = tex0; 
  // CHECK: load i32, i32* @dx.nothing

  // CHECK: br i1
  if (foo.x+bar.y >= 0) {
    tex = tex1;
    // CHECK: load i32, i32* @dx.nothing
    // CHECK: br
  }

  // CHECK: %[[d1:.+]] = fadd
  // CHECK: %[[d2:.+]] = fadd
  // CHECK: %[[d3:.+]] = fadd
  // CHECK: %[[d4:.+]] = fadd
  // fadd float [[d1]], %[[preserve_f32]]
  // fadd float [[d2]], %[[preserve_f32]]
  // fadd float [[d3]], %[[preserve_f32]]
  // fadd float [[d4]], %[[preserve_f32]]

  return tex.Load(0) + float4(foo,bar);
  // CHECK: load i32, i32* @dx.nothing
}



