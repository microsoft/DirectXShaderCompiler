// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID {
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};


float3 func1(nointerpolation float3 func1Color){
  float3 func1Ret = GetAttributeAtVertex(func1Color, 1) + func1Color;
  return func1Ret;
}

float3 func0(nointerpolation float3 func0Color){
  return func1(func0Color);
}

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[icolor0:%\w+]] Location 0
// CHECK: OpDecorate [[icolor0]] PerVertexKHR
// CHECK: [[icolor0]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
float3 main(nointerpolation float3 inputColor : COLOR) : SV_Target
{
    float3 mainRet = func0(inputColor);
    return mainRet;
}
// CHECK: [[inputColor:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3float_uint_3
// CHECK: [[param_var_func0Color:%\w+]] = OpVariable %_ptr_Function__arr_v3float_uint_3 Function
// CHECK: [[inputColor_perVertexParam0:%\w+]] = OpVariable %_ptr_Function_v3float Function
// CHECK: [[inst39:%\d+]] = OpAccessChain %_ptr_Function_v3float [[inputColor]] %uint_0
// CHECK: [[inst40:%\d+]] = OpLoad %v3float [[inst39]]
// CHECK: OpStore [[inputColor_perVertexParam0]] [[inst40]]
// CHECK: [[inst41:%\d+]] = OpLoad %_arr_v3float_uint_3 [[inputColor]]
// CHECK: OpStore [[param_var_func0Color]] [[inst41]]
// CHECK: [[inst43:%\d+]] = OpFunctionCall %v3float [[func0:%\w+]] [[param_var_func0Color]]