// Run: %dxc -T hs_6_0 -E main -fspv-reflect

#define NumOutPoints 2

// CHECK: OpCapability Tessellation
// CHECK: OpCapability ClipDistance
// CHECK: OpCapability CullDistance

// CHECK: OpExtension "SPV_GOOGLE_hlsl_functionality1"

// Input control point
struct HsCpIn
{
    float4 pos     : SV_Position;      // Builtin Position
    float2 clip0   : SV_ClipDistance0; // Builtin ClipDistance
    float  cull5   : SV_CullDistance5; // Builtin CullDistance
    float1 clip2   : SV_ClipDistance2; // Builtin ClipDistance
    float3 cull3   : SV_CullDistance3; // Builtin CullDistance

    float3 baz     : BAZ;              // Input variable

    [[vk::builtin("PointSize")]]
    float  ptSize  : PSIZE;            // Builtin PointSize
};

struct CpInner2 {
    float1 clip8   : SV_ClipDistance8; // Builtin ClipDistance
    float2 cull6   : SV_CullDistance6; // Builtin CullDistance
    float3 foo     : FOO;              // Output variable

    [[vk::builtin("PointSize")]]
    float  ptSize  : PSIZE;            // Builtin PointSize
};

struct CpInner1 {
    float4   pos   : SV_Position;      // Builtin Position
    CpInner2 s;
    float    clip6 : SV_ClipDistance6; // Builtin ClipDistance
    float4   bar   : BAR;              // Output variable
};

// Output control point
struct HsCpOut
{
    float3   cull9 : SV_CullDistance9; // Builtin CullDistance
    CpInner1 s;
    float1   clip7 : SV_ClipDistance7; // Builtin ClipDistance
};

// Output patch constant data.
struct HsPcfOut
{
  float tessOuter[4] : SV_TessFactor;
  float tessInner[2] : SV_InsideTessFactor;

  float2 texCoord[4] : TEXCOORD;
  float4 weight      : WEIGHT;
};

// Per-vertex    input  builtin : Position, PointSize, ClipDistance, CullDistance, InvocationID
// Per-vertex    output builtin : Position, PointSize, ClipDistance, CullDistance
// Per-vertex    input  variable: BAZ
// Per-vertex    output variable: FOO, BAR

// Per-primitive input  builtin : PrimitiveID
// Per-primitive output builtin : TessLevelInner, TessLevelOuter
// Per-primitive output variable: TEXCOORD, WEIGHT

// CHECK: OpEntryPoint TessellationControl %main "main" %gl_ClipDistance %gl_CullDistance %gl_ClipDistance_0 %gl_CullDistance_0 %gl_Position %in_var_BAZ %gl_PointSize %gl_InvocationID %gl_PrimitiveID %gl_Position_0 %out_var_FOO %gl_PointSize_0 %out_var_BAR %gl_TessLevelOuter %gl_TessLevelInner %out_var_TEXCOORD %out_var_WEIGHT

// CHECK: OpDecorate %gl_ClipDistance BuiltIn ClipDistance
// CHECK: OpDecorateStringGOOGLE %gl_ClipDistance HlslSemanticGOOGLE "SV_ClipDistance"
// CHECK: OpDecorate %gl_CullDistance BuiltIn CullDistance
// CHECK: OpDecorateStringGOOGLE %gl_CullDistance HlslSemanticGOOGLE "SV_CullDistance"
// CHECK: OpDecorate %gl_ClipDistance_0 BuiltIn ClipDistance
// CHECK: OpDecorateStringGOOGLE %gl_ClipDistance_0 HlslSemanticGOOGLE "SV_ClipDistance"
// CHECK: OpDecorate %gl_CullDistance_0 BuiltIn CullDistance
// CHECK: OpDecorateStringGOOGLE %gl_CullDistance_0 HlslSemanticGOOGLE "SV_CullDistance"

