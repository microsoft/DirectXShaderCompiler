// Run: %dxc -T hs_6_0 -E SubDToBezierHS

#include "bezier_common_hull.hlsl"

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
// CHECK: OpExecutionMode %SubDToBezierHS OutputVertices 16
[outputcontrolpoints(16)]
[patchconstantfunc("SubDToBezierConstantsHS")]
BEZIER_CONTROL_POINT SubDToBezierHS(InputPatch<VS_CONTROL_POINT_OUTPUT, MAX_POINTS> ip, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID) {
  VS_CONTROL_POINT_OUTPUT vsOutput;
  BEZIER_CONTROL_POINT result;
  result.vPosition = vsOutput.vPosition;
  return result;
}
