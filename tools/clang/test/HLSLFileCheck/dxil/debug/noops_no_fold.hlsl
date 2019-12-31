// RUN: %dxilver 1.7 | %dxc -E main -T ps_6_6 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1))")]
float4 main() : SV_Target {

  float x = 10;
  // CHECK: call void @llvm.donothing()

  float y = x + 5;
  // CHECK: fadd
  float z = y * 2;
  // CHECK: fmul
  float w = z / 0.5;
  // CHECK: fdiv

  Texture2D tex = tex0; 
  // CHECK: call void @llvm.donothing()

  // CHECK: br i1
  if (w >= 0) {
    tex = tex1;
    // CHECK: call void @llvm.donothing()
    // CHECK: br
  }

  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  // CHECK: fadd
  return tex.Load(0) + float4(x,y,z,w);
  // CHECK: call void @llvm.donothing()
}

