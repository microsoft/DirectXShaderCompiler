// Run: %dxc -T hs_6_0 -E main

#include "bezier_common_hull.hlsl"

// Test: PCF takes the output (OutputPatch) of the main entry point function.


// CHECK: OpEntryPoint TessellationControl %main "main" {{%\w+}} {{%\w+}} {{%\w+}} %out_var_hullEntryPointOutput {{%\w+}}

// CHECK:               %_arr_BEZIER_CONTROL_POINT_uint_16 = OpTypeArray %BEZIER_CONTROL_POINT %uint_16
// CHECK:   %_ptr_Output__arr_BEZIER_CONTROL_POINT_uint_16 = OpTypePointer Output %_arr_BEZIER_CONTROL_POINT_uint_16
// CHECK: %_ptr_Function__arr_BEZIER_CONTROL_POINT_uint_16 = OpTypePointer Function %_arr_BEZIER_CONTROL_POINT_uint_16
// CHECK:                                   [[fType:%\d+]] = OpTypeFunction %HS_CONSTANT_DATA_OUTPUT %_ptr_Function__arr_BEZIER_CONTROL_POINT_uint_16
// CHECK:                    %out_var_hullEntryPointOutput = OpVariable %_ptr_Output__arr_BEZIER_CONTROL_POINT_uint_16 Output

// CHECK:                             %main = OpFunction %void None {{%\d+}}
// CHECK:    %temp_var_hullEntryPointOutput = OpVariable %_ptr_Function__arr_BEZIER_CONTROL_POINT_uint_16 Function

// CHECK:              [[id:%\d+]] = OpLoad %uint %gl_InvocationID
// CHECK:      [[mainResult:%\d+]] = OpFunctionCall %BEZIER_CONTROL_POINT %src_main %param_var_ip %param_var_i %param_var_PatchID
// CHECK-NEXT:  [[outputLoc:%\d+]] = OpAccessChain %_ptr_Output_BEZIER_CONTROL_POINT %out_var_hullEntryPointOutput [[id]]
// CHECK-NEXT:                       OpStore [[outputLoc]] [[mainResult]]
// CHECK-NEXT:    [[tempLoc:%\d+]] = OpAccessChain %_ptr_Function_BEZIER_CONTROL_POINT %temp_var_hullEntryPointOutput [[id]]
// CHECK-NEXT:                       OpStore [[tempLoc]] [[mainResult]]

// CHECK:                 {{%\d+}} = OpFunctionCall %HS_CONSTANT_DATA_OUTPUT %PCF %temp_var_hullEntryPointOutput

// CHECK:      %PCF = OpFunction %HS_CONSTANT_DATA_OUTPUT None [[fType]]
// CHECK-NEXT:  %op = OpFunctionParameter %_ptr_Function__arr_BEZIER_CONTROL_POINT_uint_16

HS_CONSTANT_DATA_OUTPUT PCF(OutputPatch<BEZIER_CONTROL_POINT, MAX_POINTS> op) {
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
