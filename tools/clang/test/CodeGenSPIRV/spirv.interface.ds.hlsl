// Run: %dxc -T ds_6_0 -E main -fspv-reflect

// CHECK: OpCapability Tessellation
// CHECK: OpCapability ClipDistance
// CHECK: OpCapability CullDistance

// CHECK: OpExtension "SPV_GOOGLE_hlsl_functionality1"

// HS PCF output

struct HsPcfOut {
  float  outTessFactor[4]   : SV_TessFactor;        // Builtin TessLevelOuter
  float  inTessFactor[2]    : SV_InsideTessFactor;  // Builtin TessLevelInner

  float3 foo                : FOO;                  // Input variable
};

// Per-vertex input structs

struct Inner2PerVertexIn {
  float               clip3 : SV_ClipDistance3;     // Builtin ClipDistance
  float4              texco : TEXCOORD;             // Input variable
};

struct InnerPerVertexIn {
  float4              pos   : SV_Position;          // Builtin Position
  float               clip0 : SV_ClipDistance0;     // Builtin ClipDistance
  Inner2PerVertexIn   s;
  float2              cull2 : SV_CullDistance2;     // Builtin CullDistance
};

struct PerVertexIn {
  float4              cull3 : SV_CullDistance3;     // Builtin CullDistance
  InnerPerVertexIn    s;
  float2              bar   : BAR;                  // Input variable
  [[vk::builtin("PointSize")]]
  float  ptSize             : PSIZE;                // Builtin PointSize
};

// Per-vertex output structs

struct Inner2PerVertexOut {
  float3 foo                : FOO;                  // Output variable
  float2 cull3              : SV_CullDistance3;     // Builtin CullDistance
  float  clip0              : SV_ClipDistance0;     // Builtin ClipDistance
};

struct InnerPerVertexOut {
  Inner2PerVertexOut s;
  float2 cull4              : SV_CullDistance4;     // Builtin CullDistance
  float4 bar                : BAR;                  // Output variable
  [[vk::builtin("PointSize")]]
  float  ptSize             : PSIZE;                // Builtin PointSize
};

struct DsOut {
  float4 pos                : SV_Position;
  InnerPerVertexOut s;
};

// Per-vertex    input  builtin : Position, PointSize, ClipDistance, CullDistance
// Per-vertex    output builtin : Position, PointSize, ClipDistance, CullDistance
// Per-vertex    input  variable: TEXCOORD, BAR
// Per-vertex    output variable: FOO, BAR

// Per-primitive input builtin  : TessLevelInner, TessLevelOuter, TessCoord (SV_DomainLocation)
// Per-primitive input variable : FOO

// CHECK: OpEntryPoint TessellationEvaluation %main "main" %gl_ClipDistance %gl_CullDistance %gl_ClipDistance_0 %gl_CullDistance_0 %gl_Position %in_var_TEXCOORD %in_var_BAR %gl_PointSize %gl_TessCoord %gl_TessLevelOuter %gl_TessLevelInner %in_var_FOO %gl_Position_0 %out_var_FOO %out_var_BAR %gl_PointSize_0

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
// CHECK: OpDecorateStringGOOGLE %in_var_TEXCOORD HlslSemanticGOOGLE "TEXCOORD"
// CHECK: OpDecorateStringGOOGLE %in_var_BAR HlslSemanticGOOGLE "BAR"
// CHECK: OpDecorate %gl_PointSize BuiltIn PointSize
// CHECK: OpDecorateStringGOOGLE %gl_PointSize HlslSemanticGOOGLE "PSIZE"
// CHECK: OpDecorate %gl_TessCoord BuiltIn TessCoord
// CHECK: OpDecorateStringGOOGLE %gl_TessCoord HlslSemanticGOOGLE "SV_DomainLocation"
// CHECK: OpDecorate %gl_TessCoord Patch
// CHECK: OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter
// CHECK: OpDecorateStringGOOGLE %gl_TessLevelOuter HlslSemanticGOOGLE "SV_TessFactor"
// CHECK: OpDecorate %gl_TessLevelOuter Patch
// CHECK: OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner
// CHECK: OpDecorateStringGOOGLE %gl_TessLevelInner HlslSemanticGOOGLE "SV_InsideTessFactor"
// CHECK: OpDecorate %gl_TessLevelInner Patch
// CHECK: OpDecorateStringGOOGLE %in_var_FOO HlslSemanticGOOGLE "FOO"
// CHECK: OpDecorate %in_var_FOO Patch

