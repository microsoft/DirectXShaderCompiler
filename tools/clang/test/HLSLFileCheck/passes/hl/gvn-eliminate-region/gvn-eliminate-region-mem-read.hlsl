// RUN: %dxc -T ps_6_0 -E main /opt-disable gvn %s | FileCheck %s

// CHECK: @main
// CHECK: call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66
// CHECK-NOT: call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66

RWTexture1D<float> my_uav : register(u0);

cbuffer cb : register(b6) {
  bool bCond;
};

cbuffer cb1 : register(b1) {
  uint idx;
  float a, b, c;
};

float foo(float2 uv) {
  if (a < b)
    return my_uav[idx] + sin(a) + b + c;
  else
    return a - b + c;
}

float bar() {
  if (a < b)
    return my_uav[idx] + sin(a) + b + c;
  else
    return a - b + c;
}

[RootSignature("CBV(b1),DescriptorTable(UAV(u0))")]
float main(float2 uv : TEXCOORD) : SV_Target {
  if (bCond) {
    return foo(uv);
  }
  else {
    return bar();
  }
}


