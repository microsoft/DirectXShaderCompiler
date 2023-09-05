// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID {
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};


struct PSInput
{
    int i : COLOR0;
    nointerpolation float3 Color : COLOR1;
};

struct testStructure
{
    nointerpolation float3 Color : COLOR4;
};


float3 func1(nointerpolation float3 func1Color){
  testStructure chainEndVal;
  chainEndVal.Color = func1Color;
  float3 func1Ret = GetAttributeAtVertex(func1Color, 1) + chainEndVal.Color;
  return func1Ret;
}

float3 func0(nointerpolation float3 func0Color){
  return func1(func0Color);
}

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color:%\w+]] Location 0
// CHECK: OpDecorate [[color]] PerVertexKHR
// CHECK: [[param_var_inputColor:%\w+]] = OpVariable %_ptr_Function__arr_v3float_uint_3 Function
// CHECK: [[inst26:%\d+]] = OpLoad %_arr_v3float_uint_3 [[color]]
// CHECK: OpStore [[param_var_inputColor]] [[inst26]]
// CHECK: [[callMain:%\d+]] = OpFunctionCall %v3float %src_main [[param_var_inputColor]]
float3 main(nointerpolation float3 inputColor : COLOR) : SV_Target
{
    float3 mainRet = func0(inputColor);
    return mainRet;
}

// CHECK: [[inputColor:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3float_uint_3
// CHECK: %inputColor_perVertexParam0 = OpVariable %_ptr_Function_v3float Function
// CHECK: [[inst39:%\d+]] = OpAccessChain %_ptr_Function_v3float [[inputColor]] %uint_0
// CHECK: [[inst40:%\d+]] = OpLoad %v3float [[inst39]]
// CHECK: OpStore %inputColor_perVertexParam0 [[inst40]]
// CHECK: [[inst41:%\d+]] = OpLoad %_arr_v3float_uint_3 [[inputColor]]
// CHECK: OpStore [[param_var_func0Color:%\w+]] [[inst41]]
// CHECK: [[inst43:%\d+]] = OpFunctionCall %v3float [[func0:%\w+]] [[param_var_func0Color]]
// CHECK: [[chainEndVal:%\w+]] = OpVariable %_ptr_Function_testStructure Function
// CHECK: [[func1Ret:%\w+]] = OpVariable %_ptr_Function_v3float Function
// CHECK: %func1Color_perVertexParam0 = OpVariable %_ptr_Function_v3float Function
// CHECK: [[inst66:%\d+]] = OpAccessChain %_ptr_Function_v3float [[func1Color:%\w+]] %uint_0
// CHECK: [[inst67:%\d+]] = OpLoad %v3float [[inst66]]
// CHECK: OpStore %func1Color_perVertexParam0 [[inst67]]
// CHECK: [[inst68:%\d+]] = OpLoad %v3float %func1Color_perVertexParam0
// CHECK: [[inst69:%\d+]] = OpAccessChain %_ptr_Function_v3float [[chainEndVal]] %int_0 %uint_0
// CHECK: OpStore [[inst69]] [[inst68]]