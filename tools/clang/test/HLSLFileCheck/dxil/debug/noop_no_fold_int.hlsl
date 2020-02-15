// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {
  // CHECK: %[[preserve_i32:[0-9]+]] = load i32, i32* @dx.preserve.value

  int x = 10;
  // or i32 10, %[[preserve_i32]]

  int y = x + 5;
  // CHECK: %[[a1:.+]] = add
  // or i32 [[a1]], %[[preserve_i32]]

  int z = y * 2;
  // CHECK: %[[b1:.+]] = mul
  // or i32 [[b1]], %[[preserve_f32]]

  int w = z / 0.5;
  // CHECK: sitofp
  // CHECK: fdiv
  // CHECK: %[[c1:.+]] = fptosi
  // or i32 [[c1]], %[[preserve_f32]]

  Texture2D tex = tex0; 
  // CHECK: load i32, i32* @dx.nothing

  // CHECK: br i1
  if (w >= 0) {
    tex = tex1;
    // CHECK: load i32, i32* @dx.nothing
    // CHECK: br
  }

  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd

  return tex.Load(0) + float4(x,y,z,w);
  // CHECK: load i32, i32* @dx.nothing
}

