// Run: %dxc -T hs_6_0 -E main

#include "bezier_common_hull.hlsl"

// This test ensures:
// 1- Result of the main Hull shader entry point is written to output variable.
// 2- TessLevelOuter and TessLevelInner builtins are written to.
// 3- All other outputs of the PCF are written to output variables.

// CHECK: OpEntryPoint TessellationControl %main "main" %in_var_hullEntryPointInput %gl_InvocationID %gl_PrimitiveID %out_var_hullEntryPointOutput %gl_TessLevelOuter %gl_TessLevelInner %out_var_TANGENT %out_var_TEXCOORD %out_var_TANUCORNER %out_var_TANVCORNER %out_var_TANWEIGHTS

// CHECK: OpDecorate %gl_InvocationID BuiltIn InvocationId
// CHECK: OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
// CHECK: OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter
// CHECK: OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner
// CHECK: OpDecorate %in_var_hullEntryPointInput Location 0
// CHECK: OpDecorate %out_var_hullEntryPointOutput Location 0
// CHECK: OpDecorate %out_var_TANGENT Location 1
// CHECK: OpDecorate %out_var_TEXCOORD Location 2
// CHECK: OpDecorate %out_var_TANUCORNER Location 3
// CHECK: OpDecorate %out_var_TANVCORNER Location 4
// CHECK: OpDecorate %out_var_TANWEIGHTS Location 5

// CHECK:   %in_var_hullEntryPointInput = OpVariable %_ptr_Input__arr_VS_CONTROL_POINT_OUTPUT_uint_16 Input
// CHECK:              %gl_InvocationID = OpVariable %_ptr_Input_uint Input
// CHECK:               %gl_PrimitiveID = OpVariable %_ptr_Input_uint Input
// CHECK: %out_var_hullEntryPointOutput = OpVariable %_ptr_Output__arr_BEZIER_CONTROL_POINT_uint_16 Output
// CHECK:            %gl_TessLevelOuter = OpVariable %_ptr_Output__arr_float_uint_4 Output
// CHECK:            %gl_TessLevelInner = OpVariable %_ptr_Output__arr_float_uint_2 Output
// CHECK:              %out_var_TANGENT = OpVariable %_ptr_Output__arr_v3float_uint_4 Output
// CHECK:             %out_var_TEXCOORD = OpVariable %_ptr_Output__arr_v2float_uint_4 Output
// CHECK:           %out_var_TANUCORNER = OpVariable %_ptr_Output__arr_v3float_uint_4 Output
// CHECK:           %out_var_TANVCORNER = OpVariable %_ptr_Output__arr_v3float_uint_4 Output
// CHECK:           %out_var_TANWEIGHTS = OpVariable %_ptr_Output_v4float Output

// CHECK:      %param_var_ip = OpVariable %_ptr_Function__arr_VS_CONTROL_POINT_OUTPUT_uint_16 Function
// CHECK:       %param_var_i = OpVariable %_ptr_Function_uint Function
// CHECK: %param_var_PatchID = OpVariable %_ptr_Function_uint Function

// CHECK: [[mainResult:%\d+]] = OpFunctionCall %BEZIER_CONTROL_POINT %src_main %param_var_ip %param_var_i %param_var_PatchID
// CHECK: [[mainLoc:%\d+]] = OpAccessChain %_ptr_Output_BEZIER_CONTROL_POINT %out_var_hullEntryPointOutput {{%\d+}}
// CHECK: OpStore [[mainLoc]] [[mainResult]]

// CHECK:      [[PCFResult:%\d+]] = OpFunctionCall %HS_CONSTANT_DATA_OUTPUT %SubDToBezierConstantsHS %param_var_ip %param_var_PatchID
// CHECK-NEXT:       [[tso:%\d+]] = OpCompositeExtract %_arr_float_uint_4 [[PCFResult]] 0
// CHECK-NEXT:                      OpStore %gl_TessLevelOuter [[tso]]
// CHECK-NEXT:       [[tsi:%\d+]] = OpCompositeExtract %_arr_float_uint_2 [[PCFResult]] 1
// CHECK-NEXT:                      OpStore %gl_TessLevelInner [[tsi]]
// CHECK-NEXT:   [[tangent:%\d+]] = OpCompositeExtract %_arr_v3float_uint_4 [[PCFResult]] 2
// CHECK-NEXT:                      OpStore %out_var_TANGENT [[tangent]]
// CHECK-NEXT:  [[texcoord:%\d+]] = OpCompositeExtract %_arr_v2float_uint_4 [[PCFResult]] 3
// CHECK-NEXT:                      OpStore %out_var_TEXCOORD [[texcoord]]
// CHECK-NEXT:      [[tanu:%\d+]] = OpCompositeExtract %_arr_v3float_uint_4 [[PCFResult]] 4
// CHECK-NEXT:                      OpStore %out_var_TANUCORNER [[tanu]]
// CHECK-NEXT:      [[tanv:%\d+]] = OpCompositeExtract %_arr_v3float_uint_4 [[PCFResult]] 5
// CHECK-NEXT:                      OpStore %out_var_TANVCORNER [[tanv]]
// CHECK-NEXT:      [[tanw:%\d+]] = OpCompositeExtract %v4float [[PCFResult]] 6
// CHECK-NEXT:                      OpStore %out_var_TANWEIGHTS [[tanw]]

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
