// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: call float @dx.op.barycentrics.f32
// CHECK: call float @dx.op.barycentricsSampleIndex.f32
// CHECK: call float @dx.op.barycentricsCentroid.f32
// CHECK: call float @dx.op.barycentricsSnapped.f32

float4 main(float4 a : A) : SV_Target
{
  float4 vcolor0 = float4(1,0,0,1);
  float4 vcolor1 = float4(0,1,0,1);
  float4 vcolor2 = float4(0,0,0,1);

  float3 bary = GetBarycentrics();
  float3 barySample = GetBarycentricsAtSample(0);
  float3 baryCentroid = GetBarycentricsCentroid();
  float3 barySnapped = GetBarycentricsSnapped(uint2(4,10));

  float4 baryColor = bary.x * vcolor0 + bary.y * vcolor1 + bary.z * vcolor2;
  float4 barySampleColor = barySample.x * vcolor0 + barySample.y * vcolor1 + barySample.z * vcolor2;
  float4 baryCentroidColor = baryCentroid.x * vcolor0 + baryCentroid.y * vcolor1 + baryCentroid.z * vcolor2;
  float4 barySnappedColor = barySnapped.x * vcolor0 + barySnapped.y * vcolor1 + barySnapped.z * vcolor2;

  return (baryColor + barySampleColor + baryCentroidColor + barySnappedColor) / 4;
}