// CHECK: OpDecorate %gl_Position_0 BuiltIn Position
// CHECK: OpDecorateStringGOOGLE %gl_Position_0 HlslSemanticGOOGLE "SV_Position"
// CHECK: OpDecorateStringGOOGLE %out_var_FOO HlslSemanticGOOGLE "FOO"
// CHECK: OpDecorateStringGOOGLE %out_var_BAR HlslSemanticGOOGLE "BAR"
// CHECK: OpDecorate %gl_PointSize_0 BuiltIn PointSize
// CHECK: OpDecorateStringGOOGLE %gl_PointSize_0 HlslSemanticGOOGLE "PSIZE"
// CHECK: OpDecorate %in_var_BAR Location 0
// CHECK: OpDecorate %in_var_FOO Location 1
// CHECK: OpDecorate %in_var_TEXCOORD Location 2
// CHECK: OpDecorate %out_var_FOO Location 0
// CHECK: OpDecorate %out_var_BAR Location 1

// Input : clip0 + clip3 : 2 floats
// Input : cull2 + cull3 : 6 floats

// Output: clip0 + clip5 : 4 floats
// Output: cull3 + cull4 : 4 floats

// CHECK:   %gl_ClipDistance = OpVariable %_ptr_Input__arr__arr_float_uint_2_uint_3 Input
// CHECK:   %gl_CullDistance = OpVariable %_ptr_Input__arr__arr_float_uint_6_uint_3 Input
// CHECK: %gl_ClipDistance_0 = OpVariable %_ptr_Output__arr_float_uint_4 Output
// CHECK: %gl_CullDistance_0 = OpVariable %_ptr_Output__arr_float_uint_4 Output

// CHECK:       %gl_Position = OpVariable %_ptr_Input__arr_v4float_uint_3 Input
// CHECK:   %in_var_TEXCOORD = OpVariable %_ptr_Input__arr_v4float_uint_3 Input
// CHECK:        %in_var_BAR = OpVariable %_ptr_Input__arr_v2float_uint_3 Input
// CHECK:      %gl_PointSize = OpVariable %_ptr_Input__arr_float_uint_3 Input
// CHECK:      %gl_TessCoord = OpVariable %_ptr_Input_v3float Input
// CHECK: %gl_TessLevelOuter = OpVariable %_ptr_Input__arr_float_uint_4 Input
// CHECK: %gl_TessLevelInner = OpVariable %_ptr_Input__arr_float_uint_2 Input
// CHECK:        %in_var_FOO = OpVariable %_ptr_Input_v3float Input

// CHECK:     %gl_Position_0 = OpVariable %_ptr_Output_v4float Output
// CHECK:       %out_var_FOO = OpVariable %_ptr_Output_v3float Output
// CHECK:       %out_var_BAR = OpVariable %_ptr_Output_v4float Output
// CHECK:    %gl_PointSize_0 = OpVariable %_ptr_Output_float Output

