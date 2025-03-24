// RUN: %dxc -E main -T lib_6_9 %s | FileCheck %s

// CHECK: 'reordercoherent' is not a valid modifier for a declaration of type 'float'

reordercoherent RWTexture1D<float4> uav1 : register(u3);
RWBuffer<float4> uav2;
reordercoherent float m;
[shader("raygeneration")]
void main()
{
  reordercoherent  RWTexture1D<float4> uav3 = uav1;
  reordercoherent float x = 3;
  uav3[0] = 0;
  uav1[0] = 2;
  uav2[1] = 3;
  return 0;
}
