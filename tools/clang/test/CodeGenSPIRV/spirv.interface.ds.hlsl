// Run: %dxc -T ds_6_0 -E main

// CHECK: OpCapability ClipDistance
// CHECK: OpCapability CullDistance
// CHECK: OpCapability Tessellation

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

// Per-vertex    input  builtin : gl_PerVertex (Position, ClipDistance, CullDistance)
// Per-vertex    output builtin : gl_PerVertex (Position, ClipDistance, CullDistance)
// Per-vertex    input  variable: TEXCOORD, BAR
// Per-vertex    output variable: FOO, BAR

// Per-primitive input builtin  : TessLevelInner, TessLevelOuter, TessCoord (SV_DomainLocation)
// Per-primitive input variable : FOO

// CHECK: OpEntryPoint TessellationEvaluation %main "main" %gl_PerVertexIn %gl_PerVertexOut %in_var_TEXCOORD %in_var_BAR %gl_TessCoord %gl_TessLevelOuter %gl_TessLevelInner %in_var_FOO %out_var_FOO %out_var_BAR

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

// CHECK: OpDecorate %gl_TessCoord BuiltIn TessCoord
// CHECK: OpDecorate %gl_TessCoord Patch
// CHECK: OpDecorate %gl_TessLevelOuter BuiltIn TessLevelOuter
// CHECK: OpDecorate %gl_TessLevelOuter Patch
// CHECK: OpDecorate %gl_TessLevelInner BuiltIn TessLevelInner
// CHECK: OpDecorate %gl_TessLevelInner Patch
// CHECK: OpDecorate %in_var_FOO Patch
// CHECK: OpDecorate %in_var_BAR Location 0
// CHECK: OpDecorate %in_var_FOO Location 1
// CHECK: OpDecorate %in_var_TEXCOORD Location 2
// CHECK: OpDecorate %out_var_FOO Location 0
// CHECK: OpDecorate %out_var_BAR Location 1

// Input : clip0 + clip3 : 2 floats
// Input : cull2 + cull3 : 6 floats
// CHECK: %type_gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_2 %_arr_float_uint_6

// Output: clip0 + clip5 : 4 floats
// Output: cull3 + cull4 : 4 floats
// CHECK: %type_gl_PerVertex_0 = OpTypeStruct %v4float %float %_arr_float_uint_4 %_arr_float_uint_4

// CHECK:    %gl_PerVertexIn = OpVariable %_ptr_Input__arr_type_gl_PerVertex_uint_3 Input
// CHECK:   %gl_PerVertexOut = OpVariable %_ptr_Output_type_gl_PerVertex_0 Output
// CHECK:   %in_var_TEXCOORD = OpVariable %_ptr_Input__arr_v4float_uint_3 Input
// CHECK:        %in_var_BAR = OpVariable %_ptr_Input__arr_v2float_uint_3 Input
// CHECK:      %gl_TessCoord = OpVariable %_ptr_Input_v3float Input
// CHECK: %gl_TessLevelOuter = OpVariable %_ptr_Input__arr_float_uint_4 Input
// CHECK: %gl_TessLevelInner = OpVariable %_ptr_Input__arr_float_uint_2 Input
// CHECK:        %in_var_FOO = OpVariable %_ptr_Input_v3float Input
// CHECK:       %out_var_FOO = OpVariable %_ptr_Output_v3float Output
// CHECK:       %out_var_BAR = OpVariable %_ptr_Output_v4float Output

