// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

// Test that non-const arithmetic are not optimized away and
// do not impact things that require comstant (Like sample offset);

static const int2 offsets[] = {
  int2(-1,-1),
  int2(1,-1),
  int2(1,1),
  int2(7,-8),
};

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
SamplerState samp0 : register(s0);

[RootSignature("DescriptorTable(SRV(t0), SRV(t1)), DescriptorTable(Sampler(s0))")]
float4 main(float2 uv : TEXCOORD) : SV_Target {
  // CHECK: %[[preserve_i32:[0-9]+]] = load i32, i32* @dx.preserve.value

  int a = 1;
  // CHECK: %[[preserve_a:.+]] = or i32 1, %[[preserve_i32]]

  int b = 2;
  // CHECK: %[[preserve_b:.+]] = or i32 2, %[[preserve_i32]]

  int d = a;
  // CHECK: %[[preserve_d:.+]] = or i32 %[[preserve_a]], %[[preserve_i32]]

  int e = d + b;
  // CHECK: %[[add:.+]] = add
  // CHECK: %[[preserve_e:.+]] = or i32 %[[add]], %[[preserve_i32]]

  // CHECK: call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 60, 
  // CHECK-SAME: i32 7, i32 -8
  return tex0.Sample(samp0, uv, offsets[e]);
}

