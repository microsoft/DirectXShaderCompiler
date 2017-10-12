// Run: %dxc -T hs_6_0 -E main

#include "bezier_common_hull.hlsl"

// CHECK: [[fType:%\d+]] = OpTypeFunction %HS_CONSTANT_DATA_OUTPUT
// CHECK:          %main = OpFunction %void None {{%\d+}}
// CHECK:       {{%\d+}} = OpFunctionCall %HS_CONSTANT_DATA_OUTPUT %PCF
// CHECK:           %PCF = OpFunction %HS_CONSTANT_DATA_OUTPUT None [[fType]]

// PCF does not take any args
HS_CONSTANT_DATA_OUTPUT PCF() {
  HS_CONSTANT_DATA_OUTPUT Output;
  // Must initialize Edges and Inside; otherwise HLSL validation will fail.
  Output.Edges[0]  = 1.0;
  Output.Edges[1]  = 2.0;
  Output.Edges[2]  = 3.0;
  Output.Edges[3]  = 4.0;
  Output.Inside[0] = 5.0;
  Output.Inside[1] = 6.0;
  return Output;
}

[domain("isoline")]
[partitioning("fractional_odd")]
[outputtopology("line")]
[outputcontrolpoints(16)]
[patchconstantfunc("PCF")]
BEZIER_CONTROL_POINT main(InputPatch<VS_CONTROL_POINT_OUTPUT, MAX_POINTS> ip, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID) {
  VS_CONTROL_POINT_OUTPUT vsOutput;
  BEZIER_CONTROL_POINT result;
  result.vPosition = vsOutput.vPosition;
  return result;
}
