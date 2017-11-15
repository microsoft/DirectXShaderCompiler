// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Make sure nest empty struct works.
// CHECK: main

struct KillerStruct {};

struct InnerStruct {
  KillerStruct s;
};

struct OuterStruct {
  InnerStruct s;
};

cbuffer Params_cbuffer : register(b0) {
  OuterStruct constants;
};

float4 main(float4 pos : POSITION) : SV_POSITION { return float4(0, 0, 0, 0); }
