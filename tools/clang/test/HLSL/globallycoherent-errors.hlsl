// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

globallycoherent RWTexture1D<float4> uav1 : register(u3);
RWBuffer<float4> uav2;
globallycoherent Buffer<float4> srv; // expected-error {{'globallycoherent' is not a valid modifier for a non-UAV type}}
globallycoherent float m; // expected-error {{'globallycoherent' is not a valid modifier for a non-UAV type}}

globallycoherent RWTexture2D<float> tex[12];
globallycoherent RWTexture2D<float> texMD[12][12];

globallycoherent float One() { // expected-error{{'globallycoherent' is not a valid modifier for a non-UAV type}}
  return 1.0;
}

 float4 main(uint2 a : A, uint2 b : B) : SV_Target
{
  globallycoherent  RWTexture1D<float4> uav3 = uav1;
  globallycoherent float x = 3; // expected-error {{'globallycoherent' is not a valid modifier for a non-UAV type}}
  uav3[0] = srv[0];
  uav1[0] = 2;
  uav2[1] = 3;
  return 0;
}