// CHECK: OpDecorate %gl_Position BuiltIn Position
// CHECK: OpDecorateStringGOOGLE %gl_Position HlslSemanticGOOGLE "SV_Position"
// CHECK: OpDecorateStringGOOGLE %in_var_BAZ HlslSemanticGOOGLE "BAZ"
// CHECK: OpDecorate %gl_PointSize BuiltIn PointSize
// CHECK: OpDecorateStringGOOGLE %gl_PointSize HlslSemanticGOOGLE "PSIZE"
// CHECK: OpDecorate %gl_InvocationID BuiltIn InvocationId
// CHECK: OpDecorateStringGOOGLE %gl_InvocationID HlslSemanticGOOGLE "SV_OutputControlPointID"
// CHECK: OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
// CHECK: OpDecorateStringGOOGLE %gl_PrimitiveID HlslSemanticGOOGLE "SV_PrimitiveID"

// CHECK: OpDecorate %gl_Position_0 BuiltIn Position
// CHECK: OpDecorateStringGOOGLE %gl_Position_0 HlslSemanticGOOGLE "SV_Position"
// CHECK: OpDecorateStringGOOGLE %out_var_FOO HlslSemanticGOOGLE "FOO"
// CHECK: OpDecorate %gl_PointSize_0 BuiltIn PointSize
// CHECK: OpDecorateStringGOOGLE %gl_PointSize_0 HlslSemanticGOOGLE "PSIZE"
// CHECK: OpDecorateStringGOOGLE %out_var_BAR HlslSemanticGOOGLE "BAR"
// CHECK: OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter
// CHECK: OpDecorateStringGOOGLE %gl_TessLevelOuter HlslSemanticGOOGLE "SV_TessFactor"
// CHECK: OpDecorate %gl_TessLevelOuter Patch
// CHECK: OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner
// CHECK: OpDecorateStringGOOGLE %gl_TessLevelInner HlslSemanticGOOGLE "SV_InsideTessFactor"
// CHECK: OpDecorate %gl_TessLevelInner Patch
// CHECK: OpDecorateStringGOOGLE %out_var_TEXCOORD HlslSemanticGOOGLE "TEXCOORD"
// CHECK: OpDecorate %out_var_TEXCOORD Patch
// CHECK: OpDecorateStringGOOGLE %out_var_WEIGHT HlslSemanticGOOGLE "WEIGHT"
// CHECK: OpDecorate %out_var_WEIGHT Patch
// CHECK: OpDecorate %in_var_BAZ Location 0
// CHECK: OpDecorate %out_var_BAR Location 0
// CHECK: OpDecorate %out_var_FOO Location 1
// CHECK: OpDecorate %out_var_TEXCOORD Location 2
// CHECK: OpDecorate %out_var_WEIGHT Location 6

// Input : clip0 + clip2         : 3 floats
// Input : cull3 + cull5         : 4 floats

// Output: clip6 + clip7 + clip8 : 3 floats
// Output: cull6 + cull9         : 5 floats

// CHECK:   %gl_ClipDistance = OpVariable %_ptr_Input__arr__arr_float_uint_3_uint_2 Input
// CHECK:   %gl_CullDistance = OpVariable %_ptr_Input__arr__arr_float_uint_4_uint_2 Input
// CHECK: %gl_ClipDistance_0 = OpVariable %_ptr_Output__arr__arr_float_uint_3_uint_2 Output
// CHECK: %gl_CullDistance_0 = OpVariable %_ptr_Output__arr__arr_float_uint_5_uint_2 Output

// CHECK:       %gl_Position = OpVariable %_ptr_Input__arr_v4float_uint_2 Input
// CHECK:        %in_var_BAZ = OpVariable %_ptr_Input__arr_v3float_uint_2 Input
// CHECK:      %gl_PointSize = OpVariable %_ptr_Input__arr_float_uint_2 Input
// CHECK:   %gl_InvocationID = OpVariable %_ptr_Input_uint Input
// CHECK:    %gl_PrimitiveID = OpVariable %_ptr_Input_uint Input

