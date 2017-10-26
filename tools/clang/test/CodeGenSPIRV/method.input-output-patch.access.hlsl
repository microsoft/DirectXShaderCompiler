// Run: %dxc -T hs_6_0 -E SubDToBezierHS

#include "bezier_common_hull.hlsl"

HS_CONSTANT_DATA_OUTPUT PCF(OutputPatch<BEZIER_CONTROL_POINT, MAX_POINTS> op) {
  HS_CONSTANT_DATA_OUTPUT Output;
  // Must initialize Edges and Inside; otherwise HLSL validation will fail.
  Output.Edges[0]  = 1.0;
  Output.Edges[1]  = 2.0;
  Output.Edges[2]  = 3.0;
  Output.Edges[3]  = 4.0;
  Output.Inside[0] = 5.0;
  Output.Inside[1] = 6.0;

  uint x = 5;

// CHECK:      [[op_1_loc:%\d+]] = OpAccessChain %_ptr_Function_v3float %op %uint_1 %int_0
// CHECK-NEXT:          {{%\d+}} = OpLoad %v3float [[op_1_loc]]
  float3 out1pos = op[1].vPosition; // vPosition is member 0 in the BEZIER_CONTROL_POINT struct.

// CHECK:             [[x:%\d+]] = OpLoad %uint %x
// CHECK-NEXT: [[op_x_loc:%\d+]] = OpAccessChain %_ptr_Function_v3float %op [[x]] %int_0
// CHECK-NEXT:          {{%\d+}} = OpLoad %v3float [[op_x_loc]]
  float3 out5pos = op[x].vPosition; // vPosition is member 0 in the BEZIER_CONTROL_POINT struct.

  return Output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("line")]
[outputcontrolpoints(16)]
[patchconstantfunc("PCF")]
BEZIER_CONTROL_POINT SubDToBezierHS(InputPatch<VS_CONTROL_POINT_OUTPUT, MAX_POINTS> ip, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID) {
  BEZIER_CONTROL_POINT result;
  uint y = 5;

// CHECK:      [[ip_1_loc:%\d+]] = OpAccessChain %_ptr_Function_v3float %ip %uint_1 %int_0
// CHECK-NEXT:          {{%\d+}} = OpLoad %v3float [[ip_1_loc]]
  result.vPosition = ip[1].vPosition;  // vPosition is member 0 in the VS_CONTROL_POINT_OUTPUT struct.

// CHECK:             [[y:%\d+]] = OpLoad %uint %y
// CHECK-NEXT: [[ip_y_loc:%\d+]] = OpAccessChain %_ptr_Function_v3float %ip [[y]] %int_0
// CHECK-NEXT:          {{%\d+}} = OpLoad %v3float [[ip_y_loc]]
  result.vPosition = ip[y].vPosition;  // vPosition is member 0 in the VS_CONTROL_POINT_OUTPUT struct.

  return result;
}
