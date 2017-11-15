// Run: %dxc -T hs_6_0 -E main

#define NumOutPoints 2

// CHECK: OpCapability ClipDistance
// CHECK: OpCapability CullDistance
// CHECK: OpCapability Tessellation

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

// Per-vertex    input  builtin : gl_PerVertex (Position, ClipDistance, CullDistance), InvocationID
// Per-vertex    output builtin : gl_PerVertex (Position, ClipDistance, CullDistance)
// Per-vertex    input  variable: BAZ
// Per-vertex    output variable: FOO, BAR

// Per-primitive input  builtin : PrimitiveID
// Per-primitive output builtin : TessLevelInner, TessLevelOuter
// Per-primitive output variable: TEXCOORD, WEIGHT

// CHECK: OpEntryPoint TessellationControl %main "main" %gl_PerVertexIn %gl_PerVertexOut %in_var_BAZ %gl_InvocationID %gl_PrimitiveID %out_var_FOO %out_var_BAR %gl_TessLevelOuter %gl_TessLevelInner %out_var_TEXCOORD %out_var_WEIGHT

// CHECK: OpMemberDecorate %type_gl_PerVertex 0 BuiltIn Position
// CHECK: OpMemberDecorate %type_gl_PerVertex 1 BuiltIn PointSize
// CHECK: OpMemberDecorate %type_gl_PerVertex 2 BuiltIn ClipDistance
// CHECK: OpMemberDecorate %type_gl_PerVertex 3 BuiltIn CullDistance
// CHECK: OpDecorate %type_gl_PerVertex Block

// CHECK: OpMemberDecorate %type_gl_PerVertex_0 0 BuiltIn Position
// CHECK: OpMemberDecorate %type_gl_PerVertex_0 1 BuiltIn PointSize
// CHECK: OpMemberDecorate %type_gl_PerVertex_0 2 BuiltIn ClipDistance
// CHECK: OpMemberDecorate %type_gl_PerVertex_0 3 BuiltIn CullDistance
// CHECK: OpDecorate %type_gl_PerVertex_0 Block

// CHECK: OpDecorate %gl_InvocationID BuiltIn InvocationId
// CHECK: OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
// CHECK: OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter
// CHECK: OpDecorate %gl_TessLevelOuter Patch
// CHECK: OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner
// CHECK: OpDecorate %gl_TessLevelInner Patch
// CHECK: OpDecorate %out_var_TEXCOORD Patch
// CHECK: OpDecorate %out_var_WEIGHT Patch
// CHECK: OpDecorate %in_var_BAZ Location 0
// CHECK: OpDecorate %out_var_BAR Location 0
// CHECK: OpDecorate %out_var_FOO Location 1
// CHECK: OpDecorate %out_var_TEXCOORD Location 2
// CHECK: OpDecorate %out_var_WEIGHT Location 3

// Input : clip0 + clip2         : 3 floats
// Input : cull3 + cull5         : 4 floats
// CHECK:   %type_gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_3 %_arr_float_uint_4

// Output: clip6 + clip7 + clip8 : 3 floats
// Output: cull6 + cull9         : 5 floats
// CHECK: %type_gl_PerVertex_0 = OpTypeStruct %v4float %float %_arr_float_uint_3 %_arr_float_uint_5

// CHECK:    %gl_PerVertexIn = OpVariable %_ptr_Input__arr_type_gl_PerVertex_uint_2 Input
// CHECK:   %gl_PerVertexOut = OpVariable %_ptr_Output__arr_type_gl_PerVertex_0_uint_2 Output

// CHECK:        %in_var_BAZ = OpVariable %_ptr_Input__arr_v3float_uint_2 Input
// CHECK:   %gl_InvocationID = OpVariable %_ptr_Input_uint Input
// CHECK:    %gl_PrimitiveID = OpVariable %_ptr_Input_uint Input
// CHECK:       %out_var_FOO = OpVariable %_ptr_Output__arr_v3float_uint_2 Output
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
    return output;

// Read gl_PerVertex[].gl_Postion and compose a new array for HsCpIn::pos

// CHECK:           [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_v4float %gl_PerVertexIn %uint_0 %uint_0
// CHECK-NEXT:      [[val0:%\d+]] = OpLoad %v4float [[ptr0]]
// CHECK-NEXT:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_v4float %gl_PerVertexIn %uint_1 %uint_0
// CHECK-NEXT:      [[val1:%\d+]] = OpLoad %v4float [[ptr1]]
// CHECK-NEXT:  [[inPosArr:%\d+]] = OpCompositeConstruct %_arr_v4float_uint_2 [[val0]] [[val1]]

// Read gl_PerVertex[].gl_ClipDistance[] to compose a new array for HsCpIn::clip0

// CHECK-NEXT:      [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_0
// CHECK-NEXT:      [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_1
// CHECK-NEXT:      [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:    [[clip00:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// CHECK-NEXT:      [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_0
// CHECK-NEXT:      [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_1
// CHECK-NEXT:      [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:    [[clip01:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// CHECK-NEXT: [[inClip0Arr:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_2 [[clip00]] [[clip01]]

// Read gl_PerVertex[].gl_CullDistance[] to compose a new array for HsCpIn::cull5

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_3
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_3
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT: [[inCull5Arr:%\d+]] = OpCompositeConstruct %_arr_float_uint_2 [[val0]] [[val1]]