// CHECK:     %gl_Position_0 = OpVariable %_ptr_Output__arr_v4float_uint_2 Output
// CHECK:       %out_var_FOO = OpVariable %_ptr_Output__arr_v3float_uint_2 Output
// CHECK:    %gl_PointSize_0 = OpVariable %_ptr_Output__arr_float_uint_2 Output
// CHECK:       %out_var_BAR = OpVariable %_ptr_Output__arr_v4float_uint_2 Output
// CHECK: %gl_TessLevelOuter = OpVariable %_ptr_Output__arr_float_uint_4 Output
// CHECK: %gl_TessLevelInner = OpVariable %_ptr_Output__arr_float_uint_2 Output
// CHECK:  %out_var_TEXCOORD = OpVariable %_ptr_Output__arr_v2float_uint_4 Output
// CHECK:    %out_var_WEIGHT = OpVariable %_ptr_Output_v4float Output

// Patch Constant Function
HsPcfOut pcf(InputPatch<HsCpIn, NumOutPoints> patch, uint patchId : SV_PrimitiveID) {
  HsPcfOut output;
  output = (HsPcfOut)0;
  return output;
}

// Layout of input ClipDistance array:
//   clip0: 2 floats, offset 0
//   clip2: 1 floats, offset 2

// Layout of input CullDistance array:
//   cull3: 3 floats, offset 0
//   cull5: 1 floats, offset 3

// Layout of output ClipDistance array:
//   clip6: 1 floats, offset 0
//   clip7: 1 floats, offset 1
//   clip8: 1 floats, offset 2

