// Run: %dxc -T vs_6_0 -E main

// CHECK: OpCapability ClipDistance
// CHECK: OpCapability CullDistance

// CHECK: OpEntryPoint Vertex %main "main" %gl_PerVertexOut %in_var_TEXCOORD %in_var_SV_Position %in_var_SV_ClipDistance %in_var_SV_CullDistance0 %out_var_COLOR %out_var_TEXCOORD

// CHECK: OpMemberDecorate %type_gl_PerVertex 0 BuiltIn Position
// CHECK: OpMemberDecorate %type_gl_PerVertex 1 BuiltIn PointSize
// CHECK: OpMemberDecorate %type_gl_PerVertex 2 BuiltIn ClipDistance
// CHECK: OpMemberDecorate %type_gl_PerVertex 3 BuiltIn CullDistance
// CHECK: OpDecorate %type_gl_PerVertex Block

// CHECK: OpDecorate %in_var_TEXCOORD Location 0
// CHECK: OpDecorate %in_var_SV_Position Location 1
// CHECK: OpDecorate %in_var_SV_ClipDistance Location 2
// CHECK: OpDecorate %in_var_SV_CullDistance0 Location 3
// CHECK: OpDecorate %out_var_COLOR Location 0
// CHECK: OpDecorate %out_var_TEXCOORD Location 1

//     clipdis0 + clipdis1            : 5 floats
//     culldis3 + culldis5 + culldis6 : 3 floats
// CHECK: %type_gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_5 %_arr_float_uint_3

// CHECK: %gl_PerVertexOut = OpVariable %_ptr_Output_type_gl_PerVertex Output

// CHECK: %in_var_TEXCOORD = OpVariable %_ptr_Input_v4float Input
// CHECK: %in_var_SV_Position = OpVariable %_ptr_Input_v4float Input
// CHECK: %in_var_SV_ClipDistance = OpVariable %_ptr_Input_v2float Input
// CHECK: %in_var_SV_CullDistance0 = OpVariable %_ptr_Input_v3float Input
// CHECK: %out_var_COLOR = OpVariable %_ptr_Output_v4float Output
// CHECK: %out_var_TEXCOORD = OpVariable %_ptr_Output_v4float Output

struct InnerInnerStruct {
  float4           position : SV_Position;      // -> BuiltIn Position in gl_Pervertex
};

struct InnerStruct {
  float2           clipdis1 : SV_ClipDistance1; // -> BuiltIn ClipDistance in gl_PerVertex
  InnerInnerStruct s;
};

struct VSOut {
  float4           color    : COLOR;            // -> Output variable
  InnerStruct s;
};