[domain("quad")]
DsOut main(    const OutputPatch<PerVertexIn, 3> patch,
               float2   loc     : SV_DomainLocation,
               HsPcfOut pcfData,
           out float3   clip5   : SV_ClipDistance5)     // Builtin ClipDistance
{
  DsOut dsOut;
  dsOut = (DsOut)0;
  dsOut.pos = patch[0].ptSize;
  clip5 = pcfData.foo + float3(loc, 1);
  return dsOut;
// Layout of input ClipDistance array:
//   clip0: 1 floats, offset 0
//   clip3: 1 floats, offset 1

// Layout of input CullDistance array:
//   cull2: 2 floats, offset 0
//   cull3: 4 floats, offset 2

// Layout of output ClipDistance array:
//   clip0: 1 floats, offset 0
//   clip5: 3 floats, offset 1

// Layout of output CullDistance array:
//   cull3: 2 floats, offset 0
//   cull4: 2 floats, offset 2

// Read gl_CullDistance[0] and compose patch[0].cull3 (SV_CullDistance3)
// CHECK:              [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_4
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:         [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_5
// CHECK-NEXT:         [[val3:%\d+]] = OpLoad %float [[ptr3]]
// CHECK-NEXT:  [[patch0cull3:%\d+]] = OpCompositeConstruct %v4float [[val0]] [[val1]] [[val2]] [[val3]]

// Read gl_CullDistance[1] and compose patch[1].cull3 (SV_CullDistance3)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_4
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:         [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_5
// CHECK-NEXT:         [[val3:%\d+]] = OpLoad %float [[ptr3]]
// CHECK-NEXT:  [[patch1cull3:%\d+]] = OpCompositeConstruct %v4float [[val0]] [[val1]] [[val2]] [[val3]]

// Read gl_CullDistance[2] and compose patch[2].cull3 (SV_CullDistance3)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_2 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_2 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_2 %uint_4
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:         [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_2 %uint_5
// CHECK-NEXT:         [[val3:%\d+]] = OpLoad %float [[ptr3]]
// CHECK-NEXT:  [[patch2cull3:%\d+]] = OpCompositeConstruct %v4float [[val0]] [[val1]] [[val2]] [[val3]]

// Compose an array of input SV_CullDistance3 for later use
// CHECK-NEXT:   [[inCull3Arr:%\d+]] = OpCompositeConstruct %_arr_v4float_uint_3 [[patch0cull3]] [[patch1cull3]] [[patch2cull3]]

// Read gl_Position
// CHECK-NEXT:     [[inPosArr:%\d+]] = OpLoad %_arr_v4float_uint_3 %gl_Position

// Read gl_ClipDistance[0] as patch[0].s.clip0 (SV_ClipDistance0)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]

// Read gl_ClipDistance[1] as patch[1].s.clip0 (SV_ClipDistance0)
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_1 %uint_0
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]

// Read gl_ClipDistance[2] as patch[2].s.clip0 (SV_ClipDistance0)
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_2 %uint_0
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]

// Compose an array of input SV_ClipDistance0 for later use
// CHECK-NEXT:   [[inClip0Arr:%\d+]] = OpCompositeConstruct %_arr_float_uint_3 [[val0]] [[val1]] [[val2]]

// Read gl_ClipDistance[0] as patch[0].s.s.clip3 (SV_ClipDistance3)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0 %uint_1
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]

// Read gl_ClipDistance[1] as patch[1].s.s.clip3 (SV_ClipDistance3)
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_1 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]

// Read gl_ClipDistance[2] as patch[2].s.s.clip3 (SV_ClipDistance3)
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_2 %uint_1
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]

// Compose an array of input SV_ClipDistance3 for later use
// CHECK-NEXT:   [[inClip3Arr:%\d+]] = OpCompositeConstruct %_arr_float_uint_3 [[val0]] [[val1]] [[val2]]

// CHECK-NEXT:      [[texcord:%\d+]] = OpLoad %_arr_v4float_uint_3 %in_var_TEXCOORD

// Decompose temporary arrays created before to compose Inner2PerVertexIn

// CHECK-NEXT:  [[inClip3Arr0:%\d+]] = OpCompositeExtract %float [[inClip3Arr]] 0
// CHECK-NEXT:     [[texcord0:%\d+]] = OpCompositeExtract %v4float [[texcord]] 0
// CHECK-NEXT:         [[val0:%\d+]] = OpCompositeConstruct %Inner2PerVertexIn [[inClip3Arr0]] [[texcord0]]

