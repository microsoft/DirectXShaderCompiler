// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {
  // CHECK: %[[p_load:[0-9]+]] = load i32, i32*
  // CHECK-SAME: @dx.preserve.value
  // CHECK: %[[p:[0-9]+]] = trunc i32 %[[p_load]] to i1

  float2 xy = float2(10, 20);
  // select i1 %[[p]], float 1.000000e+01, float 1.000000e+01
  // select i1 %[[p]], float 2.000000e+01, float 2.000000e+01

  float2 zw = xy + float2(5, 30);
  // CHECK: %[[a1:.+]] = fadd
  // CHECK: %[[a2:.+]] = fadd
  // select i1 %[[p]], float [[a1]], float [[a1]]
  // select i1 %[[p]], float [[a2]], float [[a2]]

  float2 foo = zw * 2;
  // CHECK: %[[b1:.+]] = fmul
  // CHECK: %[[b2:.+]] = fmul
  // select i1 %[[p]], float [[b1]], float [[b1]]
  // select i1 %[[p]], float [[b2]], float [[b2]]

  float2 bar = foo / 0.5;
  // CHECK: %[[c1:.+]] = fdiv
  // CHECK: %[[c2:.+]] = fdiv
  // select i1 %[[p]], float [[c1]], float [[c1]]
  // select i1 %[[p]], float [[c2]], float [[c2]]

  Texture2D tex = tex0; 
  // CHECK: load i32, i32*
  // CHECK-SAME: @dx.nothing

  // CHECK: br i1
  if (foo.x+bar.y >= 0) {
    tex = tex1;
    // CHECK: load i32, i32*
    // CHECK-SAME: @dx.nothing
    // CHECK: br
  }

  // CHECK: %[[d1:.+]] = fadd
  // CHECK: %[[d2:.+]] = fadd
  // CHECK: %[[d3:.+]] = fadd
  // CHECK: %[[d4:.+]] = fadd
  // select i1 %[[p]], float [[d1]], %[[preserve_f32]]
  // select i1 %[[p]], float [[d2]], %[[preserve_f32]]
  // select i1 %[[p]], float [[d3]], %[[preserve_f32]]
  // select i1 %[[p]], float [[d4]], %[[preserve_f32]]

  return tex.Load(0) + float4(foo,bar);
}



