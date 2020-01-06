// RUN: %dxilver 1.7 | %dxc -E main -T ps_6_6 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {

  float2 xy = float2(10, 20);
  // CHECK: call void @llvm.donothing()

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
  // CHECK: call void @llvm.donothing()

  // CHECK: br i1
  if (foo.x+bar.y >= 0) {
    tex = tex1;
    // CHECK: call void @llvm.donothing()
    // CHECK: br
  }

  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  return tex.Load(0) + float4(foo,bar);
  // CHECK: call void @llvm.donothing()
}