// CHECK-NEXT:  [[inClip3Arr1:%\d+]] = OpCompositeExtract %float [[inClip3Arr]] 1
// CHECK-NEXT:     [[texcord1:%\d+]] = OpCompositeExtract %v4float [[texcord]] 1
// CHECK-NEXT:         [[val1:%\d+]] = OpCompositeConstruct %Inner2PerVertexIn [[inClip3Arr1]] [[texcord1]]

// CHECK-NEXT:  [[inClip3Arr2:%\d+]] = OpCompositeExtract %float [[inClip3Arr]] 2
// CHECK-NEXT:     [[texcord2:%\d+]] = OpCompositeExtract %v4float [[texcord]] 2
// CHECK-NEXT:         [[val2:%\d+]] = OpCompositeConstruct %Inner2PerVertexIn [[inClip3Arr2]] [[texcord2]]

// Compose an array of input Inner2PerVertexIn for later use
// CHECK-NEXT:   [[inIn2PVArr:%\d+]] = OpCompositeConstruct %_arr_Inner2PerVertexIn_uint_3 [[val0]] [[val1]] [[val2]]

// Read gl_CullDistance[0] as patch[0].s.cull2 (SV_CullDistance2)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_0 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:  [[patch0cull2:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// Read gl_CullDistance[1] as patch[1].s.cull2 (SV_CullDistance2)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_1 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:  [[patch1cull2:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// Read gl_CullDistance[2] as patch[2].s.cull2 (SV_CullDistance2)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_2 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_CullDistance %uint_2 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:  [[patch2cull2:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// Compose an array of input SV_CullDistance2 for later use
// CHECK-NEXT:   [[inCull2Arr:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_3 [[patch0cull2]] [[patch1cull2]] [[patch2cull2]]

// Decompose temporary arrays created before to compose InnerPerVertexIn

// CHECK-NEXT:       [[field0:%\d+]] = OpCompositeExtract %v4float [[inPosArr]] 0
// CHECK-NEXT:       [[field1:%\d+]] = OpCompositeExtract %float [[inClip0Arr]] 0
// CHECK-NEXT:       [[field2:%\d+]] = OpCompositeExtract %Inner2PerVertexIn [[inIn2PVArr]] 0
// CHECK-NEXT:       [[field3:%\d+]] = OpCompositeExtract %v2float [[inCull2Arr]] 0
// CHECK-NEXT:         [[val0:%\d+]] = OpCompositeConstruct %InnerPerVertexIn [[field0]] [[field1]] [[field2]] [[field3]]

// CHECK-NEXT:       [[field0:%\d+]] = OpCompositeExtract %v4float [[inPosArr]] 1
// CHECK-NEXT:       [[field1:%\d+]] = OpCompositeExtract %float [[inClip0Arr]] 1
// CHECK-NEXT:       [[field2:%\d+]] = OpCompositeExtract %Inner2PerVertexIn [[inIn2PVArr]] 1
// CHECK-NEXT:       [[field3:%\d+]] = OpCompositeExtract %v2float [[inCull2Arr]] 1
// CHECK-NEXT:         [[val1:%\d+]] = OpCompositeConstruct %InnerPerVertexIn [[field0]] [[field1]] [[field2]] [[field3]]

// CHECK-NEXT:       [[field0:%\d+]] = OpCompositeExtract %v4float [[inPosArr]] 2
// CHECK-NEXT:       [[field1:%\d+]] = OpCompositeExtract %float [[inClip0Arr]] 2
// CHECK-NEXT:       [[field2:%\d+]] = OpCompositeExtract %Inner2PerVertexIn [[inIn2PVArr]] 2
// CHECK-NEXT:       [[field3:%\d+]] = OpCompositeExtract %v2float [[inCull2Arr]] 2
// CHECK-NEXT:         [[val2:%\d+]] = OpCompositeConstruct %InnerPerVertexIn [[field0]] [[field1]] [[field2]] [[field3]]

