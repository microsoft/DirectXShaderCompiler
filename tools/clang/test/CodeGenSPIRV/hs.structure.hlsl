// Run: %dxc -T hs_6_0 -E main

#include "bezier_common_hull.hlsl"

// This test ensures:
// 1- The hull shader entry point function is invoked.
// 2- Control barrier is placed before PCF is called.
// 3- PCF is called by invocationID of 0.

// CHECK:         %main = OpFunction %void None {{%\d+}}
// CHECK:   [[id:%\d+]] = OpLoad %uint %gl_InvocationID

// CHECK:      {{%\d+}} = OpFunctionCall %BEZIER_CONTROL_POINT %src_main %param_var_ip %param_var_i %param_var_PatchID

// CHECK:                 OpControlBarrier %uint_2 %uint_4 %uint_0

// CHECK: [[cond:%\d+]] = OpIEqual %bool [[id]] %uint_0
// CHECK:                 OpSelectionMerge %if_merge None
// CHECK:                 OpBranchConditional [[cond]] %if_true %if_merge
// CHECK:      %if_true = OpLabel
// CHECK:      {{%\d+}} = OpFunctionCall %HS_CONSTANT_DATA_OUTPUT %SubDToBezierConstantsHS %param_var_ip %param_var_PatchID
// CHECK:                 OpBranch %if_merge
// CHECK:     %if_merge = OpLabel
// CHECK:                 OpReturn
// CHECK:                 OpFunctionEnd


[domain("isoline")]
[partitioning("fractional_odd")]
[outputtopology("line")]
[outputcontrolpoints(16)]
[patchconstantfunc("SubDToBezierConstantsHS")]
BEZIER_CONTROL_POINT main(InputPatch<VS_CONTROL_POINT_OUTPUT, MAX_POINTS> ip, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID) {
  VS_CONTROL_POINT_OUTPUT vsOutput;
  BEZIER_CONTROL_POINT result;
  result.vPosition = vsOutput.vPosition;
  return result;
}
