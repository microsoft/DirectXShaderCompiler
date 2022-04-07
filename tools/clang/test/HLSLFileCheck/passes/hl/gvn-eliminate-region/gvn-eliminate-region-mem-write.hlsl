// RUN: %dxc -T ps_6_0 -E main /opt-disable gvn %s | FileCheck %s

// CHECK: error: validation errors
// CHECK: Shader CBV descriptor range

RWTexture1D<float> my_uav : register(u0);

cbuffer cb : register(b6) {
  bool bCond;
};

cbuffer cb1 : register(b1) {
  float a, b, c;
};

float foo(float2 uv) {
  my_uav[0] = 10;
  if (a < b)
    return sin(a) + b + c;
  else
    return a - b + c;
}

float bar() {
  my_uav[0] = 10;
  if (a < b)
    return sin(a) + b + c;
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