// Read gl_PerVertex[].gl_ClipDistance[] to compose a new array for HsCpIn::clip2

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_2
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_2
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT: [[inClip2Arr:%\d+]] = OpCompositeConstruct %_arr_float_uint_2 [[val0]] [[val1]]

// Read gl_PerVertex[].gl_CullDistance[] to compose a new array for HsCpIn::cull3

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_2
// CHECK-NEXT:       [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:     [[cull30:%\d+]] = OpCompositeConstruct %v3float [[val0]] [[val1]] [[val2]]

// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_2
// CHECK-NEXT:       [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:     [[cull31:%\d+]] = OpCompositeConstruct %v3float [[val0]] [[val1]] [[val2]]

// CHECK-NEXT: [[inCull3Arr:%\d+]] = OpCompositeConstruct %_arr_v3float_uint_2 [[cull30]] [[cull31]]

// CHECK-NEXT:   [[inBazArr:%\d+]] = OpLoad %_arr_v3float_uint_2 %in_var_BAZ

// Read gl_PerVertex[].gl_PointSize[] to compose a new array for HsCpIn::ptSize
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_1
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:  [[inPtSzArr:%\d+]] = OpCompositeConstruct %_arr_float_uint_2 [[val0]] [[val1]]

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

// Write out HsCpOut::cull9 into gl_PerVertex[].gl_CullDistance[]
// CHECK-NEXT:      [[cull9:%\d+]] = OpCompositeExtract %v3float [[ret]] 0
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_3 %uint_2
// CHECK-NEXT:       [[val0:%\d+]] = OpCompositeExtract %float [[cull9]] 0
// CHECK-NEXT:                       OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_3 %uint_3
// CHECK-NEXT:       [[val1:%\d+]] = OpCompositeExtract %float [[cull9]] 1
// CHECK-NEXT:                       OpStore [[ptr1]] [[val1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_3 %uint_4
// CHECK-NEXT:       [[val2:%\d+]] = OpCompositeExtract %float [[cull9]] 2
// CHECK-NEXT:                       OpStore [[ptr2]] [[val2]]

// CHECK-NEXT:  [[outInner1:%\d+]] = OpCompositeExtract %CpInner1 [[ret]] 1

// Write out HsCpOut::CpInner1::pos to gl_PerVertex[].gl_Position
// CHECK-NEXT:     [[outPos:%\d+]] = OpCompositeExtract %v4float [[outInner1]] 0
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v4float %gl_PerVertexOut [[invoId]] %uint_0
// CHECK-NEXT:                      OpStore [[ptr]] [[outPos:%\d+]]

// Write out HsCpOut::CpInner1::CpInner2::clip8 to gl_PerVertex[].gl_ClipDistance
// CHECK-NEXT:  [[outInner2:%\d+]] = OpCompositeExtract %CpInner2 [[outInner1]] 1
// CHECK-NEXT:   [[outClip8:%\d+]] = OpCompositeExtract %float [[outInner2]] 0
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_2 %uint_2
// CHECK-NEXT:                       OpStore [[ptr]] [[outClip8]]

// Write out HsCpOut::CpInner1::CpInner2::cull6 to gl_PerVertex[].gl_CullDistance
// CHECK-NEXT:   [[outCull6:%\d+]] = OpCompositeExtract %v2float [[outInner2]] 1
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_3 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpCompositeExtract %float [[outCull6]] 0
// CHECK-NEXT:                       OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_3 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpCompositeExtract %float [[outCull6]] 1
// CHECK-NEXT:                       OpStore [[ptr1]] [[val1]]

// Write out HsCpOut::CpInner1::CpInner2::foo to out_var_FOO
// CHECK-NEXT:        [[foo:%\d+]] = OpCompositeExtract %v3float [[outInner2]] 2
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v3float %out_var_FOO [[invoId]]
// CHECK-NEXT:                       OpStore [[ptr]] [[foo]]

// Write out HsCpOut::CpInner1::CpInner2::PointSize to gl_PerVertex[].gl_PointSize
// CHECK-NEXT:     [[ptSize:%\d+]] = OpCompositeExtract %float [[outInner2]] 3
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_1
// CHECK-NEXT:                       OpStore [[ptr]] [[ptSize]]

// Write out HsCpOut::CpInner1::clip6 to gl_PerVertex[].gl_ClipDistance
// CHECK-NEXT:      [[clip6:%\d+]] = OpCompositeExtract %float [[outInner1]] 2
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_2 %uint_0
// CHECK-NEXT:                       OpStore [[ptr]] [[clip6]]

// Write out HsCpOut::CpInner1::bar to out_var_BAR
// CHECK-NEXT:        [[bar:%\d+]] = OpCompositeExtract %v4float [[outInner1]] 3
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v4float %out_var_BAR [[invoId]]
// CHECK-NEXT:                       OpStore [[ptr]] [[bar]]

// Write out HsCpOut::clip7 to gl_PerVertex[].gl_ClipDistance
// CHECK-NEXT:      [[clip7:%\d+]] = OpCompositeExtract %float [[ret]] 2
// CHECK-NEXT:        [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut [[invoId]] %uint_2 %uint_1
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
