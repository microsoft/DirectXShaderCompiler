// Run: %dxc -T gs_6_0 -E main

// CHECK: OpCapability ClipDistance
// CHECK: OpCapability CullDistance
// CHECK: OpCapability Geometry

struct GsPerVertexIn {
    float4 pos   : SV_Position;      // Builtin Position
    float3 clip2 : SV_ClipDistance2; // Builtin ClipDistance
    float2 clip0 : SV_ClipDistance0; // Builtin ClipDistance
    float3 foo   : FOO;              // Input variable

    [[vk::builtin("PointSize")]]
    float ptSize : PSIZE;            // Builtin PointSize
};

struct GsInnerOut {
    float4 pos   : SV_Position;      // Builtion Position
    float2 foo   : FOO;              // Output variable
    float2 cull3 : SV_CullDistance3; // Builtin CullDistance

    [[vk::builtin("PointSize")]]
    float ptSize : PSIZE;            // Builtin PointSize
};

struct GsPerVertexOut {
    GsInnerOut s;
    float  cull2 : SV_CullDistance2; // Builtin CullDistance
    float4 clip  : SV_ClipDistance;  // Builtin ClipDistance
    float4 bar   : BAR;              // Output variable
};

// Input  builtin : gl_PerVertex (Position, ClipDistance)
// Output builtin : Position, ClipDistance, CullDistance
// Input  variable: FOO, BAR
// Output variable: FOO, BAR

// CHECK: OpEntryPoint Geometry %main "main" %gl_PerVertexIn %gl_ClipDistance %gl_CullDistance %in_var_BAR %in_var_FOO %gl_Position %out_var_FOO %gl_PointSize %out_var_BAR

// CHECK: OpMemberDecorate %type_gl_PerVertex 0 BuiltIn Position
// CHECK: OpMemberDecorate %type_gl_PerVertex 1 BuiltIn PointSize
// CHECK: OpMemberDecorate %type_gl_PerVertex 2 BuiltIn ClipDistance
// CHECK: OpMemberDecorate %type_gl_PerVertex 3 BuiltIn CullDistance
// CHECK: OpDecorate %type_gl_PerVertex Block

// CHECK: OpDecorate %gl_ClipDistance BuiltIn ClipDistance
// CHECK: OpDecorate %gl_CullDistance BuiltIn CullDistance
// CHECK: OpDecorate %gl_Position BuiltIn Position
// CHECK: OpDecorate %gl_PointSize BuiltIn PointSize

// CHECK: OpDecorate %in_var_BAR Location 0
// CHECK: OpDecorate %in_var_FOO Location 1
// CHECK: OpDecorate %out_var_FOO Location 0
// CHECK: OpDecorate %out_var_BAR Location 1

// Input : clip0 + clip2 : 5 floats
// Input : no cull       : 1 floats (default)
// CHECK: %type_gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_5 %_arr_float_uint_1

// CHECK: %gl_PerVertexIn = OpVariable %_ptr_Input__arr_type_gl_PerVertex_uint_2 Input

// Input : clip          : 4 floats
// Input : cull2 + cull3 : 3 floats (default)
// CHECK: %gl_ClipDistance = OpVariable %_ptr_Output__arr_float_uint_4 Output
// CHECK: %gl_CullDistance = OpVariable %_ptr_Output__arr_float_uint_3 Output

// CHECK: %in_var_BAR = OpVariable %_ptr_Input__arr_v2float_uint_2 Input
// CHECK: %in_var_FOO = OpVariable %_ptr_Input__arr_v3float_uint_2 Input
// CHECK: %gl_Position = OpVariable %_ptr_Output_v4float Output
// CHECK: %out_var_FOO = OpVariable %_ptr_Output_v2float Output
// CHECK: %gl_PointSize = OpVariable %_ptr_Output_float Output
// CHECK: %out_var_BAR = OpVariable %_ptr_Output_v4float Output

