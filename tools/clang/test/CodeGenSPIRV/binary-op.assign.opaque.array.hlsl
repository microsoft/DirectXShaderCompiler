// Run: %dxc -T ps_6_0 -E main

Texture2D    gTextures[1];
SamplerState gSamplers[2];

// Copy to static variable
// CHECK:      [[src:%\d+]] = OpAccessChain %_ptr_UniformConstant_type_2d_image %gTextures %int_0
// CHECK-NEXT: [[dst:%\d+]] = OpAccessChain %_ptr_Private_type_2d_image %sTextures %int_0
// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_2d_image [[src]]
// CHECK-NEXT:                OpStore [[dst]] [[val]]
static Texture2D sTextures[1] = gTextures;

struct Samplers {
    SamplerState samplers[2];
};

struct Resources {
    Texture2D textures[1];
    Samplers  samplers;
};

float4 doSample(Texture2D t, SamplerState s[2]);

float4 main() : SV_Target {
    Resources r;
    // Copy to struct field
// CHECK: [[r:%\d+]] = OpAccessChain %_ptr_Function__arr_type_2d_image_uint_1 %r %int_0
// CHECK: OpAccessChain %_ptr_UniformConstant_type_2d_image %gTextures %int_0
// CHECK: OpAccessChain %_ptr_Function_type_2d_image [[r]] %int_0
    r.textures          = gTextures;

// CHECK: [[r:%\d+]] = OpAccessChain %_ptr_Function__arr_type_sampler_uint_2 %r %int_1 %int_0
// CHECK: OpAccessChain %_ptr_UniformConstant_type_sampler %gSamplers %int_0
// CHECK: OpAccessChain %_ptr_Function_type_sampler [[r]] %int_0
// CHECK: OpAccessChain %_ptr_UniformConstant_type_sampler %gSamplers %int_1
// CHECK: OpAccessChain %_ptr_Function_type_sampler [[r]] %int_1
    r.samplers.samplers = gSamplers;

    // Copy to local variable
// CHECK: [[r:%\d+]] = OpAccessChain %_ptr_Function__arr_type_2d_image_uint_1 %r %int_0
// CHECK: OpAccessChain %_ptr_Function_type_2d_image [[r]] %int_0
// CHECK: OpAccessChain %_ptr_Function_type_2d_image %textures %int_0
    Texture2D    textures[1] = r.textures;
    SamplerState samplers[2];
// CHECK: [[r:%\d+]] = OpAccessChain %_ptr_Function__arr_type_sampler_uint_2 %r %int_1 %int_0
// CHECK: OpAccessChain %_ptr_Function_type_sampler [[r]] %int_0
// CHECK: OpAccessChain %_ptr_Function_type_sampler %samplers %int_0
// CHECK: OpAccessChain %_ptr_Function_type_sampler [[r]] %int_1
// CHECK: OpAccessChain %_ptr_Function_type_sampler %samplers %int_1
    samplers = r.samplers.samplers;

// Copy to function parameter
// CHECK: OpAccessChain %_ptr_Function_type_sampler %samplers %int_0
// CHECK: OpAccessChain %_ptr_Function_type_sampler %param_var_s %int_0
// CHECK: OpAccessChain %_ptr_Function_type_sampler %samplers %int_1
// CHECK: OpAccessChain %_ptr_Function_type_sampler %param_var_s %int_1
    return doSample(textures[0], samplers);
}

float4 doSample(Texture2D t, SamplerState s[2]) {
    return t.Sample(s[1], float2(0.1, 0.2));
}