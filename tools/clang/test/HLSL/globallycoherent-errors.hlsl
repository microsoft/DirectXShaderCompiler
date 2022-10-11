// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

globallycoherent RWTexture1D<float4> uav1 : register(u3);
RWBuffer<float4> uav2;
globallycoherent Buffer<float4> srv; // expected-error {{'globallycoherent' is not a valid modifier for a non-UAV type}}

 float4 main(uint2 a : A, uint2 b : B) : SV_Target
{
  globallycoherent  RWTexture1D<float4> uav3 = uav1;
  float x = 3;
  uav3[0] = srv[0];
  uav1[0] = 2;
  uav2[1] = 3;
  return 0;
}