[maxvertexcount(2)]
void main(in    line float2                     bar   [2] : BAR,
          in    line GsPerVertexIn              inData[2],
          inout      LineStream<GsPerVertexOut> outData)
{
// Layout of input ClipDistance array:
//   clip0: 2 floats, offset 0
//   clip2: 3 floats, offset 2

// Layout of output ClipDistance array:
//   clip : 4 floats, offset 0

// Layout of output CullDistance array:
//   cull2: 1 floats, offset 0
//   cull3: 2 floats, offset 1

    GsPerVertexOut vertex;

    vertex = (GsPerVertexOut)0;

    outData.Append(vertex);

    outData.RestartStrip();
// CHECK:      [[bar:%\d+]] = OpLoad %_arr_v2float_uint_2 %in_var_BAR
// CHECK-NEXT:                OpStore %param_var_bar [[bar]]

// Compose an array for GsPerVertexIn::pos
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_v4float %gl_PerVertexIn %uint_0 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %v4float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_v4float %gl_PerVertexIn %uint_1 %uint_0
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %v4float [[ptr1]]
// CHECK-NEXT:   [[inPosArr:%\d+]] = OpCompositeConstruct %_arr_v4float_uint_2 [[val0]] [[val1]]

// Compose an array for GsPerVertexIn::clip2
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_2
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_3
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_4
// CHECK-NEXT:       [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:     [[clip20:%\d+]] = OpCompositeConstruct %v3float [[val0]] [[val1]] [[val2]]
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_2
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_3
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:       [[ptr2:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_4
// CHECK-NEXT:       [[val2:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:     [[clip21:%\d+]] = OpCompositeConstruct %v3float [[val0]] [[val1]] [[val2]]
// CHECK-NEXT: [[inClip2Arr:%\d+]] = OpCompositeConstruct %_arr_v3float_uint_2 [[clip20]] [[clip21]]

// Compose an array for GsPerVertexIn::clip0
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_2 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:     [[clip00:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]
// CHECK-NEXT:       [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_0
// CHECK-NEXT:       [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:       [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_2 %uint_1
// CHECK-NEXT:       [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT:     [[clip01:%\d+]] = OpCompositeConstruct %v2float [[val0]] [[val1]]
// CHECK-NEXT: [[inClip0Arr:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_2 [[clip00]] [[clip01]]

// CHECK-NEXT:   [[inFooArr:%\d+]] = OpLoad %_arr_v3float_uint_2 %in_var_FOO

// Compose an array for GsPerVertexIn::ptSize
// CHECK-NEXT:      [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_0 %uint_1
// CHECK-NEXT:      [[val0:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:      [[ptr1:%\d+]] = OpAccessChain %_ptr_Input_float %gl_PerVertexIn %uint_1 %uint_1
// CHECK-NEXT:      [[val1:%\d+]] = OpLoad %float [[ptr1]]
// CHECK-NEXT: [[inPtSzArr:%\d+]] = OpCompositeConstruct %_arr_float_uint_2 [[val0]] [[val1]]

// CHECK-NEXT:      [[val0:%\d+]] = OpCompositeExtract %v4float [[inPosArr]] 0
// CHECK-NEXT:      [[val1:%\d+]] = OpCompositeExtract %v3float [[inClip2Arr]] 0
// CHECK-NEXT:      [[val2:%\d+]] = OpCompositeExtract %v2float [[inClip0Arr]] 0
// CHECK-NEXT:      [[val3:%\d+]] = OpCompositeExtract %v3float [[inFooArr]] 0
// CHECK-NEXT:      [[val4:%\d+]] = OpCompositeExtract %float [[inPtSzArr]] 0
// CHECK-NEXT:   [[inData0:%\d+]] = OpCompositeConstruct %GsPerVertexIn [[val0]] [[val1]] [[val2]] [[val3]] [[val4]]
// CHECK-NEXT:      [[val0:%\d+]] = OpCompositeExtract %v4float [[inPosArr]] 1
// CHECK-NEXT:      [[val1:%\d+]] = OpCompositeExtract %v3float [[inClip2Arr]] 1
// CHECK-NEXT:      [[val2:%\d+]] = OpCompositeExtract %v2float [[inClip0Arr]] 1
// CHECK-NEXT:      [[val3:%\d+]] = OpCompositeExtract %v3float [[inFooArr]] 1
// CHECK-NEXT:      [[val4:%\d+]] = OpCompositeExtract %float [[inPtSzArr]] 1
// CHECK-NEXT:   [[inData1:%\d+]] = OpCompositeConstruct %GsPerVertexIn [[val0]] [[val1]] [[val2]] [[val3]] [[val4]]

// CHECK-NEXT:    [[inData:%\d+]] = OpCompositeConstruct %_arr_GsPerVertexIn_uint_2 [[inData0]] [[inData1]]
// CHECK-NEXT:                      OpStore %param_var_inData [[inData]]

// CHECK-NEXT:           {{%\d+}} = OpFunctionCall %void %src_main %param_var_bar %param_var_inData %param_var_outData

// No write back after the call
// CHECK-NEXT:                      OpReturn
}