[[vk::builtin("PointSize")]]
float main(out VSOut  vsOut,
           out   float3 clipdis0 : SV_ClipDistance0, // -> BuiltIn ClipDistance in gl_PerVertex
           inout float4 coord    : TEXCOORD,         // -> Input & output variable
           out   float  culldis5 : SV_CullDistance5, // -> BuiltIn CullDistance in gl_PerVertex
           out   float  culldis3 : SV_CullDistance3, // -> BuiltIn CullDistance in gl_PerVertex
           out   float  culldis6 : SV_CullDistance6, // -> BuiltIn CullDistance in gl_PerVertex
           in    float4 inPos    : SV_Position,      // -> Input variable
           in    float2 inClip   : SV_ClipDistance,  // -> Input variable
           in    float3 inCull   : SV_CullDistance0  // -> Input variable
         ) : PSize {                                 // -> Builtin PointSize
    vsOut    = (VSOut)0;
    clipdis0 = 1.;
    coord    = 2.;
    culldis5 = 3.;
    culldis3 = 4.;
    culldis6 = 5.;
    inPos    = 6.;

    return 7.;

// Layout of ClipDistance array:
//   clipdis0: 3 floats, offset 0
//   clipdis1: 2 floats, offset 3

// Layout of CullDistance array:
//   culldis3: 1 floats, offset 0
//   culldis5: 1 floats, offset 1
//   culldis6: 1 floats, offset 2

// CHECK:      [[texcoord:%\d+]] = OpLoad %v4float %in_var_TEXCOORD
// CHECK-NEXT:                     OpStore %param_var_coord [[texcoord]]
// CHECK-NEXT:      [[pos:%\d+]] = OpLoad %v4float %in_var_SV_Position
// CHECK-NEXT:                     OpStore %param_var_inPos [[pos]]
// CHECK-NEXT:   [[inClip:%\d+]] = OpLoad %v2float %in_var_SV_ClipDistance
// CHECK-NEXT:                     OpStore %param_var_inClip [[inClip]]
// CHECK-NEXT:   [[inCull:%\d+]] = OpLoad %v3float %in_var_SV_CullDistance0
// CHECK-NEXT:                     OpStore %param_var_inCull [[inCull]]

// CHECK-NEXT:   [[ptSize:%\d+]] = OpFunctionCall %float %src_main

// CHECK-NEXT:      [[ptr:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_1
// CHECK-NEXT:                     OpStore [[ptr]] [[ptSize]]

// Write out COLOR
// CHECK-NEXT:    [[vsOut:%\d+]] = OpLoad %VSOut %param_var_vsOut
// CHECK-NEXT: [[outColor:%\d+]] = OpCompositeExtract %v4float [[vsOut]] 0
// CHECK-NEXT:                     OpStore %out_var_COLOR [[outColor]]

// CHECK-NEXT:   [[innerS:%\d+]] = OpCompositeExtract %InnerStruct [[vsOut]] 1

// Write out SV_ClipDistance1
// CHECK-NEXT:    [[clip1:%\d+]] = OpCompositeExtract %v2float [[innerS]] 0
// CHECK-NEXT: [[clipArr3:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_3
// CHECK-NEXT:   [[clip10:%\d+]] = OpCompositeExtract %float [[clip1]] 0
// CHECK-NEXT:                     OpStore [[clipArr3]] [[clip10]]
// CHECK-NEXT: [[clipArr4:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_4
// CHECK-NEXT:   [[clip11:%\d+]] = OpCompositeExtract %float [[clip1]] 1
// CHECK-NEXT:                     OpStore [[clipArr4]] [[clip11]]

// CHECK-NEXT: [[inner2S:%\d+]] = OpCompositeExtract %InnerInnerStruct [[innerS]] 1

// Write out SV_Position
// CHECK-NEXT:     [[pos:%\d+]] = OpCompositeExtract %v4float [[inner2S]] 0
// CHECK-NEXT:  [[outPos:%\d+]] = OpAccessChain %_ptr_Output_v4float %gl_PerVertexOut %uint_0
// CHECK-NEXT:       OpStore [[outPos]] [[pos]]

// Write out SV_ClipDistance0
// CHECK-NEXT:    [[clip0:%\d+]] = OpLoad %v3float %param_var_clipdis0
// CHECK-NEXT: [[clipArr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_0
// CHECK-NEXT:   [[clip00:%\d+]] = OpCompositeExtract %float [[clip0]] 0
// CHECK-NEXT:                     OpStore [[clipArr0]] [[clip00]]
// CHECK-NEXT: [[clipArr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_1
// CHECK-NEXT:   [[clip01:%\d+]] = OpCompositeExtract %float [[clip0]] 1
// CHECK-NEXT:                     OpStore [[clipArr1]] [[clip01]]
// CHECK-NEXT: [[clipArr2:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_2 %uint_2
// CHECK-NEXT:   [[clip02:%\d+]] = OpCompositeExtract %float [[clip0]] 2
// CHECK-NEXT:                     OpStore [[clipArr2]] [[clip02]]

// Write out TEXCOORD
// CHECK-NEXT:  [[texcord:%\d+]] = OpLoad %v4float %param_var_coord
// CHECK-NEXT:                     OpStore %out_var_TEXCOORD [[texcord]]

// Write out SV_CullDistance5
// CHECK-NEXT:    [[cull5:%\d+]] = OpLoad %float %param_var_culldis5
// CHECK-NEXT: [[cullArr1:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_3 %uint_1
// CHECK-NEXT:                     OpStore [[cullArr1]] [[cull5]]

// Write out SV_CullDistance3
// CHECK-NEXT:    [[cull3:%\d+]] = OpLoad %float %param_var_culldis3
// CHECK-NEXT: [[cullArr0:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_3 %uint_0
// CHECK-NEXT:                     OpStore [[cullArr0]] [[cull3]]

// Write out SV_CullDistance6
// CHECK-NEXT:    [[cull6:%\d+]] = OpLoad %float %param_var_culldis6
// CHECK-NEXT: [[cullArr2:%\d+]] = OpAccessChain %_ptr_Output_float %gl_PerVertexOut %uint_3 %uint_2
// CHECK-NEXT:                     OpStore [[cullArr2]] [[cull6]]
}
