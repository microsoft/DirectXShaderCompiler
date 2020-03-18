// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {
  // CHECK: %[[p_load:[0-9]+]] = load i32, i32*
  // CHECK-SAME: @dx.preserve.value
  // CHECK: %[[p:[0-9]+]] = trunc i32 %[[p_load]] to i1

  int x = 10;
  // select i1 %[[p]], i32 10, i32 10

  int y = x + 5;
  // CHECK: %[[a1:.+]] = add
  // select i1 %[[p]], i32 [[a1]], i32 [[a1]]

  int z = y * 2;
  // CHECK: %[[b1:.+]] = mul
  // select i1 %[[p]], i32 [[b1]], i32 [[b1]]

  int w = z / 0.5;
  // CHECK: sitofp
  // CHECK: fdiv
  // CHECK: %[[c1:.+]] = fptosi
  // select i1 %[[p]], i32 [[c1]], i32 [[c1]]

  Texture2D tex = tex0; 
  // CHECK: load i32, i32*
  // CHECK-SAME: @dx.nothing

  // CHECK: br i1
  if (w >= 0) {
    tex = tex1;
    // CHECK: load i32, i32*
    // CHECK-SAME: @dx.nothing
    // CHECK: br
  }

  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd

  return tex.Load(0) + float4(x,y,z,w);
}

