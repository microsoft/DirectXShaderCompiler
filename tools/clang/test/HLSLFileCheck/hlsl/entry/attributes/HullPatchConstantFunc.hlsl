// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// CHECK: error: hull entry point must have the patchconstantfunc attribute

[shader("hull")]
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
//[patchconstantfunc("HSPatch")]
float4 main(uint ix : SV_OutputControlPointID)
{
  return 0;
}