// Compose an array of input InnerPerVertexIn for later use
// CHECK-NEXT:    [[inInPVArr:%\d+]] = OpCompositeConstruct %_arr_InnerPerVertexIn_uint_3 [[val0]] [[val1]] [[val2]]

// CHECK-NEXT:     [[inBarArr:%\d+]] = OpLoad %_arr_v2float_uint_3 %in_var_BAR

// Read gl_PointSize
// CHECK-NEXT:  [[inPtSizeArr:%\d+]] = OpLoad %_arr_float_uint_3 %gl_PointSize

// Decompose temporary arrays created before to compose PerVertexIn

// CHECK-NEXT:       [[field0:%\d+]] = OpCompositeExtract %v4float [[inCull3Arr]] 0
// CHECK-NEXT:       [[field1:%\d+]] = OpCompositeExtract %InnerPerVertexIn [[inInPVArr]] 0
// CHECK-NEXT:       [[field2:%\d+]] = OpCompositeExtract %v2float [[inBarArr]] 0
// CHECK-NEXT:       [[field3:%\d+]] = OpCompositeExtract %float [[inPtSizeArr]] 0
// CHECK-NEXT:         [[val0:%\d+]] = OpCompositeConstruct %PerVertexIn [[field0]] [[field1]] [[field2]] [[field3]]

// CHECK-NEXT:       [[field0:%\d+]] = OpCompositeExtract %v4float [[inCull3Arr]] 1
// CHECK-NEXT:       [[field1:%\d+]] = OpCompositeExtract %InnerPerVertexIn [[inInPVArr]] 1
// CHECK-NEXT:       [[field2:%\d+]] = OpCompositeExtract %v2float [[inBarArr]] 1
// CHECK-NEXT:       [[field3:%\d+]] = OpCompositeExtract %float [[inPtSizeArr]] 1
// CHECK-NEXT:         [[val1:%\d+]] = OpCompositeConstruct %PerVertexIn [[field0]] [[field1]] [[field2]] [[field3]]

// CHECK-NEXT:       [[field0:%\d+]] = OpCompositeExtract %v4float [[inCull3Arr]] 2
// CHECK-NEXT:       [[field1:%\d+]] = OpCompositeExtract %InnerPerVertexIn [[inInPVArr]] 2
// CHECK-NEXT:       [[field2:%\d+]] = OpCompositeExtract %v2float [[inBarArr]] 2
// CHECK-NEXT:       [[field3:%\d+]] = OpCompositeExtract %float [[inPtSizeArr]] 2
// CHECK-NEXT:         [[val2:%\d+]] = OpCompositeConstruct %PerVertexIn [[field0]] [[field1]] [[field2]] [[field3]]

// The final value for the patch parameter!
// CHECK-NEXT:        [[patch:%\d+]] = OpCompositeConstruct %_arr_PerVertexIn_uint_3 [[val0]] [[val1]] [[val2]]
// CHECK-NEXT:                         OpStore %param_var_patch [[patch]]

// Write SV_DomainLocation to tempoary variable for function call
// CHECK-NEXT:    [[tesscoord:%\d+]] = OpLoad %v3float %gl_TessCoord
// CHECK-NEXT:      [[shuffle:%\d+]] = OpVectorShuffle %v2float [[tesscoord]] [[tesscoord]] 0 1
// CHECK-NEXT:                         OpStore %param_var_loc [[shuffle]]

// Compose pcfData and write to tempoary variable for function call
// CHECK-NEXT:          [[tlo:%\d+]] = OpLoad %_arr_float_uint_4 %gl_TessLevelOuter
// CHECK-NEXT:          [[tli:%\d+]] = OpLoad %_arr_float_uint_2 %gl_TessLevelInner
// CHECK-NEXT:        [[inFoo:%\d+]] = OpLoad %v3float %in_var_FOO
// CHECK-NEXT:      [[pcfData:%\d+]] = OpCompositeConstruct %HsPcfOut [[tlo]] [[tli]] [[inFoo]]
// CHECK-NEXT:                         OpStore %param_var_pcfData [[pcfData]]

