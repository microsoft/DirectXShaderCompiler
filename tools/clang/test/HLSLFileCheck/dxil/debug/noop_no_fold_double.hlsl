// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {
  // CHECK: %[[preserve_i32:[0-9]+]] = load i32, i32* @dx.preserve.value
  // CHECK-DAG: %[[preserve_f32:[0-9]+]] = sitofp i32 %[[preserve_i32]] to float
  // CHECK-DAG: %[[preserve_f64:[0-9]+]] = sitofp i32 %[[preserve_i32]] to double

  double x = 10;
  // fadd double 1.000000e+01, %[[preserve_f64]]

  double y = x + 5;
  // CHECK: %[[a1:.+]] = fadd
  // fadd double [[a1]], %[[preserve_f64]]

  double z = y * 2;
  // CHECK: %[[b1:.+]] = fmul
  // fadd double [[b1]], %[[preserve_f64]]

  double w = z / 0.5;
  // CHECK: %[[c1:.+]] = fdiv
  // fadd double [[c1]], %[[preserve_f64]]

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