[domain("quad")]
DsOut main(    const OutputPatch<PerVertexIn, 3> patch,
               float2   loc     : SV_DomainLocation,
               HsPcfOut pcfData,
           out float3   clip5   : SV_ClipDistance5)     // Builtin ClipDistance
{
  DsOut dsOut;
  dsOut = (DsOut)0;
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

// Read gl_PerVertex[0].gl_ClipDistance and compose patch[0].cull3 (SV_CullDistance3)
// CHECK:              [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_4
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:         [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_5
// CHECK-NEXT:         [[val3:%\d+]] = OpLoad %float [[ptr3]]
// CHECK-NEXT:  [[patch0cull3:%\d+]] = OpCompositeConstruct %v4float [[val0]] [[val1]] [[val2]] [[val3]]

// Read gl_PerVertex[1].gl_ClipDistance and compose patch[1].cull3 (SV_CullDistance3)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_4
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:         [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_5
// CHECK-NEXT:         [[val3:%\d+]] = OpLoad %float [[ptr3]]
// CHECK-NEXT:  [[patch1cull3:%\d+]] = OpCompositeConstruct %v4float [[val0]] [[val1]] [[val2]] [[val3]]

// Read gl_PerVertex[2].gl_ClipDistance and compose patch[2].cull3 (SV_CullDistance3)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_3 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_3 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_3 %uint_4
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:         [[ptr3:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_3 %uint_5
// CHECK-NEXT:         [[val3:%\d+]] = OpLoad %float [[ptr3]]
// CHECK-NEXT:  [[patch2cull3:%\d+]] = OpCompositeConstruct %v4float [[val0]] [[val1]] [[val2]] [[val3]]

// Compose an array of input SV_CullDistance3 for later use
// CHECK-NEXT:   [[inCull3Arr:%\d+]] = OpCompositeConstruct %_arr_v4float_uint_3 [[patch0cull3]] [[patch1cull3]] [[patch2cull3]]

// Read gl_PerVertex[0].gl_Position as patch[0].s.pos (SV_Position)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_v4float %gl_PerVertexIn %uint_0 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %v4float [[ptr0]]

// Read gl_PerVertex[1].gl_Position as patch[1].s.pos (SV_Position)
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_v4float %gl_PerVertexIn %uint_1 %uint_0
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %v4float [[ptr1]]

// Read gl_PerVertex[2].gl_Position as patch[2].s.pos (SV_Position)
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_v4float %gl_PerVertexIn %uint_2 %uint_0
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %v4float [[ptr2]]

// Compose an array of input SV_Position for later use
// CHECK-NEXT:     [[inPosArr:%\d+]] = OpCompositeConstruct %_arr_v4float_uint_3 [[val0]] [[val1]] [[val2]]

// Read gl_PerVertex[0].gl_ClipDistance as patch[0].s.clip0 (SV_ClipDistance0)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]

// Read gl_PerVertex[1].gl_ClipDistance as patch[1].s.clip0 (SV_ClipDistance0)
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_0
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]

// Read gl_PerVertex[2].gl_ClipDistance as patch[2].s.clip0 (SV_ClipDistance0)
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_2 %uint_0
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]

// Compose an array of input SV_ClipDistance0 for later use
// CHECK-NEXT:   [[inClip0Arr:%\d+]] = OpCompositeConstruct %_arr_float_uint_3 [[val0]] [[val1]] [[val2]]

// Read gl_PerVertex[0].gl_ClipDistance as patch[0].s.s.clip3 (SV_ClipDistance3)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_1
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]

// Read gl_PerVertex[1].gl_ClipDistance as patch[1].s.s.clip3 (SV_ClipDistance3)
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]

// Read gl_PerVertex[2].gl_ClipDistance as patch[2].s.s.clip3 (SV_ClipDistance3)
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_2 %uint_1
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

// Read gl_PerVertex[0].gl_CullDistance as patch[0].s.cull2 (SV_CullDistance2)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_3 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:  [[patch0cull2:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// Read gl_PerVertex[1].gl_CullDistance as patch[1].s.cull2 (SV_CullDistance2)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_3 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:  [[patch1cull2:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]

// Read gl_PerVertex[2].gl_CullDistance as patch[2].s.cull2 (SV_CullDistance2)
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_3 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_3 %uint_1
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

// Compose an array of input PointSize for later use
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_1
// CHECK-NEXT:         [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:         [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_2 %uint_1
// CHECK-NEXT:         [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:  [[inPtSizeArr:%\d+]] = OpCompositeConstruct %_arr_float_uint_3 [[val0]] [[val1]] [[val2]]

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
// CHECK-NEXT:          [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v4float %gl_PerVertexOut %uint_0
// CHECK-NEXT:                         OpStore [[ptr]] [[outPos]]

// CHECK-NEXT:      [[outInPV:%\d+]] = OpCompositeExtract %InnerPerVertexOut [[ret]] 1
// CHECK-NEXT:     [[outIn2PV:%\d+]] = OpCompositeExtract %Inner2PerVertexOut [[outInPV]] 0

// Decompose Inner2PerVertexOut and write out DsOut.s.s.foo (FOO)
// CHECK-NEXT:          [[foo:%\d+]] = OpCompositeExtract %v3float [[outIn2PV]] 0
// CHECK-NEXT:                         OpStore %out_var_FOO [[foo]]

// Decompose Inner2PerVertexOut and write out DsOut.s.s.cull3 (SV_CullDistance3) at offset 0
// CHECK-NEXT:        [[cull3:%\d+]] = OpCompositeExtract %v2float [[outIn2PV]] 1
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_3 %uint_0
// CHECK-NEXT:         [[val0:%\d+]] = OpCompositeExtract %float [[cull3]] 0
// CHECK-NEXT:                         OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_3 %uint_1
// CHECK-NEXT:         [[val1:%\d+]] = OpCompositeExtract %float [[cull3]] 1
// CHECK-NEXT:                         OpStore [[ptr1]] [[val1]]

// Decompose Inner2PerVertexOut and write out DsOut.s.s.clip0 (SV_ClipDistance0) at offset 0
// CHECK-NEXT:        [[clip0:%\d+]] = OpCompositeExtract %float [[outIn2PV]] 2
// CHECK-NEXT:          [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_0
// CHECK-NEXT:                         OpStore [[ptr]] [[clip0]]

// Decompose InnerPerVertexOut and write out DsOut.s.cull4 (SV_CullDistance4) at offset 2
// CHECK-NEXT:        [[cull4:%\d+]] = OpCompositeExtract %v2float [[outInPV]] 1
// CHECK-NEXT:         [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_3 %uint_2
// CHECK-NEXT:         [[val0:%\d+]] = OpCompositeExtract %float [[cull4]] 0
// CHECK-NEXT:                         OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:         [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_3 %uint_3
// CHECK-NEXT:         [[val1:%\d+]] = OpCompositeExtract %float [[cull4]] 1
// CHECK-NEXT:                         OpStore [[ptr1]] [[val1]]

// Decompose InnerPerVertexOut and write out DsOut.s.bar (BAR)
// CHECK-NEXT:          [[bar:%\d+]] = OpCompositeExtract %v4float [[outInPV]] 2
// CHECK-NEXT:                         OpStore %out_var_BAR [[bar]]

// Decompose InnerPerVertexOut and write out DsOut.s.ptSize (PointSize)
// CHECK-NEXT:       [[ptSize:%\d+]] = OpCompositeExtract %float [[outInPV]] 3
// CHECK-NEXT:          [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_1
// CHECK-NEXT:                         OpStore [[ptr]] [[ptSize]]

// Write out clip5 (SV_ClipDistance5) at offset 1
// CHECK-NEXT: [[clip5:%\d+]] = OpLoad %v3float %param_var_clip5
// CHECK-NEXT:  [[ptr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_1
// CHECK-NEXT:  [[val0:%\d+]] = OpCompositeExtract %float [[clip5]] 0
// CHECK-NEXT:                  OpStore [[ptr0]] [[val0]]
// CHECK-NEXT:  [[ptr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_2
// CHECK-NEXT:  [[val1:%\d+]] = OpCompositeExtract %float [[clip5]] 1
// CHECK-NEXT:                  OpStore [[ptr1]] [[val1]]
// CHECK-NEXT:  [[ptr2:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_3
// CHECK-NEXT:  [[val2:%\d+]] = OpCompositeExtract %float [[clip5]] 2
// CHECK-NEXT:                  OpStore [[ptr2]] [[val2]]
}