// Make the call!
// CHECK-NEXT:          [[ret:%\d+]] = OpFunctionCall %DsOut %src_main %param_var_patch %param_var_loc %param_var_pcfData %param_var_clip5

// Decompose DsOut and write out output SV_Position
// CHECK-NEXT:       [[outPos:%\d+]] = OpCompositeExtract %v4float [[ret]] 0
// CHECK-NEXT:                         OpStore %gl_Position_0 [[outPos]]

// CHECK-NEXT:      [[outInPV:%\d+]] = OpCompositeExtract %InnerPerVertexOut [[ret]] 1
// CHECK-NEXT:     [[outIn2PV:%\d+]] = OpCompositeExtract %Inner2PerVertexOut [[outInPV]] 0

// Decompose Inner2PerVertexOut and write out DsOut.s.s.foo (FOO)
// CHECK-NEXT:          [[foo:%\d+]] = OpCompositeExtract %v3float [[outIn2PV]] 0
// CHECK-NEXT:                         OpStore %out_var_FOO [[foo]]

// Decompose Inner2PerVertexOut and write out DsOut.s.s.cull3 (SV_CullDistance3) at offset 0
// CHECK-NEXT:        [[cull3:%\d+]] = OpCompositeExtract %v2float [[outIn2PV]] 1
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpCompositeExtract %float [[cull3]] 0
// CHECK-NEXT:                         OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpCompositeExtract %float [[cull3]] 1
// CHECK-NEXT:                         OpStore [[ptr1]] [[val1]]

// Decompose Inner2PerVertexOut and write out DsOut.s.s.clip0 (SV_ClipDistance0) at offset 0
// CHECK-NEXT:        [[clip0:%\d+]] = OpCompositeExtract %float [[outIn2PV]] 2
// CHECK-NEXT:          [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance_0 %uint_0
// CHECK-NEXT:                         OpStore [[ptr]] [[clip0]]

// Decompose InnerPerVertexOut and write out DsOut.s.cull4 (SV_CullDistance4) at offset 2
// CHECK-NEXT:        [[cull4:%\d+]] = OpCompositeExtract %v2float [[outInPV]] 1
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpCompositeExtract %float [[cull4]] 0
// CHECK-NEXT:                         OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_CullDistance_0 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpCompositeExtract %float [[cull4]] 1
// CHECK-NEXT:                         OpStore [[ptr1]] [[val1]]

// Decompose InnerPerVertexOut and write out DsOut.s.bar (BAR)
// CHECK-NEXT:          [[bar:%\d+]] = OpCompositeExtract %v4float [[outInPV]] 2
// CHECK-NEXT:                         OpStore %out_var_BAR [[bar]]

// Decompose InnerPerVertexOut and write out DsOut.s.ptSize (PointSize)
// CHECK-NEXT:       [[ptSize:%\d+]] = OpCompositeExtract %float [[outInPV]] 3
// CHECK-NEXT:                         OpStore %gl_PointSize_0 [[ptSize]]

// Write out clip5 (SV_ClipDistance5) at offset 1
// CHECK-NEXT: [[clip5:%\d+]] = OpLoad %v3float %param_var_clip5
// CHECK-NEXT:  [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance_0 %uint_1
// CHECK-NEXT:  [[val0:%\d+]] = OpCompositeExtract %float [[clip5]] 0
// CHECK-NEXT:                  OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:  [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance_0 %uint_2
// CHECK-NEXT:  [[val1:%\d+]] = OpCompositeExtract %float [[clip5]] 1
// CHECK-NEXT:                  OpStore [[ptr1]] [[val1]]
// CHECK-NEXT:  [[ptr2:%\d+]] = OpAccessChain %_ptr_Output_float %gl_ClipDistance_0 %uint_3
// CHECK-NEXT:  [[val2:%\d+]] = OpCompositeExtract %float [[clip5]] 2
// CHECK-NEXT:                  OpStore [[ptr2]] [[val2]]
}