// Layout of output CullDistance array:
//   cull6: 2 floats, offset 0
//   cull9: 3 floats, offset 2

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(NumOutPoints)]
[patchconstantfunc("pcf")]
HsCpOut main(InputPatch<HsCpIn, NumOutPoints> patch, uint cpId : SV_OutputControlPointID, uint patchId : SV_PrimitiveID) {
    HsCpOut output;
    output = (HsCpOut)0;
    output.clip7 = patch[0].pos.x + cpId + patchId;
    return output;

// Read gl_Postion for HsCpIn::pos

// CHECK:       [[inPosArr:%\d+]] = OpLoad %_arr_v4float_uint_2 %gl_Position

// Read gl_ClipDistance[] to compose a new array for HsCpIn::clip0

// CHECK-NEXT:      [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0 %uint_0
// CHECK-NEXT:      [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0 %uint_1
// CHECK-NEXT:      [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:    [[clip00:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// CHECK-NEXT:      [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_1 %uint_0
// CHECK-NEXT:      [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_1 %uint_1
// CHECK-NEXT:      [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:    [[clip01:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// CHECK-NEXT: [[inClip0Arr:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_2 [[clip00]] [[clip01]]

// Read gl_CullDistance[] to compose a new array for HsCpIn::cull5

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_3
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_3
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT: [[inCull5Arr:%\d+]] = OpCompositeConstruct %_arr_float_uint_2 [[val0]] [[val1]]

// Read gl_ClipDistance[] to compose a new array for HsCpIn::clip2

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0 %uint_2
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_1 %uint_2
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT: [[inClip2Arr:%\d+]] = OpCompositeConstruct %_arr_float_uint_2 [[val0]] [[val1]]

// Read gl_CullDistance[] to compose a new array for HsCpIn::cull3

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_2
// CHECK-NEXT:       [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:     [[cull30:%\d+]] = OpCompositeConstruct %v3float [[val0]] [[val1]] [[val2]]

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_2
// CHECK-NEXT:       [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:     [[cull31:%\d+]] = OpCompositeConstruct %v3float [[val0]] [[val1]] [[val2]]

// CHECK-NEXT: [[inCull3Arr:%\d+]] = OpCompositeConstruct %_arr_v3float_uint_2 [[cull30]] [[cull31]]

// CHECK-NEXT:   [[inBazArr:%\d+]] = OpLoad %_arr_v3float_uint_2 %in_var_BAZ

// Read gl_PointSize[] for HsCpIn::ptSize
// CHECK-NEXT:  [[inPtSzArr:%\d+]] = OpLoad %_arr_float_uint_2 %gl_PointSize

// Compose a temporary HsCpIn value out of the temporary arrays constructed before
// CHECK-NEXT:       [[val0:%\d+]] = OpCompositeExtract %v4float [[inPosArr]] 0
// CHECK-NEXT:       [[val1:%\d+]] = OpCompositeExtract %v2float [[inClip0Arr]] 0
// CHECK-NEXT:       [[val2:%\d+]] = OpCompositeExtract %float [[inCull5Arr]] 0
// CHECK-NEXT:       [[val3:%\d+]] = OpCompositeExtract %float [[inClip2Arr]] 0
// CHECK-NEXT:       [[val4:%\d+]] = OpCompositeExtract %v3float [[inCull3Arr]] 0
// CHECK-NEXT:       [[val5:%\d+]] = OpCompositeExtract %v3float [[inBazArr]] 0
// CHECK-NEXT:       [[val6:%\d+]] = OpCompositeExtract %float [[inPtSzArr]] 0
// CHECK-NEXT:    [[hscpin0:%\d+]] = OpCompositeConstruct %HsCpIn [[val0]] [[val1]] [[val2]] [[val3]] [[val4]] [[val5]] [[val6]]

// Compose a temporary HsCpIn value out of the temporary arrays constructed before
// CHECK-NEXT:       [[val0:%\d+]] = OpCompositeExtract %v4float [[inPosArr]] 1
// CHECK-NEXT:       [[val1:%\d+]] = OpCompositeExtract %v2float [[inClip0Arr]] 1
// CHECK-NEXT:       [[val2:%\d+]] = OpCompositeExtract %float [[inCull5Arr]] 1
// CHECK-NEXT:       [[val3:%\d+]] = OpCompositeExtract %float [[inClip2Arr]] 1
// CHECK-NEXT:       [[val4:%\d+]] = OpCompositeExtract %v3float [[inCull3Arr]] 1
// CHECK-NEXT:       [[val5:%\d+]] = OpCompositeExtract %v3float [[inBazArr]] 1
// CHECK-NEXT:       [[val6:%\d+]] = OpCompositeExtract %float [[inPtSzArr]] 1
// CHECK-NEXT:    [[hscpin1:%\d+]] = OpCompositeConstruct %HsCpIn [[val0]] [[val1]] [[val2]] [[val3]] [[val4]] [[val5]] [[val6]]

// Populate the temporary variables for function call

// CHECK-NEXT:      [[patch:%\d+]] = OpCompositeConstruct %_arr_HsCpIn_uint_2 [[hscpin0]] [[hscpin1]]
// CHECK-NEXT:                       OpStore %param_var_patch [[patch]]

// CHECK-NEXT:     [[invoId:%\d+]] = OpLoad %uint %gl_InvocationID
// CHECK-NEXT:                       OpStore %param_var_cpId [[invoId]]

// CHECK-NEXT:     [[primId:%\d+]] = OpLoad %uint %gl_PrimitiveID
// CHECK-NEXT:                       OpStore %param_var_patchId [[primId]]

// CHECK-NEXT:        [[ret:%\d+]] = OpFunctionCall %HsCpOut %src_main %param_var_patch %param_var_cpId %param_var_patchId

// Write out HsCpOut::cull9 into gl_CullDistance[]
// CHECK-NEXT:      [[cull9:%\d+]] = OpCompositeExtract %v3float [[ret]] 0
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 [[invoId]] %uint_2
// CHECK-NEXT:       [[val0:%\d+]] = OpCompositeExtract %float [[cull9]] 0
// CHECK-NEXT:                       OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 [[invoId]] %uint_3
// CHECK-NEXT:       [[val1:%\d+]] = OpCompositeExtract %float [[cull9]] 1
// CHECK-NEXT:                       OpStore [[ptr1]] [[val1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 [[invoId]] %uint_4
// CHECK-NEXT:       [[val2:%\d+]] = OpCompositeExtract %float [[cull9]] 2
// CHECK-NEXT:                       OpStore [[ptr2]] [[val2]]

// CHECK-NEXT:  [[outInner1:%\d+]] = OpCompositeExtract %CpInner1 [[ret]] 1

// Write out HsCpOut::CpInner1::pos to gl_Position
// CHECK-NEXT:     [[outPos:%\d+]] = OpCompositeExtract %v4float [[outInner1]] 0
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v4float %gl_Position_0 [[invoId]]
// CHECK-NEXT:                      OpStore [[ptr]] [[outPos:%\d+]]

// Write out HsCpOut::CpInner1::CpInner2::clip8 to gl_ClipDistance
// CHECK-NEXT:  [[outInner2:%\d+]] = OpCompositeExtract %CpInner2 [[outInner1]] 1
// CHECK-NEXT:   [[outClip8:%\d+]] = OpCompositeExtract %float [[outInner2]] 0
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance_0 [[invoId]] %uint_2
// CHECK-NEXT:                       OpStore [[ptr]] [[outClip8]]

// Write out HsCpOut::CpInner1::CpInner2::cull6 to gl_CullDistance
// CHECK-NEXT:   [[outCull6:%\d+]] = OpCompositeExtract %v2float [[outInner2]] 1
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 [[invoId]] %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpCompositeExtract %float [[outCull6]] 0
// CHECK-NEXT:                       OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 [[invoId]] %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpCompositeExtract %float [[outCull6]] 1
// CHECK-NEXT:                       OpStore [[ptr1]] [[val1]]

// Write out HsCpOut::CpInner1::CpInner2::foo to out_var_FOO
// CHECK-NEXT:        [[foo:%\d+]] = OpCompositeExtract %v3float [[outInner2]] 2
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v3float %out_var_FOO [[invoId]]
// CHECK-NEXT:                       OpStore [[ptr]] [[foo]]

// Write out HsCpOut::CpInner1::CpInner2::PointSize to gl_PointSize
// CHECK-NEXT:     [[ptSize:%\d+]] = OpCompositeExtract %float [[outInner2]] 3
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PointSize_0 [[invoId]]
// CHECK-NEXT:                       OpStore [[ptr]] [[ptSize]]

// Write out HsCpOut::CpInner1::clip6 to gl_ClipDistance
// CHECK-NEXT:      [[clip6:%\d+]] = OpCompositeExtract %float [[outInner1]] 2
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance_0 [[invoId]] %uint_0
// CHECK-NEXT:                       OpStore [[ptr]] [[clip6]]

// Write out HsCpOut::CpInner1::bar to out_var_BAR
// CHECK-NEXT:        [[bar:%\d+]] = OpCompositeExtract %v4float [[outInner1]] 3
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v4float %out_var_BAR [[invoId]]
// CHECK-NEXT:                       OpStore [[ptr]] [[bar]]

// Write out HsCpOut::clip7 to gl_ClipDistance
// CHECK-NEXT:      [[clip7:%\d+]] = OpCompositeExtract %float [[ret]] 2
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance_0 [[invoId]] %uint_1
// CHECK-NEXT:                       OpStore [[ptr]] [[clip7]]

// Call PCF
// CHECK:             [[ret:%\d+]] = OpFunctionCall %HsPcfOut %pcf %param_var_patch %param_var_patchId

// Write out PCF output
// CHECK-NEXT:        [[tlo:%\d+]] = OpCompositeExtract %_arr_float_uint_4 [[ret]] 0
// CHECK-NEXT:                       OpStore %gl_TessLevelOuter [[tlo]]
// CHECK-NEXT:        [[tli:%\d+]] = OpCompositeExtract %_arr_float_uint_2 [[ret]] 1
// CHECK-NEXT:                       OpStore %gl_TessLevelInner [[tli]]
// CHECK-NEXT:    [[texcord:%\d+]] = OpCompositeExtract %_arr_v2float_uint_4 [[ret]] 2
// CHECK-NEXT:                       OpStore %out_var_TEXCOORD [[texcord]]
// CHECK-NEXT:     [[weight:%\d+]] = OpCompositeExtract %v4float [[ret]] 3
// CHECK-NEXT:                       OpStore %out_var_WEIGHT [[weight]]
}
