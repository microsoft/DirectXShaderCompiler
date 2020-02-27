// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float main() : SV_Target {
  // CHECK: %[[p_load:[0-9]+]] = load i32, i32* @dx.preserve.value
  // CHECK: %[[p:[0-9]+]] = trunc i32 %[[p_load]] to i1

  int x = 10;
  // CHECK: %[[x1:.+]] = select i1 %[[p]], i32 10, i32 10
  x = 6;
  // CHECK: %[[x2:.+]] = select i1 %[[p]], i32 %[[x1]], i32 6
  x = 10;
  // CHECK: %[[x3:.+]] = select i1 %[[p]], i32 %[[x2]], i32 10
  x = 40;
  // CHECK: %[[x4:.+]] = select i1 %[[p]], i32 %[[x3]], i32 40
  x = 80;
  // CHECK: %[[x5:.+]] = select i1 %[[p]], i32 %[[x4]], i32 80
  x = x * 5;
  // CHECK: %[[x6:.+]] = mul 
  // CHECK-SAME: %[[x5]]

  return x;
}

