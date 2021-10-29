// Run: %dxc -T ps_6_0 -E main -fcgl -Vd -spirv

[[vk::ext_decorate(1, 0)]]
bool b0;

[[vk::ext_decorate(30, 23)]]
float4 main(

//CHECK: OpDecorate {{%\w+}} SpecId 0
//CHECK: OpDecorate {{%\w+}} BuiltIn BaryCoordNoPerspAMD
//CHECK: OpDecorate {{%\w+}} BuiltIn BaryCoordNoPerspCentroidAMD
//CHECK: OpDecorate {{%\w+}} BuiltIn BaryCoordNoPerspSampleAMD
//CHECK: OpDecorate {{%\w+}} BuiltIn BaryCoordSmoothAMD
//CHECK: OpDecorate {{%\w+}} BuiltIn BaryCoordSmoothCentroidAMD
//CHECK: OpDecorate {{%\w+}} BuiltIn BaryCoordSmoothSampleAMD
//CHECK: OpDecorate {{%\w+}} BuiltIn BaryCoordPullModelAMD
//CHECK: OpDecorate {{%\w+}} ExplicitInterpAMD
//CHECK: OpDecorate {{%\w+}} Location 23
//CHECK: OpDecorateString {{%\w+}} UserSemantic "return variable"
//CHECK: OpDecorate {{%\w+}} FPRoundingMode RTE
    
    // spv::Decoration::builtIn = 11
    [[vk::ext_decorate(11, 4992)]] float2 b0 : COLOR0,
    [[vk::ext_decorate(11, 4993)]] float2 b1 : COLOR1,
    [[vk::ext_decorate(11, 4994)]] float2 b2 : COLOR2,
    [[vk::ext_decorate(11, 4995)]] float2 b3 : COLOR3,
    [[vk::ext_decorate(11, 4996)]] float2 b4 : COLOR4,
    [[vk::ext_decorate(11, 4997)]] float2 b5 : COLOR5,
    [[vk::ext_decorate(11, 4998)]] float2 b6 : COLOR6,
    // ExplicitInterpAMD
    [[vk::location(12), vk::ext_decorate(4999)]] float2 b7 : COLOR7
            ) : SV_Target {
    
    // spv::Decoration::FPRoundingMode = 39, RTZ=0
    [[vk::ext_decorate(39, 0), vk::ext_decorate_string(5635, "return variable")]] float4 ret = 1.0;
    ret.xy = b0 * b1 + b2 + b3 + b4;
    ret.zw = b5 + b6 + b7;
    return ret;
}
